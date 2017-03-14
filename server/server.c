#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>	// errno
#include <string.h>	// for strerror()
#include <arpa/inet.h>	// for inet_pton()

#include <mtcp_server.h>
#include <mtcp_common.h>

unsigned char global_send_buf[MAX_BUF_SIZE];
unsigned char global_recv_buf[MAX_BUF_SIZE];

int main(int argc, char **argv){
	int file_fd;			// output file.
	int sock_fd;			// server socket.
	struct sockaddr_in server_addr;	// addr structure for server
	struct sockaddr_in client_addr;	// addr structure for client
	char buf[MAX_BUF_SIZE];		// local buffer.
	int read_rtn;
	char filename[20];

	// Check the number of run-time argument
	if(argc != 3){
		fprintf(stderr, "Usage: %s [server address] [output filename]\n", argv[0]);
		exit(1);
	}

	// Initialize a UDP socket
	if( (sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		fprintf(stderr, "%s (line %d): %s - socket():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: %s\n", strerror(errno));
		exit(1);
	}

	// Configure the UDP socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	if( inet_pton(	AF_INET, (const char*) argv[1], &server_addr.sin_addr) == 0){
		fprintf(stderr, "%s (line %d): %s - inet_pton():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: Wrong address format\n");
		exit(1);
	}
	
	if( bind(sock_fd,(struct sockaddr*) &server_addr,sizeof(server_addr)) < 0 ){
		fprintf(stderr, "%s (line %d): %s - bind():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tBind Socket Error\n");
        close(sock_fd);
		exit(1);
    }

    // Connect to the server with mTCP protocol
	mtcp_accept(sock_fd, &client_addr);

	fflush(stdout);
	if( (file_fd = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1){
		fprintf(stderr, "%s (line %d): %s - open():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: %s\n", strerror(errno));
		exit(1);
	}

	while(1){
		// Receive data through mTCP protocol and write it to file
		if(mtcp_read(sock_fd, (unsigned char*) buf, MAX_BUF_SIZE) <= 0)
			break;
			
		// Write the file
		write(file_fd, buf, read_rtn);
	}
	
	// Close the mTCP connection
	mtcp_close(sock_fd);

	// Close the UDP socket
	close(file_fd);
	return 0;
}

