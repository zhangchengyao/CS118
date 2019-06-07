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
#include <thread>
#include <time.h>
#include <list>
#include <algorithm>

#include "rdt_header.h"

#define HEADER_SIZE 12
#define MAX_BUF_SIZE 512
#define MAX_SEQ_NUM 25600
#define MAX_CWND 10240
#define INIT_CWND 512
#define INIT_SSTHRESH 5120
#define RTO 500 // ms

int cwnd = INIT_CWND;  // initial window size
int ssthresh = INIT_SSTHRESH;  // initial slow start threshold
uint32_t curSeqNum;
time_t lastServerPktTime;

int wait10Sec(int sockfd) {
    fd_set active_fd_set;
    struct timeval timeout;
    FD_ZERO(&active_fd_set);
    FD_SET(sockfd, &active_fd_set);

    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    return select(sockfd + 1, &active_fd_set, NULL, NULL, &timeout);
}

bool initConnection(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size) {
	// initiate connection to the server
	// make the packet
    srand(2);
	curSeqNum = rand() % (MAX_SEQ_NUM + 1);
	pkt.hd.flags = (1 << 14); // set SYNbit = 1
	pkt.hd.seqNum = curSeqNum;
	pkt.hd.ackNum = 0;
	pkt.hd.dataSize = 0;

	// send the packet to server
	if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: initConnection sendto" << std::endl;
		return false;
	}

    printPacketInfo(pkt, INIT_CWND, INIT_SSTHRESH, true, false);
	// receive the SYNACK packet from the server

	int ret = wait10Sec(sockfd);
	if(ret < 0) {
		std::cerr << "ERROR: initConnection sock select\n";
		exit(1);
	} else if(ret == 0) { // timeout
		// std::cout << "Receive no packet from client, close connection...\n\n";
		close(sockfd);
		exit(1);
	}

	if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
		std::cerr << "ERROR: initConnection recvfrom" << std::endl;
		return false;
	}
    printPacketInfo(pkt, INIT_CWND, INIT_SSTHRESH, false, false);
	lastServerPktTime = time(NULL);
	// check whether the infomation is right
	if(pkt.hd.flags == ((1 << 15) | (1 << 14)) && pkt.hd.ackNum == (curSeqNum + 1) % (MAX_SEQ_NUM + 1)) {
		pkt.hd.flags = (1 << 15); // set ACKbit = 1
		curSeqNum = pkt.hd.ackNum;
		uint32_t ackNum = (pkt.hd.seqNum + 1) % (MAX_SEQ_NUM + 1);
		pkt.hd.seqNum = curSeqNum;
		pkt.hd.ackNum = ackNum;
		// send the last ACK packet in 3 way handshake
		if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
			std::cerr << "ERROR: initConnection last ack" << std::endl;
			return false;
		}
		printPacketInfo(pkt, INIT_CWND, INIT_SSTHRESH, true, false);
		return true;
	}
	return false;
}

