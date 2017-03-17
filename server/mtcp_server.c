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

/* The Sending Thread and Receive Thread Function */
static void *send_thread();
static void *receive_thread();

void mtcp_accept(int socket_fd, struct sockaddr_in *server_addr){
    // Initialize mutex for send_thread and receive_thread
    pthread_mutex_init(&send_thread_sig_mutex, NULL);
    pthread_mutex_init(&app_thread_sig_mutex, NULL);

    pthread_create( &send_thread_pid, NULL,send_thread,NULL );
    pthread_create( &recv_thread_pid, NULL,receive_thread,NULL );

    pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);


}

int mtcp_read(int socket_fd, unsigned char *buf, int buf_len){

}

void mtcp_close(int socket_fd){

}

static void *send_thread(){

    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
}

static void *receive_thread(){
    // When received SYN packet, then wake up send_thread
    pthread_cond_signal(&send_thread_sig);

    pthread_cond_signal(&app_thread_sig);
}
