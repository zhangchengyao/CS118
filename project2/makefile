all: server client

server: server.o
	g++ -o server server.o

client: client.o
	g++ -o client client.o

server.o: server.cc
	g++ -c server.cc -std=c++11

client.o: client.cc
	g++ -c client.cc -std=c++11

clean:
	rm -rf *.o client server