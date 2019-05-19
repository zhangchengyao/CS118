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
#define MAX_BUF_SIZE 1024
#define BACKLOG 1 /* pending connections queue size */

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
    char buf[MAX_BUF_SIZE];

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
        if((len = recvfrom(server_sockfd, buf, MAX_BUF_SIZE, 0, (struct sockaddr *) &their_addr, &sin_size)) < 0){
            perror("recvfrom error"); 
            return 1;
        }
        printf("received packet from %s:\n", inet_ntoa(their_addr.sin_addr));
        buf[len] = '\0';
        printf("contents: %s\n", buf);
    }
    
	close(server_sockfd);

    return 0;
}