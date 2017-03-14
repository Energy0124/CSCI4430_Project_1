#ifndef __mtcp_SERVER__

#define __mtcp_SERVER__

void mtcp_accept(int socket_fd, struct sockaddr_in *server_addr);
int mtcp_read(int socket_fd, unsigned char *buf, int buf_len);
void mtcp_close(int socket_fd);

#endif // __mtcp_SERVER__
