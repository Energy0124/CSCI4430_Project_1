CSCI4430 Project 1
Group 2
1155062557 Ling Leong
1155065237 Chan Tsz Long Aaron
1155062584 Wong Yat Chun

List of files:
- client\Makefile : client makefile
- client\mtcp_client.c : client mtcp library c file
- client\mtcp_client.h : client mtcp library header file
- client\mtcp_common.h : mtcp common header file
- client\client.c : client c file
- server\Makefile : server makefile
- server\mtcp_server.c : server mtcp library c file
- server\mtcp_server.h : server mtcp library header file
- server\mtcp_common.h : mtcp common header file
- server\server.c : server c file
- README.txt : this readme

Methods of compilation:
Client:
    cd client
    make
Server:
    cd server
    make

Methods of execution:
Client:
    ./client/client [server address] [input filename]
Server:
    ./server/server [server address] [output filename]