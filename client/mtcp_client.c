#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include "mtcp_client.h"
#include "mtcp_common.h"

/* -------------------- Global Variables -------------------- */


typedef enum client_state {
    UNDEFINED, CONNECTING, DATA_TRANSMITTING, DISCONNECTING

} ClientState;

typedef enum packet_type {
    SYN, SYN_ACK, FIN, FIN_ACK, ACK, DATA, OTHER
} PacketType;

/* ThreadID for Sending Thread and Receiving Thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *send_buffer;
static size_t send_buffer_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;
static char *receive_buffer;
static size_t receive_buffer_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;

//client state
static ClientState state = UNDEFINED;
static PacketType lastPacketType = OTHER;
static int sequence_number = 0;


static void *send_thread() {

}

static void *receive_thread() {

}

/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr) {

    // initialize size variable which is used later
    socklen_t addrLen = sizeof(server_addr);
    //create a large enough init buffer
    send_buffer = malloc(send_buffer_size);
    receive_buffer = malloc(receive_buffer_size);
    //create the two thread
    pthread_create(&send_thread_pid, NULL, (void *(*)(void *)) send_thread, NULL);
    pthread_create(&recv_thread_pid, NULL, (void *(*)(void *)) receive_thread, NULL);


   /* char recvBuff[100];
    char *buff = "hello";

    ssize_t len;
    // send message to server
    printf("Say 'hello' to server.\n");
    if ((len = sendto(socket_fd, buff, strlen(buff), 0, (struct sockaddr *) server_addr, addrLen)) <= 0) {
        printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }

    // receiver message from server
    if ((len = recvfrom(socket_fd, recvBuff, sizeof(recvBuff) - 1, 0, NULL, NULL)) < 0) {
        printf("Recv Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    } else {
        recvBuff[len] = '\0';
        printf("Received response from server: %s\n\n", recvBuff);
    }
    sleep(1);*/


}

/* Write Function Call (mtcp Version) */
int mtcp_write(int socket_fd, unsigned char *buf, int buf_len) {

}

/* Close Function Call (mtcp Version) */
void mtcp_close(int socket_fd) {

}

