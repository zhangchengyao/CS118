#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <algorithm>

#include "rdt_header.h"

#define HEADER_SIZE 12
#define BACKLOG 1 /* pending connections queue size */
#define MAX_SEQ_NUM 25600

bool handleConnection(buffer& clientPkt, int server_sockfd, sockaddr_in& their_addr, unsigned int sin_size) {
    // choose init seq num, send SYNACK msg, acking SYN from client
    buffer serverPkt;
    serverPkt.hd.flags |= ((1 << 15) | (1 << 14)); // set ACKbit = 1 and SYNbit = 1
    serverPkt.hd.seqNum = rand() % (MAX_SEQ_NUM + 1);
    serverPkt.hd.ackNum = clientPkt.hd.seqNum + 1;
    memset(serverPkt.data, sizeof(serverPkt.data), '\0');
    if(sendto(server_sockfd, &serverPkt, sizeof(serverPkt), 0, (struct sockaddr*) &their_addr, sin_size) < 0) {
        perror("send SYNACK packet");
        return false;
    }
    printf("send SYNACK packet to: %s\n", inet_ntoa(their_addr.sin_addr));

    // receive ACK packet from client, finish 3-way handshake
    if(recvfrom(server_sockfd, &clientPkt, sizeof(clientPkt), 0, (struct sockaddr*) &their_addr, &sin_size) < 0) {
        perror("receive ACK from client");
        return false;
    }
    printf("receive ACK packet from: %s\n", inet_ntoa(their_addr.sin_addr));
    if(clientPkt.hd.ackNum == serverPkt.hd.seqNum + 1) {
        return true;
    } else {
        perror("ACKnum");
        return false;
    }
}

int main(int argc, char *argv[]){
    if(argc != 2) {
        printf("Usage: ./server <portnum>\n");
        return 1;
    }

    int portnum = std::atoi(argv[1]);

    int server_sockfd;

    struct sockaddr_in my_addr;
    struct sockaddr_in their_addr;

    unsigned int sin_size;
    int len;
    buffer buf;

    /* create a socket */
    if((server_sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
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
        perror("bind");
        exit(1);
    }

    sin_size = sizeof(struct sockaddr_in);
    printf("waiting for a packet...\n");

    while(1){
        bool connected = false;
        if((len = recvfrom(server_sockfd, &buf, sizeof(buf), 0, (struct sockaddr *) &their_addr, &sin_size)) < 0){
            perror("recvfrom error"); 
            return 1;
        }
        if((buf.hd.flags >> 14) & 1) { // SYNbit = 1
            printf("receive connection from %s:\n", inet_ntoa(their_addr.sin_addr));
            connected = handleConnection(buf, server_sockfd, their_addr, sin_size);
        }
        if(connected) {
            printf("establish connection with %s:\n", inet_ntoa(their_addr.sin_addr));
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