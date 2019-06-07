#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include <csignal>
#include <list>

#include "rdt_header.h"

#define HEADER_SIZE 12
#define BACKLOG 1 /* pending connections queue size */
#define MAX_SEQ_NUM 25600
#define MAX_BUF_SIZE 512

int connectionOrder = 1;
bool connected = false;
uint32_t expectedSeqNum;
uint32_t serverSeqNum;

std::list<packet> buffer;

bool handleConnection(packet& clientPkt, int server_sockfd, sockaddr_in& their_addr, unsigned int sin_size) {
    // choose init seq num, send SYNACK msg, acking SYN from client
    packet serverPkt;
    serverPkt.hd.flags = ((1 << 15) | (1 << 14)); // set ACKbit = 1 and SYNbit = 1
    srand(1);
    serverSeqNum = rand() % (MAX_SEQ_NUM + 1);
    serverPkt.hd.seqNum = serverSeqNum;
    serverPkt.hd.ackNum = (clientPkt.hd.seqNum + 1) % (MAX_SEQ_NUM + 1);
    memset(serverPkt.data, '\0', sizeof(serverPkt.data));
    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send SYNACK packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, 0, 0, true);
    // printf("send SYNACK packet to: %s\n\n", inet_ntoa(their_addr.sin_addr));
    serverSeqNum++;

    // receive ACK packet from client, finish 3-way handshake
    if(recvfrom(server_sockfd, &clientPkt, sizeof(clientPkt), 0, (struct sockaddr*) &their_addr, &sin_size) < 0) {
        std::cerr << "ERROR: receive ACK from client" << std::endl;
        return false;
    }
    printPacketInfo(clientPkt, 0, 0, false);
    // printf("receive ACK packet from: %s\n\n", inet_ntoa(their_addr.sin_addr));
    
    if(clientPkt.hd.ackNum == serverSeqNum) {
        expectedSeqNum = clientPkt.hd.seqNum;
        // printf("establish connection with %s:\n\n", inet_ntoa(their_addr.sin_addr));
        return true;
    } else {
        std::cerr << "ACKnum" << std::endl;
        return false;
    }
}

bool closeConnection(packet& clientPkt, int server_sockfd, sockaddr_in &their_addr, unsigned int sin_size) {
    packet serverPkt;
    memset(serverPkt.data, '\0', sizeof(serverPkt.data));

    serverPkt.hd.flags = (1 << 15); // set ACKbit = 1
    serverPkt.hd.seqNum = serverSeqNum;
    serverPkt.hd.ackNum = (clientPkt.hd.seqNum + 1) % (MAX_SEQ_NUM + 1);

    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send ACK packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, 0, 0, true);

    // std::cout << "send ACK packet to: " << inet_ntoa(their_addr.sin_addr) << "\n\n";

    // send FIN to client
    bzero(&serverPkt, sizeof(serverPkt));
    serverPkt.hd.flags = (1 << 13); // set FINbit = 1
    serverPkt.hd.seqNum = serverSeqNum;
    serverPkt.hd.ackNum = 0;

    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send FIN packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, 0, 0, true);

    // std::cout << "send FIN packet to: " << inet_ntoa(their_addr.sin_addr) << "\n\n";

    // receive ACK packet from client, close connection
    if(recvfrom(server_sockfd, &clientPkt, sizeof(clientPkt), 0, (struct sockaddr*) &their_addr, &sin_size) < 0) {
        std::cerr << "ERROR: receive ACK from client" << std::endl;
        return false;
    }
    printPacketInfo(clientPkt, 0, 0, false);
    // std::cout << "receive ACK from: " << inet_ntoa(their_addr.sin_addr) << "\n\n";

    connectionOrder++;
    connected = false;
    buffer.clear();
    return true;
}

void signalHandler(int signal) {
    // std::cout << "interrupted: " << signal << std::endl;
    if(connected) {
        std::string filename = std::to_string(connectionOrder) + ".file";
        std::ifstream is(filename, std::ios::in | std::ios::binary);
        if(is) {        // the client already sends some data
            is.close();
            std::ofstream os(filename, std::ios::out | std::ios::binary);
            os << "INTERRUPT";
            os.close();
        }
    }
    exit(0);
}

