#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "mtcp_client.h"

/* -------------------- Global Variables -------------------- */

/* ThreadID for Sending Thread and Receiving Thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr){

}

/* Write Function Call (mtcp Version) */
int mtcp_write(int socket_fd, unsigned char *buf, int buf_len){

}

/* Close Function Call (mtcp Version) */
void mtcp_close(int socket_fd){

}

static void *send_thread(){

}

static void *receive_thread(){

}