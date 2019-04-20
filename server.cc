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
#define MAX_BUF_SIZE 1024
#define BACKLOG 1 /* pending connections queue size */

struct mimetype_mapping {
    const char* extension;
    const char* mime_type;
} mappings[] = {
    { "gif", "image/gif" },
    { "htm", "text/html" },
    { "html", "text/html" },
    { "jpg", "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "tar", "application/x-tar" },
    { "png", "image/png" },
    { "txt", "text/plain"},
    { "zip", "application/zip"}
};

std::string extension_to_type(const std::string& extension) {
  for(mimetype_mapping m: mappings) {
    if(m.extension == extension) {
      return m.mime_type;
    }
  }
  return "text/plain";
}

bool serve_static_file(int sockfd, std::string filename) {
    char buf[MAX_BUF_SIZE];
    FILE *fp = fopen("index.html", "r");
    int file_block_length = 0;
    std::stringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: text/html\r\n\r\n";
    bzero(buf, sizeof(buf));
    write(sockfd, resp.str().c_str(), resp.str().length());
    while( (file_block_length = fread(buf, sizeof(char), 1024, fp)) > 0)
    {
        // printf("file_block_length = %d\n", file_block_length);

        // 发送buffer中的字符串到new_server_socket,实际上就是发送给客户端
        if (write(sockfd, buf, file_block_length) < 0)
        {
            printf("Send File:\t%s Failed!\n", "file_name");
            return false;
        }
        bzero(buf, sizeof(buf));
    }
    fclose(fp);
    return true;
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Usage: ./server <portnum>\n");
        return 1;
    }

    int portnum = std::atoi(argv[1]);

    int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */
    struct sockaddr_in my_addr; /* my address */
    struct sockaddr_in their_addr; /* connector address */
    unsigned int sin_size;
    char buf[MAX_BUF_SIZE];
    request request_;
    request_parser request_parser_;

    /* create a socket */
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
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
            if (request_.uri != "/") {
                serve_static_file(new_fd, "");
            } else {
                std::stringstream resp;
                resp << "HTTP/1.1 200 OK\r\n\r\n";
                resp << buf;
                write(new_fd, resp.str().c_str(), resp.str().length());
            }
        } else {
            printf("Bad request!\n");
        }
        request_.clear();
        request_parser_.reset();
        close(new_fd);
    }
    
    close(sockfd);
    return 0;
}