void receiveData(packet& clientPkt, int server_sockfd, sockaddr_in &their_addr, unsigned int sin_size) {
    // std::cout << "receive packet from " << inet_ntoa(their_addr.sin_addr) << "\n\n";
    packet serverPkt;

    serverPkt.hd.flags = (1 << 15); // set ACKbit = 1
    serverPkt.hd.seqNum = serverSeqNum;

    if(clientPkt.hd.seqNum == expectedSeqNum) {
        int dataBytes = clientPkt.hd.dataSize;
        expectedSeqNum = (expectedSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);

        // receive the data in client's packet
        if(dataBytes > 0) {
            // std::cout << "receive " << dataBytes << " Bytes data\n\n";
            std::string filename = std::to_string(connectionOrder) + ".file";
            std::ofstream os(filename, std::ios::out | std::ios::binary | std::ios::app);
            os.write(clientPkt.data, dataBytes);
            os.close();
        }

        // check whether packets in buffer should be written to the file
        while(!buffer.empty() && buffer.front().hd.seqNum == expectedSeqNum) {
            dataBytes = buffer.front().hd.dataSize;

            if(dataBytes > 0) {
                // std::cout << "write buffered data to file " << dataBytes << " Bytes\n";
                std::string filename = std::to_string(connectionOrder) + ".file";
                std::ofstream os(filename, std::ios::out | std::ios::binary | std::ios::app);
                os.write(buffer.front().data, dataBytes);
                os.close();
            }
            expectedSeqNum = (expectedSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
            buffer.pop_front();
        }
    } else {
        buffer.push_back(clientPkt);
    }
    serverPkt.hd.ackNum = expectedSeqNum;
    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send ACK packet in receiveData" << std::endl;
        return;
    }
    printPacketInfo(serverPkt, 0, 0, true);
    // std::cout << "send packet to " << inet_ntoa(their_addr.sin_addr) << "\n\n";
}

int wait10Sec(int sockfd) {
    fd_set active_fd_set;
    struct timeval timeout;
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    return select(sockfd + 1, &active_fd_set, NULL, NULL, &timeout);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        std::cerr << "ERROR: Usage: ./server <portnum>" << std::endl;
        return 1;
    }

    signal(SIGQUIT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    int portnum = atoi(argv[1]);
    if(portnum < 1024 || portnum > 65535) {
        std::cerr << "ERROR: incorrect port number" << std::endl;
        exit(1);
    }

    int server_sockfd;

    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;

    unsigned int sin_size;
    int len;
    packet pkt;

    /* create a socket */
    if((server_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        std::cerr << "ERROR: create socket" << std::endl;
        exit(1);
    }

    /* set the address info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(portnum); /* short, network byte order */
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* INADDR_ANY allows clients to connect to any one of the host's IP
    address. Optionally, use this line if you know the IP to use:
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    */
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

    /* bind the socket */
    if(bind(server_sockfd, (struct sockaddr*) &my_addr, sizeof(struct sockaddr)) == -1) {
        std::cerr << "ERROR: bind" << std::endl;
        exit(1);
    }

    sin_size = sizeof(struct sockaddr_in);
    // printf("waiting for a packet...\n\n");

    while(1) {
        if((len = recvfrom(server_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &their_addr, &sin_size)) < 0) {
            std::cerr << "ERROR: recvfrom" << std::endl;
            return 1;
        }
        printPacketInfo(pkt, 0, 0, false);

        if(isSYN(pkt.hd.flags)) { // SYNbit = 1
            // std::cout << "receive connection from: " << inet_ntoa(their_addr.sin_addr) << "\n\n";
            connected = handleConnection(pkt, server_sockfd, their_addr, sin_size);
            if(connected) {
                // wait for 10 seconds, if no data, close connection, else begin to
                // receive data
                int ret = wait10Sec(server_sockfd);    
                if(ret < 0) {
                    std::cerr << "ERROR: establish connection sock select\n";
                    exit(1);
                } else if(ret == 0) { // timeout
                    std::string filename = std::to_string(connectionOrder) + ".file";
                    std::ofstream os(filename, std::ios::out | std::ios::binary);
                    os.close();
                    connected = false; // close the connection
                    // std::cout << "Receive no data from client, save empty file and close connection...\n\n";
                    // std::cout << "waiting for a packet...\n\n";
                    continue;
                } else {
                    continue;      // begin to receive data
                }
            }
        } else if(isFIN(pkt.hd.flags)) { // FINbit = 1
            // std::cout << "receive finish connection from: " << inet_ntoa(their_addr.sin_addr) << "\n";
            // std::cout << "FIN ... \n\n";
            if(!closeConnection(pkt, server_sockfd, their_addr, sin_size)) {
                std::cerr << "ERROR: close connection" << std::endl;
            }
            continue;
        }

        if(connected) {
            receiveData(pkt, server_sockfd, their_addr, sin_size);

            int ret = wait10Sec(server_sockfd);
            if(ret < 0) {
                std::cerr << "ERROR: receiveData sock select\n";
                exit(1);
            } else if(ret == 0) { // timeout
                connected = false;
                // std::cout << "Receive no more data from client, close connection...\n\n";
                // std::cout << "waiting for a packet...\n\n";
            }
        } else {
            // printf("received packet from: %s and dropped\n", inet_ntoa(their_addr.sin_addr));
            pkt.data[len - HEADER_SIZE] = '\0';
            // printf("contents: %s\n", pkt.data);
        }
    }
    
	close(server_sockfd);

    return 0;
}