void transmitData(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size, char* file) {
	std::ifstream is(file, std::ios::in | std::ios::binary);
	if(!is) {
        std::cerr << "ERROR: open file" << std::endl;
		return;
	}
	char filebuf[MAX_BUF_SIZE];

	// transmit the file
	bool eof = false;
	std::list<packet> senderBuffer;
	clock_t timer;

	int dupPktCnt = 0;

	if(is.read(filebuf, sizeof(filebuf)).gcount() > 0 ) {
		pkt.hd.flags = 0;
		pkt.hd.ackNum = 0;
		pkt.hd.dataSize = is.gcount();
		pkt.hd.seqNum = curSeqNum;
		memcpy(pkt.data, filebuf, MAX_BUF_SIZE);
		
		senderBuffer.push_back(pkt);
		uint16_t dataBytes = pkt.hd.dataSize;
		curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
	} else {
		eof = true;
	}

	while(!eof) {
		sendto(sockfd, &senderBuffer.front(), sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
		printPacketInfo(senderBuffer.front(), cwnd, ssthresh, true, false);
		timer = clock();
		// for(std::list<packet>::iterator it = senderBuffer.begin(); it != senderBuffer.end(); it++) {
		// 	sendto(sockfd, &(*it), sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
		// 	printPacketInfo(*it, cwnd, ssthresh, true);
		// 	if(it == senderBuffer.begin()) {
		// 		timer = clock();
		// 	}
		// }
		
		fd_set active_fd_set;
		struct timeval timeout;
		FD_ZERO(&active_fd_set);
		FD_SET(sockfd, &active_fd_set);

		timeout.tv_sec = 0;
    	timeout.tv_usec = RTO * 1000;

		while(true) {
			int ret = select(sockfd + 1, &active_fd_set, NULL, NULL, &timeout); 

			if(ret < 0) {
				std::cerr << "ERROR: receive ACK packet" << std::endl;
            	return ;
			} else if(ret == 0) {
				// first check whether the data transmission finishes
				if(is.eof() && senderBuffer.empty()) {
					break;
				}

				// then check whether the client has not received any more packet from
				// server for 10sec
				time_t currentTime = time(NULL);
				if(difftime(currentTime, lastServerPktTime) > 10) {
					// std::cout << "Receive no more packets from server, close connection...\n\n";
					close(sockfd);
					exit(1);
				}

				// timeout, reset ssthresh and cwnd
				ssthresh = std::max(cwnd / 2, 1024);
				cwnd = INIT_CWND;
				timeout.tv_usec = RTO * 1000;
				break;
			} else {
				// an ACK arrives
				recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size);
				lastServerPktTime = time(NULL);
				// printPacketInfo(pkt, cwnd, ssthresh, false);

				if(pkt.hd.ackNum == (senderBuffer.front().hd.seqNum + senderBuffer.front().hd.dataSize) % (MAX_SEQ_NUM + 1)) {
					// The packet has been successfully received, remove it from buffer
					printPacketInfo(pkt, cwnd, ssthresh, false, false);
					senderBuffer.pop_front();
					timer = clock();
					timeout.tv_usec = RTO * 1000;
			
					if(cwnd < ssthresh) {
						// slow start
						cwnd += MAX_BUF_SIZE;
					} else if(cwnd < MAX_CWND) {
						// congestion avoidance
						cwnd += (MAX_BUF_SIZE * MAX_BUF_SIZE) / cwnd;
						cwnd = std::min(cwnd, MAX_CWND);
					}

					int pktsToSend = cwnd / MAX_BUF_SIZE - senderBuffer.size();
					for(int k = 0; k < pktsToSend; k++) {
						if(is.read(filebuf, sizeof(filebuf)).gcount() > 0) {
							pkt.hd.flags = 0;
							pkt.hd.ackNum = 0;
							pkt.hd.dataSize = is.gcount();
							pkt.hd.seqNum = curSeqNum;
							memcpy(pkt.data, filebuf, MAX_BUF_SIZE);
							sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
							printPacketInfo(pkt, cwnd, ssthresh, true, false);
							// buffer the sent packet
							senderBuffer.push_back(pkt);
							uint16_t dataBytes = pkt.hd.dataSize;
							curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
						}
					}
				} else if(pkt.hd.ackNum == senderBuffer.front().hd.seqNum) {
					// duplicate ACK
					printPacketInfo(pkt, cwnd, ssthresh, false, true);
					dupPktCnt++;

					if(dupPktCnt == 3) {
						// fast retransmit
						ssthresh = std::max(cwnd / 2, 1024);
						cwnd = ssthresh + 3 * MAX_BUF_SIZE;

						// retransmit the lost packet
						sendto(sockfd, &senderBuffer.front(), sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
						printPacketInfo(pkt, cwnd, ssthresh, true, false);

						int pktsToSend = cwnd / MAX_BUF_SIZE - senderBuffer.size();
						for(int k = 0; k < pktsToSend; k++) {
							if(is.read(filebuf, sizeof(filebuf)).gcount() > 0) {
								pkt.hd.flags = 0;
								pkt.hd.ackNum = 0;
								pkt.hd.dataSize = is.gcount();
								pkt.hd.seqNum = curSeqNum;
								memcpy(pkt.data, filebuf, MAX_BUF_SIZE);
								sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
								printPacketInfo(pkt, cwnd, ssthresh, true, false);
								// buffer the sent packet
								senderBuffer.push_back(pkt);
								uint16_t dataBytes = pkt.hd.dataSize;
								curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
							}
						}
					} else if(dupPktCnt > 3) {
						// another duplicate ACK arrives
						if(cwnd < MAX_CWND) {
							cwnd += MAX_BUF_SIZE;
							cwnd = std::min(cwnd, MAX_CWND);

							if(is.read(filebuf, sizeof(filebuf)).gcount() > 0) {
								pkt.hd.flags = 0;
								pkt.hd.ackNum = 0;
								pkt.hd.dataSize = is.gcount();
								pkt.hd.seqNum = curSeqNum;
								memcpy(pkt.data, filebuf, MAX_BUF_SIZE);
								sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
								printPacketInfo(pkt, cwnd, ssthresh, true, false);
								// buffer the sent packet
								senderBuffer.push_back(pkt);
								uint16_t dataBytes = pkt.hd.dataSize;
								curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
							}
						}
					}
				} else {
					// a new ACK comes
					printPacketInfo(pkt, cwnd, ssthresh, false, false);
					cwnd = ssthresh;
					dupPktCnt = 0;

					while(!senderBuffer.empty() && senderBuffer.front().hd.seqNum != pkt.hd.ackNum) {
						senderBuffer.pop_front();
					}
					if(!senderBuffer.empty()) {
						timer = clock();
						timeout.tv_usec = RTO * 1000;
					}

					int pktsToSend = cwnd / MAX_BUF_SIZE - senderBuffer.size();
					for(int k = 0; k < pktsToSend; k++) {
						if(is.read(filebuf, sizeof(filebuf)).gcount() > 0) {
							pkt.hd.flags = 0;
							pkt.hd.ackNum = 0;
							pkt.hd.dataSize = is.gcount();
							pkt.hd.seqNum = curSeqNum;
							memcpy(pkt.data, filebuf, MAX_BUF_SIZE);
							sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr));
							printPacketInfo(pkt, cwnd, ssthresh, true, false);
							// buffer the sent packet
							senderBuffer.push_back(pkt);
							uint16_t dataBytes = pkt.hd.dataSize;
							curSeqNum = (curSeqNum + dataBytes) % (MAX_SEQ_NUM + 1);
						}
					}
				}
			}
		}

		if(is.eof() && senderBuffer.empty()) {
			eof = true;
		}
	}

	is.close();
}

