#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <ctime>

#include "rdt_header.h"

#define HEADER_SIZE 12
#define MAX_BUF_SIZE 512
#define MAX_SEQ_NUM 25600
#define MAX_CWND 10240

int cwnd = 512;
int ssthresh = 5120;

bool initConnection(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size) {
	// initiate connection to the server
	// make the packet
    srand(2);
	uint32_t curSeqNum = rand() % (MAX_SEQ_NUM + 1);
	pkt.hd.flags = (1 << 14); // set SYNbit = 1
	pkt.hd.seqNum = curSeqNum;
	pkt.hd.ackNum = 0;
	pkt.hd.reserved = 0;
	memset(pkt.data, '\0', sizeof(pkt.data));

	// send the packet to server
	if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: initConnection sendto" << std::endl;
		return false;
	} else {
        printPacketInfo(pkt, cwnd, ssthresh, true);
		// receive the SYNACK packet from the server
		if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
			std::cerr << "ERROR: initConnection recvfrom" << std::endl;
			return false;
		}
        printPacketInfo(pkt, cwnd, ssthresh, false);
		// check whether the infomation is right
		if(pkt.hd.flags == ((1 << 15) | (1 << 14)) && pkt.hd.ackNum == curSeqNum + 1) {
			return true;
		}
		return false;
	}
}

void ackAndTransit(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size, char* file) {
	// ack SYNACK from the server
	pkt.hd.flags = (1 << 15); // set ACKbit = 1
	uint32_t curSeqNum = pkt.hd.ackNum;
	uint32_t ackNum = pkt.hd.seqNum + 1;
	pkt.hd.seqNum = curSeqNum;
	pkt.hd.ackNum = ackNum;
	pkt.hd.reserved = 0;

	// this packet can include data
	std::ifstream is(file, std::ios::in | std::ios::binary);
	if(!is) {
        std::cerr << "ERROR: open file" << std::endl;
		return;
	}
	char filebuf[MAX_BUF_SIZE];
	is.read(filebuf, sizeof(filebuf));
	strcpy(pkt.data, filebuf);

    std::cout << "sending: " << pkt.data << std::endl;
	// send the ack packet with data to the server
	if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: ack SYNACK sendto" << std::endl;
	}
    printPacketInfo(pkt, cwnd, ssthresh, true);
	int dataBytes = strlen(pkt.data) < MAX_BUF_SIZE ? strlen(pkt.data) : MAX_BUF_SIZE;
	curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);

	// send large files
	while(is.read(filebuf, sizeof(filebuf)).gcount() > 0) {
		if(is.gcount() < MAX_BUF_SIZE) {
			filebuf[is.gcount()] = '\0';
		}
		recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size);
		while(pkt.hd.ackNum != curSeqNum) {
			// TODO packet loss
			std::cout << "duplicate ack!" << std::endl;
			recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size);
		}
		pkt.hd.flags = 0;
		pkt.hd.ackNum = 0;
		pkt.hd.reserved = 0;
		pkt.hd.seqNum = curSeqNum;
		strcpy(pkt.data, filebuf);
		std::cout << "sending: " << pkt.data << std::endl;
		sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
		curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
	}
	is.close();

	// receive the last ACK from server
	if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
		std::cerr << "ERROR: ackAndTransit recvfrom" << std::endl;
		return;
	}
}

bool closeConnection(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size) {
    // send FIN to server
    srand(2);
    uint32_t curSeqNum = rand() % (MAX_SEQ_NUM + 1);
	pkt.hd.flags = (1 << 13); // set FINbit = 1
	pkt.hd.seqNum = curSeqNum;
	pkt.hd.ackNum = 0;
	pkt.hd.reserved = 0;
	memset(pkt.data, '\0', sizeof(pkt.data));

    if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: closeConnection FIN sendto" << std::endl;
        return false;
    }
    printPacketInfo(pkt, cwnd, ssthresh, true);

    // receive the ACK packet from the server
    if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
        std::cerr << "ERROR: closeConnection ACK recvfrom" << std::endl;
        return false;
    }
    printPacketInfo(pkt, cwnd, ssthresh, false);

    if(pkt.hd.flags == (1 << 15) && pkt.hd.ackNum == (curSeqNum + 1) % MAX_SEQ_NUM) {
        // wait for 2 seconds...

        uint32_t serverSeqNum = pkt.hd.seqNum;
        
        if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
            std::cerr << "ERROR: closeConnection FIN recvfrom" << std::endl;
            return false;
        }
        printPacketInfo(pkt, cwnd, ssthresh, false);

        // send ACK to server
        pkt.hd.flags = (1 << 15); // set ACKbit = 1
        pkt.hd.seqNum = (curSeqNum + 1) % MAX_SEQ_NUM;
        pkt.hd.ackNum = (serverSeqNum + 1) % MAX_SEQ_NUM;
        pkt.hd.reserved = 0;
        memset(pkt.data, '\0', sizeof(pkt.data));

        if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
            std::cerr << "ERROR: closeConnection ACK sendto" << std::endl;
            return false;
        }
        printPacketInfo(pkt, cwnd, ssthresh, true);

        std::cout << "successfully closed connection" << std::endl;
        return true;
    }
    
    return false;
}

int main(int argc, char *argv[]){
	if(argc != 4) {
		std::cerr << "ERROR: Usage: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
        return 1;
	}

	hostent* host = gethostbyname(argv[1]);
	if(host == NULL) {
		std::cerr << "ERROR: Incorrect hostname" << std::endl;
		exit(1);
	}
	int portnum = atoi(argv[2]);
	if(portnum < 1024 || portnum > 65535) {
		std::cerr << "ERROR: Incorrect portnum" << std::endl;
		exit(1);
	}

	packet pkt;
	
    int sockfd;
    struct sockaddr_in server_addr;
    unsigned int sin_size;
    
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = ((struct in_addr*)(host->h_addr_list[0]))->s_addr;
	server_addr.sin_port = htons(portnum);
 
	if((sockfd = socket(PF_INET,SOCK_DGRAM,0)) < 0){
        std::cerr << "ERROR: socket" << std::endl;
		return 1;
	}

	sin_size = sizeof(struct sockaddr_in);
	
	bool connected = initConnection(sockfd, pkt, server_addr, sin_size);
	if(connected) {
		ackAndTransit(sockfd, pkt, server_addr, sin_size, argv[3]);
        
        if(!closeConnection(sockfd, pkt, server_addr, sin_size)) {
            std::cerr << "ERROR: close connection" << std::endl;
        }
	} else {
		std::cerr << "ERROR: init connection" << std::endl;
	}
 
	close(sockfd);

    return 0;
}