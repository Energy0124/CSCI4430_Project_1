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
#include <stdbool.h>
# include <sys/types.h>


typedef enum server_state {
    CLOSED,
    CONNECTING_START, CONNECTING_SYN_ACK_SENT, CONNECTING_SYN_RECEIVED, CONNECTED,

    DATA_TRANSMITTING, DISCONNECTING,
    OTHER

} ServerState;

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

//  New variable
struct sockaddr_in *client_addr;
int sd;
ServerState state = CLOSED;
PacketType last_packet_type = UNDEFINED;

socklen_t addrLen;
char buff[1000];
/* The Sending Thread and Receive Thread Function */
static void *send_thread();
static void *receive_thread();


/*
 * change server state with mutex
 */
void change_state(ServerState serverState) {
    pthread_mutex_lock(&info_mutex);
    state = serverState;
    pthread_mutex_unlock(&info_mutex);
}

void mtcp_accept(int socket_fd, struct sockaddr_in *server_addr){
    // Initialize mutex for send_thread and receive_thread
    pthread_mutex_init(&send_thread_sig_mutex, NULL);
    pthread_mutex_init(&app_thread_sig_mutex, NULL);

    sd = socket_fd;
    client_addr = server_addr;
    addrLen =  sizeof(server_addr);

    // begin connection
    pthread_create( &send_thread_pid, NULL, (void *(*)(void *))send_thread,NULL );
    pthread_create( &recv_thread_pid, NULL,(void *(*)(void *)) receive_thread,NULL );
    change_state(CONNECTING_START);


    pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);


}

int mtcp_read(int socket_fd, unsigned char *buf, int buf_len){

}

void mtcp_close(int socket_fd){

}

int encode_header_to_packet(PacketType type, uint32_t seq, char *header_buffer) {
    seq = htonl(seq);
    memcpy(header_buffer, &seq, 4);
    header_buffer[0] = header_buffer[0] | (type << 4);
    return 4;
}


size_t construct_packet_to_buffer(PacketType type, uint32_t seq, char *data, size_t data_size, char *packet_buffer) {
    int header_size = encode_header_to_packet(type, seq, packet_buffer);
    if (data_size > 0 || NULL != data) {
        memcpy(packet_buffer + header_size, data, data_size);
    }
    return header_size + data_size;
}

char *construct_packet(PacketType type, uint32_t seq, char *data, size_t data_size, size_t *packet_size) {
    char *packet_buffer = malloc(4 + data_size);
    if (NULL != packet_size) {
        *packet_size = construct_packet_to_buffer(type, seq, data, data_size, packet_buffer);

    } else {
        construct_packet_to_buffer(type, seq, data, data_size, packet_buffer);
    }
    return packet_buffer;
}

static void *send_thread() {
    int ackBuff = 0;
    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
    do {
        switch (state) {
            case CLOSED:
                break;
            case CONNECTING_START: {
                size_t packet_size;
                char *buff = construct_packet(SYN, 0, NULL, 0, &packet_size);
                ssize_t len;
                // send response(SYN-ACK) to the client
                if ((len = sendto(sd, buff, strlen(buff), 0, (struct sockaddr *) client_addr, addrLen)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }
                change_state(CONNECTING_SYN_ACK_SENT);
                pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
            }
                break;
            case CONNECTING_SYN_ACK_SENT:
                break;
            case CONNECTING_SYN_RECEIVED:
                break;
            case CONNECTED:
                break;
            case DATA_TRANSMITTING:
                break;
            case DISCONNECTING:
                break;
            case OTHER:
                break;
        }

        if (state == OTHER) {
            break;
        }
    } while (true);

    //pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);

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
