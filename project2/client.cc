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

#include "rdt_header.h"

#define HEADER_SIZE 12
#define MAX_BUF_SIZE 512

int main(int argc, char *argv[]){
	if(argc != 4) {
		std::cerr << "ERROR: Usage: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
        return 1;
	}

	hostent* host = gethostbyname(argv[1]);
	int portnum = atoi(argv[2]);
	std::ifstream is(argv[3], std::ios::in | std::ios::binary);
	if(!is) {
        std::cerr << "ERROR: open file" << std::endl;
		return 1;
	}
	char fileBuf[MAX_BUF_SIZE];
	is.read(fileBuf, sizeof(fileBuf));

	buffer buf;
	strcpy(buf.data, fileBuf);

    int sockfd;
    int len;
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

	printf("sending: %s\n", buf.data); 

	sin_size = sizeof(struct sockaddr_in);
	
	if((len = sendto(sockfd, &buf, sizeof(buf), 0, (struct sockaddr *) &server_addr, sizeof(struct sockaddr))) < 0){
        std::cerr << "ERROR: recvfrom" << std::endl;
		return 1;
	}
 
	close(sockfd);

    return 0;
}