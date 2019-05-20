# 1 Overview
The purpose of this project is to implement a basic version of reliable data transfer protocol, 
including connection management and congestion control. You will design a new customized reliable data transfer protocol, 
akin to TCP but using UDP in C/C++ programming language. This project will deepen your understanding on how TCP protocol 
works and specifically how it handles packet losses.
You will implement this protocol in context of server and client applications, 
where client transmits a file as soon as the connection is established. 
We made several simplifications to the real TCP protocol and its congestion control, especially:

* Do not need to implement checksum computation and/or verification;
* Assume there are no corruption, no reordering, and no duplication of the packets in transmit;
The only unreliability you will work on is packet loss;
* Do not need to estimate RTT, nor to update RTO using RTT estimation or using Karn’s algorithm to double it; 
Use a fixed retransmission timer value;
* Do not need to handle parallel connections; all connections are assumed sequential.

All implementations will be written in C/C++ using BSD sockets. 
You are not allowed to use any third-party libraries (like Boost.Asio or similar) other than the standard libraries 
provided by C/C++. You are allowed to use some high-level abstractions, 
including C++11 extensions, for parts that are not directly related to networking, 
such as string parsing and multi-threading. We will also accept implementations written in C. 
You are encouraged to use a version control system (i.e. Git/SVN) to track the progress of your work.

# 2 Instruction
The project contains two parts: a server and a client.

* The server opens UDP socket and implements incoming connection management from clients. 
For each of the connection, the server saves all the received data from the client in a file.
* The client opens UDP socket, implements outgoing connection management, 
and connects to the server. Once connection is established, it sends the content of a file to the server.

Both client and server must implement reliable data transfer using unreliable UDP transport, 
including data sequencing, cumulative acknowledgments, and a basic version of congestion control.

## 2.1 Basic Protocol Specification
### 2.1.1 Header Format
* You have to design your own protocol header for this project. 
It is up to you what information you want to include in the header to achieve the functionalities required, 
but you will at least need a
**Sequence Number** field, an **Acknowledgment Number** field, and **ACK** , **SYN** , and **FIN** flags.
* The header length needs to be exactly **12** bytes, while if your design does not use up all 12 bytes, 
pad zeros to make it so.

### 2.1.2 Requirements
* The maximum UDP packet size is **524** bytes including a header ( **512** bytes for the payload). 
You should not construct a UDP packet smaller than **524** bytes while the client has more data to send.
* The maximum **Sequence Number** should be **25600** and be reset to **0** whenever it reaches the maximum value.
* Packet retransmission and appropriate congestion control actions should be triggered 
when no data was acknowledged for more than **RTO = 0.5 seconds**. It is a fixed retransmission timeout, 
so you do not need to maintain and update this timer using Karn’s algorithm, nor to estimate RTT and update it.
* If you have an **ACK** field, **Acknowledgment Number** should be set to **0** if **ACK** is not set.
* **FIN** should take logically **1** byte of the data stream (same as in TCP, see examples).
* **FIN** and **FIN-ACK** packets must not carry any payload.
* You do not need to implement checksum and/or its verification in the header.

## 2.2 Server Application Specification

## 2.3 Client Application Specification

