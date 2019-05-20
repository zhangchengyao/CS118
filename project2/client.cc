#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>

#define MAX_BUF_SIZE 1024

int main(int argc, char *argv[]){
	if(argc != 4) {
		printf("Usage: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>\n");
        return 1;
	}

	hostent* host = gethostbyname(argv[1]);
	int portnum = atoi(argv[2]);
	std::ifstream is(argv[3], std::ios::in | std::ios::binary);
	if(!is) {
		perror("open file error");
		return 1;
	}
	char buf[MAX_BUF_SIZE];
	is.read(buf, sizeof(buf));

    int sockfd;
    int len;
    struct sockaddr_in server_addr;
    unsigned int sin_size;
    
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = ((struct in_addr*)(host->h_addr_list[0]))->s_addr;
	server_addr.sin_port = htons(portnum);
 
	if((sockfd = socket(PF_INET,SOCK_DGRAM,0)) < 0){  
		perror("socket error");
		return 1;
	}

	printf("sending: %s\n", buf); 

	sin_size = sizeof(struct sockaddr_in);
	
	if((len = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &server_addr, sizeof(struct sockaddr))) < 0){
		perror("recvfrom"); 
		return 1;
	}
 
	close(sockfd);

    return 0;
}