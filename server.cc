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

std::string get_file_name(std::string str) {
    if(str.rfind("/") == -1) {
        return "";
    } else {
        return str.substr(str.rfind("/") + 1);
    }
}

void to_lower(char* arr) {
    int len = strlen(arr);
    for(int i = 0; i < len; i++) {
        if(arr[i] >= 'A' && arr[i] <= 'Z') {
            arr[i] = arr[i] - 'A' + 'a';
        }
    }
}

// reference: https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
std::unordered_map<std::string, std::string> get_all_files() {
    char buf[MAX_BUF_SIZE];
    getcwd(buf, MAX_BUF_SIZE);
    DIR* dir;
    struct dirent* ent;
    if((dir = opendir(buf)) != NULL) {
        std::unordered_map<std::string, std::string> mymap;
    /* print all the files and directories within directory */
        while((ent = readdir(dir)) != NULL) {
            char temp[MAX_BUF_SIZE];
            strcpy(temp, ent->d_name);
            to_lower(temp);
            mymap[temp] = ent->d_name;
        }
        return mymap;
        closedir (dir);
    } else {
    /* could not open directory */
        perror ("open directory");
        exit(1);
    }
}

// reference: https://www.boost.org/doc/libs/1_39_0/doc/html/boost_asio/example/http/server3/request_handler.cpp

bool url_decode(const std::string& in, std::string& out) {
	out.clear();
	out.reserve(in.size());
	for(std::size_t i = 0; i < in.size(); ++i) {
		if(in[i] == '%') {
			if(i + 3 <= in.size()) {
				int value = 0;
				std::istringstream is(in.substr(i + 1, 2));
				if (is >> std::hex >> value) {
					out += static_cast<char>(value);
					i += 2;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else if(in[i] == '+') {
			out += ' ';
		} else {
			out += in[i];
		}
	}
  	return true;
}

std::string serve_static_file(std::string file) {
    std::unordered_map<std::string, std::string> mymap = get_all_files();
    std::stringstream response;
    std::string decoded;
    
    if(!url_decode(file, decoded)) {
        response << "HTTP/1.1 400 Bad Request\r\n\r\n";
        response << "url decode!";
        return response.str();
    }

    std::transform(decoded.begin(), decoded.end(), decoded.begin(), ::tolower);
    if(mymap.find(decoded) == mymap.end()) {
        response << "HTTP/1.1 404 Not Found\r\n\r\n";
        response << "Not Found!";
        return response.str();
    }
    // Open the file
	std::ifstream is(mymap[decoded].c_str(), std::ios::in | std::ios::binary);
    if(!is) {
        response << "HTTP/1.1 400 Bad Request\r\n\r\n";
        response << "open file!";
        return response.str();
    }
    char buf[MAX_BUF_SIZE];
	std::string fileContent = "";
	while(is.read(buf, sizeof(buf)).gcount() > 0) {
		fileContent.append(buf, is.gcount());
	}

    // Determine the file extension. (txt, jpg, html....)
	std::size_t last_dot_pos = decoded.find_last_of(".");
	std::string extension = "";
	if(last_dot_pos != std::string::npos) {
		extension = decoded.substr(last_dot_pos + 1);
	}

    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Length: " << std::to_string(fileContent.size()) << "\r\n";
    if(!extension.empty()) {
        response << "Content-Type: " << extension_to_type(extension) << "\r\n";
    } else {
        response << "Content-Type: " << "text/plain\r\n";
    }
    response << "\r\n";
    response << fileContent;
    return response.str();
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
        memset(buf, 0, sizeof(buf));
        int len = read(new_fd, buf, sizeof(buf));
        printf("request header: \n%s\n", buf);
        request_parser::result_type result = request_parser_.parse(request_, buf, buf + len);
        if(result == request_parser::good) {
            std::string target = get_file_name(request_.uri);
            std::string resp = serve_static_file(target);
            write(new_fd, resp.c_str(), resp.size());
        // if(result == request_parser::good) {  
        //     if (request_.uri != "/") {
        //         serve_static_file(new_fd, "");
        //     } else {
        //         std::stringstream resp;
        //         resp << "HTTP/1.1 200 OK\r\n\r\n";
        //         resp << buf;
        //         write(new_fd, resp.str().c_str(), resp.str().length());
        //     }
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