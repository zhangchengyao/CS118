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
#include <string>
#include <sstream>
#include "request.h"
#include "request_parser.h"
#define MYPORT 8080
#define BACKLOG 1 /* pending connections queue size */

int main() {
    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr; /* my address */
    struct sockaddr_in their_addr; /* connector address */
    unsigned int sin_size;
    char buf[1024];
    request request_;
    request_parser request_parser_;

    /* create a socket */
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* set the address info */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT); /* short, network byte order */
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* INADDR_ANY allows clients to connect to any one of the host's IP
    address. Optionally, use this line if you know the IP to use:
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    */
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

    /* bind the socket */
    if(bind(sockfd, (struct sockaddr*) &my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    /* listen */
    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    while(1) { /* main accept loop */
        sin_size = sizeof(struct sockaddr_in);
        if((new_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        printf("server: got connection from %s\n\n",
               inet_ntoa(their_addr.sin_addr));
        int len = read(new_fd, buf, sizeof(buf));
        printf("request header: \n%s\n", buf);
        request_parser::result_type result = request_parser_.parse(request_, buf, buf + len);
        if(result == request_parser::good) {
            std::stringstream resp;
            resp << "HTTP/1.1 200 OK\r\n\r\n";
            resp << buf;
            write(new_fd, resp.str().c_str(), resp.str().length());
        } else {
            printf("Bad request!\n");
        }
        close(new_fd);
    }
    
    close(sockfd);
    return 0;
}