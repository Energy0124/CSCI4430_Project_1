#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>	// for inet_pton()
#include <errno.h>	// for "errno"
#include <string.h>	// for strerror()

#include <mtcp_client.h>
#include <mtcp_common.h>

unsigned char global_send_buf[MAX_BUF_SIZE];
unsigned char global_recv_buf[MAX_BUF_SIZE];

int main(int argc, char **argv){
	int file_fd;			// input file.
	int sock_fd;			// client socket.
	struct sockaddr_in server_addr;	// addr structure for UDP
	char buf[MAX_BUF_SIZE];		// local buffer
	int read_rtn;

	// Check the number of run-time argument
	if(argc != 3){
		fprintf(stderr, "Usage: %s [server address] [input filename]\n", argv[0]);
		exit(1);
	}

	// Open the file
	if( (file_fd = open(argv[2], O_RDONLY)) == -1){
		fprintf(stderr, "%s (line %d): %s - open():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: %s\n", strerror(errno));
		exit(1);
	}

    // Initialize a UDP socket
	if( (sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "%s (line %d): %s - socket():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: %s\n", strerror(errno));
		exit(1);
	}

	// Configure the UDP socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	if(inet_pton(AF_INET, (const char*) argv[1], &server_addr.sin_addr) == 0) {
		fprintf(stderr, "%s (line %d): %s - inet_pton():\n", __FILE__, __LINE__, __FUNCTION__);
		fprintf(stderr, "\tError message: Wrong address format\n");
		exit(1);
	}

    // Connect to the server with mTCP protocol
	mtcp_connect(sock_fd, &server_addr);

	// Read the file
	fflush(stdout);
	while( (read_rtn = read(file_fd, buf, MAX_BUF_SIZE)) > 0){
	    // Send the read data through mTCP protocol
		mtcp_write(sock_fd, (unsigned char *) buf, read_rtn);

	}
	printf("done\n\n");

    // Close the UDP socket
	mtcp_close(sock_fd);

	// Close the file
	close(file_fd);

	return 0;
}
