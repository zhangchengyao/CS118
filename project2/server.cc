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

#include "rdt_header.h"

#define HEADER_SIZE 12
#define BACKLOG 1 /* pending connections queue size */
#define MAX_SEQ_NUM 25600

int connectionOrder = 1;
bool connected = false;

bool handleConnection(buffer& clientPkt, int server_sockfd, sockaddr_in& their_addr, unsigned int sin_size) {
    // choose init seq num, send SYNACK msg, acking SYN from client
    buffer serverPkt;
    serverPkt.hd.flags |= ((1 << 15) | (1 << 14)); // set ACKbit = 1 and SYNbit = 1
    srand(1);
    serverPkt.hd.seqNum = rand() % (MAX_SEQ_NUM + 1);
    serverPkt.hd.ackNum = clientPkt.hd.seqNum + 1;
    memset(serverPkt.data, '\0', sizeof(serverPkt.data));
    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send SYNACK packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, true);

    printf("send SYNACK packet to: %s\n", inet_ntoa(their_addr.sin_addr));

    // receive ACK packet from client, finish 3-way handshake
    if(recvfrom(server_sockfd, &clientPkt, sizeof(clientPkt), 0, (struct sockaddr*) &their_addr, &sin_size) < 0) {
        std::cerr << "ERROR: receive ACK from client" << std::endl;
        return false;
    }
    printf("receive ACK packet from: %s\n", inet_ntoa(their_addr.sin_addr));
    printPacketInfo(clientPkt, false);
    
    if(clientPkt.hd.ackNum == serverPkt.hd.seqNum + 1) {
        return true;
    } else {
        std::cerr << "ACKnum" << std::endl;
        return false;
    }
}

bool closeConnection(buffer& clientPkt, int server_sockfd, sockaddr_in &their_addr, unsigned int sin_size) {
    buffer serverPkt;
    memset(serverPkt.data, '\0', sizeof(serverPkt.data));

    srand(1);
    uint32_t seqNum = rand() % (MAX_SEQ_NUM + 1);

    serverPkt.hd.flags |= (1 << 15); // set ACKbit = 1
    serverPkt.hd.seqNum = seqNum;
    serverPkt.hd.ackNum = clientPkt.hd.seqNum + 1;

    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send ACK packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, true);

    std::cout << "send ACK packet to: " << inet_ntoa(their_addr.sin_addr) << std::endl;

    // ...

    bzero(&serverPkt, sizeof(serverPkt));
    serverPkt.hd.flags |= (1 << 13); // set FINbit = 1
    serverPkt.hd.seqNum = seqNum;
    serverPkt.hd.ackNum = 0;

    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send FIN packet" << std::endl;
        return false;
    }
    printPacketInfo(serverPkt, true);

    std::cout << "send FIN packet to: " << inet_ntoa(their_addr.sin_addr) << std::endl;

    // receive ACK packet from client, close connection
    if(recvfrom(server_sockfd, &clientPkt, sizeof(clientPkt), 0, (struct sockaddr*) &their_addr, &sin_size) < 0) {
        std::cerr << "ERROR: receive ACK from client" << std::endl;
        return false;
    }
    printPacketInfo(clientPkt, false);

    connectionOrder++;
    connected = false;
    return true;
}

void signalHandler(int signal) {
    std::cout << "interrupted: " << signal << std::endl;
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

void receiveData(buffer& clientPkt, int server_sockfd, sockaddr_in &their_addr, unsigned int sin_size) {
    buffer serverPkt;
    uint32_t expectedSeqNum = clientPkt.hd.seqNum + strlen(clientPkt.data);
    
    // ACK the ACK packet from client at the last of 3-way handshake
    serverPkt.hd.flags |= (1 << 15); // set ACKbit = 1
    serverPkt.hd.ackNum = expectedSeqNum;
    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        std::cerr << "ERROR: send ACK packet in receiveData" << std::endl;
        return;
    }
    printPacketInfo(serverPkt, true);

    // receive the data in client's ACK packet in connection establishment if any
    if(strlen(clientPkt.data) > 0) {
        std::string filename = std::to_string(connectionOrder) + ".file";
        std::ofstream os(filename, std::ios::out | std::ios::binary);
        os << clientPkt.data;
        os.close();
    }
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
    buffer buf;

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
    printf("waiting for a packet...\n");

    while(1) {
        if((len = recvfrom(server_sockfd, &buf, sizeof(buf), 0, (struct sockaddr *) &their_addr, &sin_size)) < 0) {
            std::cerr << "ERROR: recvfrom" << std::endl;
            return 1;
        }
        printPacketInfo(buf, false);

        if(isSYN(buf.hd.flags)) { // SYNbit = 1
            std::cout << "receive connection from: " << inet_ntoa(their_addr.sin_addr) << std::endl;
            connected = handleConnection(buf, server_sockfd, their_addr, sin_size);
        } else if(isFIN(buf.hd.flags)) { // FINbit = 1
            std::cout << "receive connection from: " << inet_ntoa(their_addr.sin_addr) << std::endl;
            std::cout << "FIN ... " << std::endl;
            if(!closeConnection(buf, server_sockfd, their_addr, sin_size)) {
                std::cerr << "ERROR: close connection" << std::endl;
            }
            continue;
        }

        if(connected) {
            printf("establish connection with %s:\n", inet_ntoa(their_addr.sin_addr));
            receiveData(buf, server_sockfd, their_addr, sin_size);
            // TODO receive the file
        } else {
            printf("received packet from: %s and dropped\n", inet_ntoa(their_addr.sin_addr));
            buf.data[len - HEADER_SIZE] = '\0';
            printf("contents: %s\n", buf.data);
        }
        
    }
    
	close(server_sockfd);

    return 0;
}