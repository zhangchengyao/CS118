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

bool initConnection(int sockfd, buffer& buf, sockaddr_in& server_addr, unsigned int& sin_size) {
	// initiate connection to the server
	// make the packet
    srand(2);
	uint32_t curSeqNum = rand() % (MAX_SEQ_NUM + 1);
	buf.hd.flags = (1 << 14); // set SYNbit = 1
	buf.hd.seqNum = curSeqNum;
	buf.hd.ackNum = 0;
	buf.hd.reserved = 0;
	memset(buf.data, '\0', sizeof(buf.data));

	// send the packet to server
	if(sendto(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: initConnection sendto" << std::endl;
		return false;
	} else {
        printPacketInfo(buf, true);
		// receive the SYNACK packet from the server
		if(recvfrom(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
			std::cerr << "ERROR: initConnection recvfrom" << std::endl;
			return false;
		}
        printPacketInfo(buf, false);
		// check whether the infomation is right
		if(buf.hd.flags == ((1 << 15) | (1 << 14)) && buf.hd.ackNum == curSeqNum + 1) {
			return true;
		}
		return false;
	}
}

void ackAndTransit(int sockfd, buffer& buf, sockaddr_in& server_addr, unsigned int& sin_size, char* file) {
	// ack SYNACK from the server
	buf.hd.flags = (1 << 15); // set ACKbit = 1
	uint32_t curSeqNum = buf.hd.ackNum;
	uint32_t ackNum = buf.hd.seqNum + 1;
	buf.hd.seqNum = curSeqNum;
	buf.hd.ackNum = ackNum;
	buf.hd.reserved = 0;

	// this packet can include data
	std::ifstream is(file, std::ios::in | std::ios::binary);
	if(!is) {
        std::cerr << "ERROR: open file" << std::endl;
		return;
	}
	char fileBuf[MAX_BUF_SIZE];
	is.read(fileBuf, sizeof(fileBuf));
	strcpy(buf.data, fileBuf);

    std::cout << "sending: " << buf.data << std::endl;
	// send the ack packet with data to the server
	if(sendto(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: ack SYNACK sendto" << std::endl;
	}
    printPacketInfo(buf, true);
	// TODO send large files
}

bool closeConnection(int sockfd, buffer& buf, sockaddr_in& server_addr, unsigned int& sin_size) {
    // send FIN to server
    srand(2);
    uint32_t curSeqNum = rand() % (MAX_SEQ_NUM + 1);
	buf.hd.flags = (1 << 13); // set FINbit = 1
	buf.hd.seqNum = curSeqNum;
	buf.hd.ackNum = 0;
	buf.hd.reserved = 0;
	memset(buf.data, '\0', sizeof(buf.data));

    if(sendto(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: closeConnection FIN sendto" << std::endl;
        return false;
    }
    printPacketInfo(buf, true);

    // receive the ACK packet from the server
    if(recvfrom(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
        std::cerr << "ERROR: closeConnection ACK recvfrom" << std::endl;
        return false;
    }
    printPacketInfo(buf, false);

    if(buf.hd.flags == (1 << 15) && buf.hd.ackNum == (curSeqNum + 1) % MAX_SEQ_NUM) {
        // wait for 2 seconds...

        uint32_t serverSeqNum = buf.hd.seqNum;
        
        if(recvfrom(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
            std::cerr << "ERROR: closeConnection FIN recvfrom" << std::endl;
            return false;
        }
        printPacketInfo(buf, false);

        // send ACK to server
        buf.hd.flags = (1 << 15); // set ACKbit = 1
        buf.hd.seqNum = (curSeqNum + 1) % MAX_SEQ_NUM;
        buf.hd.ackNum = (serverSeqNum + 1) % MAX_SEQ_NUM;
        buf.hd.reserved = 0;
        memset(buf.data, '\0', sizeof(buf.data));

        if(sendto(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
            std::cerr << "ERROR: closeConnection ACK sendto" << std::endl;
            return false;
        }
        printPacketInfo(buf, true);

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
	int portnum = atoi(argv[2]);

	buffer buf;
	
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
	
	bool connected = initConnection(sockfd, buf, server_addr, sin_size);
	if(connected) {
		ackAndTransit(sockfd, buf, server_addr, sin_size, argv[3]);
        
        if(!closeConnection(sockfd, buf, server_addr, sin_size)) {
            std::cerr << "ERROR: close connection" << std::endl;
        }
	} else {
		std::cerr << "ERROR: init connection" << std::endl;
	}
 
	close(sockfd);

    return 0;
}