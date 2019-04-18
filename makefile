server: server.o
	g++ -o server server.o

server.o: server.cc
	g++ -c server.cc
