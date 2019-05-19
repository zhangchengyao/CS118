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

#define MAX_BUF_SIZE 1024

int main(int argc, char *argv[]){
    int sockfd;
    int len;
    struct sockaddr_in server_addr;
    unsigned int sin_size;
    char buf[MAX_BUF_SIZE];
    
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; 
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(8000);
 
	if((sockfd = socket(PF_INET,SOCK_DGRAM,0)) < 0){  
		perror("socket error");
		return 1;
	}

	strcpy(buf,"This is a test message");
	printf("sending: '%s'\n",buf); 

	sin_size = sizeof(struct sockaddr_in);
	
	if((len = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &server_addr, sizeof(struct sockaddr))) < 0){
		perror("recvfrom"); 
		return 1;
	}
 
	close(sockfd);

    return 0;
}