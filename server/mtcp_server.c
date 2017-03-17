#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mtcp_server.h"

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

    pthread_t thread_1;
    pthread_t thread_2;
    pthread_create( &thread_1, NULL,send_thread,NULL );
    pthread_create( &thread_2, NULL,receive_thread,NULL );
}

int mtcp_read(int socket_fd, unsigned char *buf, int buf_len){

}

void mtcp_close(int socket_fd){

}

static void *send_thread(){

}

static void *receive_thread(){

}
