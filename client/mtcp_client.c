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
    CLOSED,
    CONNECTING_START, CONNECTING_SYN_SENT, CONNECTING_SYN_ACK_RECEIVED, CONNECTED,

    DATA_TRANSMITTING, DISCONNECTING,

} ClientState;

typedef enum packet_type {
    SYN, SYN_ACK, FIN, FIN_ACK, ACK, DATA, UNDEFINED
} PacketType;

/* ThreadID for Sending Thread and Receiving Thread */
static pthread_t send_thread_pid;
static pthread_t recv_thread_pid;

static pthread_cond_t app_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t app_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t send_thread_sig = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t send_thread_sig_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t info_mutex = PTHREAD_MUTEX_INITIALIZER;

char *send_buffer;
size_t send_buffer_max_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;
size_t send_buffer_current_size = 0;
char *receive_buffer;
size_t receive_buffer_max_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;
size_t receive_buffer_current_size = 0;

//client state
ClientState state = CLOSED;
PacketType lastPacketType = UNDEFINED;
int sequence_number = 0;


/*
 * append data to buffer
 * if data > max buffer size, then try to double the buffer size and append
 * otherwise just append it to the end of the current buffer data
 * then return new target buffer
 * if realloc encounter error, a NULL pointer will be returned
 */
char *enqueue_buffer(char *target_buffer, size_t *target_buffer_current_size_ptr, size_t *target_buffer_max_size_ptr,
                     char *source_buffer, size_t source_buffer_size) {

    if (source_buffer_size + *target_buffer_current_size_ptr > *target_buffer_max_size_ptr) {
        char *new_target_buffer = realloc(target_buffer,
                                          (size_t) (source_buffer_size + *target_buffer_max_size_ptr) * 2);
        if (new_target_buffer != NULL) {
            return new_target_buffer;
        } else {
            printf("Realloc buffer failed!\n");
            return NULL;
        }
    } else {
        memcpy(target_buffer + *target_buffer_current_size_ptr, source_buffer, source_buffer_size);
        return target_buffer;
    }
}

/*
 * wrapper of enqueue_buffer for send buffer
 */
char *enqueue_send_buffer(char *source_buffer, size_t source_buffer_size) {
    enqueue_buffer(send_buffer, &send_buffer_current_size, &send_buffer_max_size, source_buffer, source_buffer_size);
}

/*
 * wrapper of enqueue_buffer for receive buffer
 */
char *enqueue_receive_buffer(char *source_buffer, size_t source_buffer_size) {
    enqueue_buffer(receive_buffer, &receive_buffer_current_size, &receive_buffer_max_size, source_buffer,
                   source_buffer_size);
}


/*
 * get the first N bytes from buffer and remove them from the buffer
 * the got buffer will then be stored in dequeued_buffer
 * it will return the new target_buffer_current_size
 */
size_t dequeue_buffer(char *target_buffer, size_t *target_buffer_current_size_ptr,
                      char *dequeued_buffer, size_t dequeued_buffer_size) {
    if (*target_buffer_current_size_ptr < dequeued_buffer_size) {
        printf("target_buffer_current_size < dequeued_buffer_size!\n");
        return NULL;
    }
    memcpy(dequeued_buffer, target_buffer, dequeued_buffer_size);
    memmove(target_buffer, target_buffer + (int) dequeued_buffer_size, dequeued_buffer_size);
    return *target_buffer_current_size_ptr -= dequeued_buffer_size;
}


/*
 * wrapper of dequeue_buffer for receive buffer
 */
size_t dequeue_send_buffer(char *dequeued_buffer, size_t dequeued_buffer_size) {
    dequeue_buffer(send_buffer, &send_buffer_current_size, dequeued_buffer, dequeued_buffer_size);
}

/*
 * wrapper of dequeue_buffer for receive buffer
 */
size_t dequeue_receive_buffer(char *dequeued_buffer, size_t dequeued_buffer_size) {
    dequeue_buffer(receive_buffer, &receive_buffer_current_size, dequeued_buffer, dequeued_buffer_size);
}

/*
 * given a header buffer, packet type and seq
 * it will encode the header to the buffer
 * return header size
 */
int encode_header_to_packet(char type, uint32_t seq, char *header_buffer) {
    seq = htonl(seq);
    memcpy(header_buffer, &seq, 4);
    header_buffer[0] = header_buffer[0] | (type << 4);
    return 4;
}

/*
 * given a header buffer, and provide type and seq pointer for data return
 * it will decode the header to the buffer
 * it returns the type
 */
char decode_header_from_packet(char *type_ptr, uint32_t *seq_ptr, char *header_buffer) {
    *type_ptr = header_buffer[0] >> 4;
    header_buffer[0] = header_buffer[0] & (char) 0x0F;
    memcpy(seq_ptr, header_buffer, 4);
    *seq_ptr = ntohl(*seq_ptr);
    return *type_ptr;

}

/*
 * given a packet buffer,  packet type and seq, data buffer and data size
 * it will construct the packet to the buffer
 * and return the packet size
 */
size_t construct_packet_to_buffer(char type, uint32_t seq, char *data, size_t data_size, char *packet_buffer) {
    int header_size = encode_header_to_packet(type, seq, packet_buffer);
    memcpy(packet_buffer + header_size, data, data_size);
    return header_size + data_size;
}

/*
 * return the packet type
 */
char get_packet_type(char *packet_buffer) {
    char type;
    uint32_t seq;
    decode_header_from_packet(&type, &seq, packet_buffer);
    return type;
}

/*
 * return the packet seq
 */
uint32_t get_packet_seq(char *packet_buffer) {
    char type;
    uint32_t seq;
    decode_header_from_packet(&type, &seq, packet_buffer);
    return seq;
}


static void *send_thread() {

}

static void *receive_thread() {

}

/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr) {

    // initialize size variable which is used later
    socklen_t addrLen = sizeof(server_addr);
    //create a large enough init buffer
    send_buffer = malloc(send_buffer_max_size);
    receive_buffer = malloc(receive_buffer_max_size);
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

