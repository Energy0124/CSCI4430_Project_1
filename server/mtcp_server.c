#include <netinet/in.h>
# include <netinet/udp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mtcp_server.h"
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>

/* ThreadID for Sending Thread and Receiving Thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;

struct sockaddr_in *client_addr;
int sd;

socklen_t addrLen;
char buff[1000];
/* The Sending Thread and Receive Thread Function */
static void *send_thread();
static void *receive_thread();

void mtcp_accept(int socket_fd, struct sockaddr_in *server_addr){
    // Initialize mutex for send_thread and receive_thread
    pthread_mutex_init(&send_thread_sig_mutex, NULL);
    pthread_mutex_init(&app_thread_sig_mutex, NULL);

    pthread_create( &send_thread_pid, NULL,send_thread,NULL );
    pthread_create( &recv_thread_pid, NULL,receive_thread,NULL );

    sd = socket_fd;
    client_addr = server_addr;
    addrLen =  sizeof(server_addr);
    pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);


}

int mtcp_read(int socket_fd, unsigned char *buf, int buf_len){

}

void mtcp_close(int socket_fd){

}

static void *send_thread(){
    int len;
    int ackBuff = 0;
    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);

    // send response(SYN-ACK) to the client
    if((len = sendto(sd, ackBuff, strlen(ackBuff), 0, (struct sockaddr*)&client_addr, addrLen)) <= 0) {
        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }

    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);

}

static void *receive_thread(){
    int len;
    // receive SYN from the client
    if((len = recvfrom(sd, buff, sizeof(buff), 0, (struct sockaddr*)client_addr, &addrLen)) <= 0) {
        printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
    }
    buff[len] = '\0';
    // When received SYN packet, then wake up send_thread
    pthread_cond_signal(&send_thread_sig);

    // receive ACK from the client
    if((len = recvfrom(sd, buff, sizeof(buff), 0, (struct sockaddr*)client_addr, &addrLen)) <= 0) {
        printf("receive error: %s (Errno:%d)\n", strerror(errno),errno);
    }
    pthread_cond_signal(&app_thread_sig);
}
