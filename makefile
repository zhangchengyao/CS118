server: server.o request_parser.o
	g++ -o server server.o request_parser.o

server.o: server.cc request.h request_parser.h
	g++ -c server.cc -std=c++11

request_parser.o: request_parser.cc request.h request_parser.h
	g++ -c request_parser.cc -std=c++11