bool closeConnection(int sockfd, packet& pkt, sockaddr_in& server_addr, unsigned int& sin_size) {
    // send FIN to server
	pkt.hd.flags = (1 << 13); // set FINbit = 1
	pkt.hd.seqNum = curSeqNum;
	pkt.hd.ackNum = 0;
	pkt.hd.dataSize = 0;
	memset(pkt.data, '\0', sizeof(pkt.data));

    if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
        std::cerr << "ERROR: closeConnection FIN sendto" << std::endl;
        return false;
    }
    printPacketInfo(pkt, cwnd, ssthresh, true, false);
	curSeqNum = (curSeqNum + 1) % MAX_SEQ_NUM;

    int ret = wait10Sec(sockfd);
	if(ret < 0) {
		std::cerr << "ERROR: closeConnection sock select\n";
		exit(1);
	} else if(ret == 0) { // timeout
		// std::cout << "Receive no packet from server, abort connection...\n\n";
		close(sockfd);
		exit(1);
	}

    // receive the ACK packet from the server
    if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
        std::cerr << "ERROR: closeConnection ACK recvfrom" << std::endl;
        return false;
    }
    printPacketInfo(pkt, cwnd, ssthresh, false, false);

    if(pkt.hd.flags == (1 << 15) && pkt.hd.ackNum == curSeqNum ) {
        // receive ACK from the server
		// wait for 2 seconds before closing connection

        uint32_t serverSeqNum = pkt.hd.seqNum;

		clock_t start = clock();

		fd_set active_fd_set;
		struct timeval timeout;
		FD_ZERO(&active_fd_set);
		FD_SET(sockfd, &active_fd_set);

		clock_t cur;
		double time_elapse = 0.0;
		do {
			double time_remaining = 2 - time_elapse;
			timeout.tv_sec = (int)time_remaining;
    		timeout.tv_usec = (time_remaining - timeout.tv_sec) * 1000000;

			ret = select(sockfd + 1, &active_fd_set, NULL, NULL, &timeout); 

			if(ret < 0) {
				std::cerr << "ERROR: closeConnection sock select" << std::endl;
            	return false;
			} else if(ret == 0) {
				// timeout
				break;
			} else {
				// server sends FIN to the client
				if(recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, &sin_size) < 0) {
					std::cerr << "ERROR: closeConnection FIN recvfrom" << std::endl;
					return false;
				}
				printPacketInfo(pkt, cwnd, ssthresh, false, false);

				// check the flag of the packet
				// drop any other non-FIN packet
				if(pkt.hd.flags == (1 << 13)) {
					// send ACK to server
					pkt.hd.flags = (1 << 15); // set ACKbit = 1
					pkt.hd.seqNum = curSeqNum;
					pkt.hd.ackNum = (serverSeqNum + 1) % MAX_SEQ_NUM;
					pkt.hd.dataSize = 0;
					memset(pkt.data, '\0', sizeof(pkt.data));

					if(sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr)) < 0) {
						std::cerr << "ERROR: closeConnection ACK sendto" << std::endl;
						return false;
					}
					printPacketInfo(pkt, cwnd, ssthresh, true, false);
				}
			}

			cur = clock();
			time_elapse = (double)(cur - start) / CLOCKS_PER_SEC;
		} while(time_elapse < 2.0);

        // std::cout << "successfully closed connection" << std::endl;
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
		transmitData(sockfd, pkt, server_addr, sin_size, argv[3]);
        
        if(!closeConnection(sockfd, pkt, server_addr, sin_size)) {
            std::cerr << "ERROR: close connection" << std::endl;
        }
	} else {
		std::cerr << "ERROR: init connection" << std::endl;
	}
 
	close(sockfd);

    return 0;
}