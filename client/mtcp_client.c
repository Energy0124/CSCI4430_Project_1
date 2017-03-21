#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "mtcp_client.h"
#include "mtcp_common.h"
#include <signal.h>

/* -------------------- Global Variables -------------------- */


typedef enum client_state {
    CLOSED,
    CONNECTING_START, CONNECTING_SYN_SENT, CONNECTING_SYN_ACK_RECEIVED, CONNECTED,

    DATA_TRANSMITTING_START, DATA_TRANSMITTING_DATA_SENT, DATA_TRANSMITTING_ACK_RECEIVED, DATA_TRANSMITTING_TIMEOUT,
    DISCONNECTING,
    OTHER

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

unsigned char *send_buffer;
size_t send_buffer_max_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;
size_t send_buffer_current_size = 0;
unsigned char *receive_buffer;
size_t receive_buffer_max_size = MAX_BUF_SIZE * MAX_BUF_SIZE + 1;
size_t receive_buffer_current_size = 0;

//client state
ClientState state = CLOSED;
PacketType last_packet_type = UNDEFINED;
uint32_t sequence_number = 0;
uint32_t last_received_sequence_number = 0;
uint32_t next_expected_sequence_number = 0;
size_t last_sent_data_size = 0;

int socket_file_descriptor;
struct sockaddr_in *server_address;
socklen_t address_length;

volatile bool receive_thread_should_stop = false;
volatile bool send_thread_should_stop = false;

bool app_thread_should_wake = false;
bool send_thread_should_wake = false;
int sendto_status = 0;


/*
 * append data to buffer
 * if data > max buffer size, then try to double the buffer size and append
 * otherwise just append it to the end of the current buffer data
 * then return new target buffer
 * if realloc encounter error, a NULL pointer will be returned
 */
unsigned char *enqueue_buffer(unsigned char *target_buffer, size_t *target_buffer_current_size_ptr,
                              size_t *target_buffer_max_size_ptr,
                              unsigned char *source_buffer, size_t source_buffer_size) {

    if (source_buffer_size + *target_buffer_current_size_ptr > *target_buffer_max_size_ptr) {
        *target_buffer_max_size_ptr = (source_buffer_size + *target_buffer_max_size_ptr) * 2;
        unsigned char *new_target_buffer = realloc(target_buffer, *target_buffer_max_size_ptr);
        if (new_target_buffer != NULL) {
            return new_target_buffer;
        } else {
            printf("Realloc buffer failed!\n");
            return NULL;
        }
    } else {
        memcpy(target_buffer + *target_buffer_current_size_ptr, source_buffer, source_buffer_size);
        *target_buffer_current_size_ptr += source_buffer_size;
        return target_buffer;
    }
}

/*
 * wrapper of enqueue_buffer for send buffer
 */
unsigned char *enqueue_send_buffer(unsigned char *source_buffer, size_t source_buffer_size) {
    return enqueue_buffer(send_buffer, &send_buffer_current_size, &send_buffer_max_size, source_buffer, source_buffer_size);
}

/*
 * wrapper of enqueue_buffer for receive buffer
 */
unsigned char *enqueue_receive_buffer(unsigned char *source_buffer, size_t source_buffer_size) {
    return enqueue_buffer(receive_buffer, &receive_buffer_current_size, &receive_buffer_max_size, source_buffer,
                   source_buffer_size);
}


/*
 * get the first N bytes from buffer and remove them from the buffer
 * the got buffer will then be stored in dequeued_buffer
 * it will return the new target_buffer_current_size
 */
size_t dequeue_buffer(unsigned char *target_buffer, size_t *target_buffer_current_size_ptr,
                      unsigned char *dequeued_buffer, size_t dequeued_buffer_size) {
    if (*target_buffer_current_size_ptr < dequeued_buffer_size) {
        printf("target_buffer_current_size < dequeued_buffer_size!\n");
        return 0;
    }
    memcpy(dequeued_buffer, target_buffer, dequeued_buffer_size);
    memmove(target_buffer, target_buffer + (int) dequeued_buffer_size, dequeued_buffer_size);
    return *target_buffer_current_size_ptr -= dequeued_buffer_size;
}


/*
 * wrapper of dequeue_buffer for receive buffer
 */
size_t dequeue_send_buffer(unsigned char *dequeued_buffer, size_t dequeued_buffer_size) {
    return dequeue_buffer(send_buffer, &send_buffer_current_size, dequeued_buffer, dequeued_buffer_size);
}

/*
 * wrapper of dequeue_buffer for receive buffer
 */
size_t dequeue_receive_buffer(unsigned char *dequeued_buffer, size_t dequeued_buffer_size) {
    return dequeue_buffer(receive_buffer, &receive_buffer_current_size, dequeued_buffer, dequeued_buffer_size);
}

/*
 * given a header buffer, packet type and seq
 * it will encode the header to the buffer
 * return header size
 */
int encode_header_to_packet(PacketType type, uint32_t seq, unsigned char *header_buffer) {
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
char decode_header_from_packet(PacketType *type_ptr, uint32_t *seq_ptr, unsigned char *header_buffer) {
    *type_ptr = (PacketType) (header_buffer[0] >> 4);
    header_buffer[0] = header_buffer[0] & (char) 0x0F;
    memcpy(seq_ptr, header_buffer, 4);
    *seq_ptr = ntohl(*seq_ptr);
    return *type_ptr;

}

/*
 * given a packet buffer,  packet type and seq, data buffer and data size
 * it will construct the packet to the buffer
 * if data_size < 0, it will ignore the data pointer and simply construct and empty header packet
 * it will return the packet size
 */
size_t construct_packet_to_buffer(PacketType type, uint32_t seq, unsigned char *data, size_t data_size,
                                  unsigned char *packet_buffer) {
    int header_size = encode_header_to_packet(type, seq, packet_buffer);
    if (data_size > 0 || NULL != data) {
        memcpy(packet_buffer + header_size, data, data_size);
    }
    return header_size + data_size;
}

/*
 * given  packet type and seq, data buffer and data size
 * it will malloc the required memory and construct the packet to the malloc buffer
 * if data_size < 0, it will ignore the data pointer and simply construct and empty header packet
 * if packet_size pointer is given, it will set the packet size to the variable
 * otherwise it will just ignore it
 * it return the packet buffer
 */
unsigned char *construct_packet(PacketType type, uint32_t seq, unsigned char *data, size_t data_size,
                                size_t *packet_size) {
    unsigned char *packet_buffer = malloc(4 + data_size);
    if (NULL != packet_size) {
        *packet_size = construct_packet_to_buffer(type, seq, data, data_size, packet_buffer);

    } else {
        construct_packet_to_buffer(type, seq, data, data_size, packet_buffer);
    }
    return packet_buffer;
}

/*
 * malloc the max size packet buffer
 */
unsigned char *malloc_packet_buffer(size_t *packet_size) {
    *packet_size = 4 + 1000;
    unsigned char *packet_buffer = malloc(*packet_size);
    return packet_buffer;
}

/*
 * return the packet type
 */
PacketType get_packet_type(unsigned char *packet_buffer) {
    PacketType type;
    uint32_t seq;
    decode_header_from_packet(&type, &seq, packet_buffer);
    if (type >= UNDEFINED) {
        return UNDEFINED;
    }
    return type;
}

/*
 * return the packet seq
 */
uint32_t get_packet_seq(unsigned char *packet_buffer) {
    PacketType type;
    uint32_t seq;
    decode_header_from_packet(&type, &seq, packet_buffer);
    return seq;
}
/*
 * return the packet data
 */
unsigned char *get_packet_data(unsigned char *packet_buffer) {
    return packet_buffer + 4;
}

/*
 * return the packet data size
 */
size_t get_packet_data_size(ssize_t packet_len) {
    return (size_t) (packet_len - 4);
}

/*
 * change client state with mutex
 */
void change_state(ClientState clientState) {
    pthread_mutex_lock(&info_mutex);
    state = clientState;
    pthread_mutex_unlock(&info_mutex);
    switch (state) {

        case CLOSED:
            break;
        case CONNECTING_START:
            printf("CONNECTING_START\n");
            break;
        case CONNECTING_SYN_SENT:
            printf("CONNECTING_SYN_SENT\n");
            break;
        case CONNECTING_SYN_ACK_RECEIVED:
            printf("CONNECTING_SYN_ACK_RECEIVED\n");
            break;
        case CONNECTED:
            printf("CONNECTED\n");
            break;
        case DATA_TRANSMITTING_START:
            printf("DATA_TRANSMITTING_START\n");
            break;
        case DATA_TRANSMITTING_DATA_SENT:
            printf("DATA_TRANSMITTING_DATA_SENT\n");
            break;
        case DATA_TRANSMITTING_ACK_RECEIVED:
            printf("DATA_TRANSMITTING_ACK_RECEIVED\n");
            break;
        case DATA_TRANSMITTING_TIMEOUT:
            printf("DATA_TRANSMITTING_TIMEOUT\n");
            break;
        case DISCONNECTING:
            break;
        case OTHER:
            break;

    }
}

static void *send_thread() {

    do {
        //wait 1 sec at most
        //ref: http://stackoverflow.com/questions/1486833/pthread-cond-timedwait-help
        int timeInMs = 1000;
        struct timeval tv;
        struct timespec ts;
        gettimeofday(&tv, NULL);
        ts.tv_sec = time(NULL) + timeInMs / 1000;
        ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeInMs % 1000);
        ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec %= (1000 * 1000 * 1000);
//        pthread_mutex_lock(&send_thread_sig_mutex);
        pthread_cond_timedwait(&send_thread_sig, &send_thread_sig_mutex, &ts);
//        pthread_mutex_unlock(&send_thread_sig_mutex);

        switch (state) {
            case CLOSED:
                break;
            case CONNECTING_START: {
                size_t packet_size;
                unsigned char *buff = construct_packet(SYN, sequence_number, NULL, 0, &packet_size);

/*                 struct sockaddr_in server_addr;

               // configure settings in address struct
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                server_addr.sin_port = htons(12333);

                // initialize size variable which is used later
                socklen_t addrLen = sizeof(server_addr);*/

                ssize_t len;
                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) server_address,
                                  address_length)) <= 0) {
//                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) &server_addr,
//                                  addrLen)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }
                free(buff);
                printf("SYN packet #%d sent\n", sequence_number);
                next_expected_sequence_number = sequence_number + 1;
                change_state(CONNECTING_SYN_SENT);
            }
                break;
            case CONNECTING_SYN_SENT:
                pthread_mutex_lock(&send_thread_sig_mutex);
                while (!send_thread_should_wake) {
                    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
                }
                send_thread_should_wake = false;
                pthread_mutex_unlock(&send_thread_sig_mutex);
                break;
            case CONNECTING_SYN_ACK_RECEIVED: {
                size_t packet_size;
                unsigned char *buff = construct_packet(ACK, sequence_number, NULL, 0, &packet_size);
                ssize_t len;
                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) server_address,
                                  address_length)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }
                free(buff);
                printf("ACK packet #%d sent\n", sequence_number);
                next_expected_sequence_number = sequence_number + 1;
                change_state(CONNECTED);
                pthread_mutex_trylock(&app_thread_sig_mutex);
                app_thread_should_wake = true;
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&app_thread_sig_mutex);
            }
                break;
            case CONNECTED:

/*
                pthread_mutex_lock(&send_thread_sig_mutex);
                while (!send_thread_should_wake) {
                    pthread_cond_wait(&send_thread_sig, &send_thread_sig_mutex);
                }
                send_thread_should_wake = false;
                pthread_mutex_unlock(&send_thread_sig_mutex);*/
                break;
            case DATA_TRANSMITTING_START: {
                unsigned char *data;
                size_t data_size;
                if (send_buffer_current_size > 1000) {
                    data_size = 1000;
                } else if (send_buffer_current_size >= 0) {
                    data_size = send_buffer_current_size;
                } else {
                    break;
                }
                data = send_buffer;

                size_t packet_size;
                unsigned char *buff = construct_packet(ACK, sequence_number, data, data_size, &packet_size);
                ssize_t len;
                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) server_address,
                                  address_length)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    free(buff);
                    break;
                }
                free(buff);
                printf("DATA packet #%d sent\n", sequence_number);
                last_sent_data_size = data_size;
                next_expected_sequence_number = (uint32_t) (sequence_number + data_size);
                change_state(DATA_TRANSMITTING_DATA_SENT);
                pthread_mutex_trylock(&app_thread_sig_mutex);
                app_thread_should_wake = true;
                pthread_cond_signal(&app_thread_sig);
                pthread_mutex_unlock(&app_thread_sig_mutex);
            }
                break;
            case DATA_TRANSMITTING_DATA_SENT: {
                unsigned char *data;
                size_t data_size;
                if (send_buffer_current_size > 1000) {
                    data_size = 1000;
                } else if (send_buffer_current_size >= 0) {
                    data_size = send_buffer_current_size;
                } else {
                    break;
                }
                data = send_buffer;
                size_t packet_size;
                unsigned char *buff = construct_packet(ACK, sequence_number, data, data_size, &packet_size);
                ssize_t len;
                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) server_address,
                                  address_length)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    free(buff);
                    break;
                }
                free(buff);
                printf("DATA packet #%d sent\n", sequence_number);
                last_sent_data_size = data_size;
                next_expected_sequence_number = (uint32_t) (sequence_number + data_size);
                change_state(DATA_TRANSMITTING_DATA_SENT);
            }
                break;
            case DATA_TRANSMITTING_ACK_RECEIVED: {
                unsigned char *data;
                size_t data_size;
                if (send_buffer_current_size > 1000) {
                    data_size = 1000;
                } else if (send_buffer_current_size >= 0) {
                    data_size = send_buffer_current_size;
                } else {
                    change_state(CONNECTED);
                    break;
                }
                data = send_buffer;
                size_t packet_size;
                unsigned char *buff = construct_packet(ACK, sequence_number, data, data_size, &packet_size);
                ssize_t len;
                if ((len = sendto(socket_file_descriptor, buff, packet_size, 0, (struct sockaddr *) server_address,
                                  address_length)) <= 0) {
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    free(buff);
                    break;
                }
                free(buff);
                printf("DATA packet #%d sent\n", sequence_number);
                last_sent_data_size = data_size;
                next_expected_sequence_number = (uint32_t) (sequence_number + data_size);

                change_state(DATA_TRANSMITTING_DATA_SENT);
            }
                break;
            case DATA_TRANSMITTING_TIMEOUT:
                break;
            case DISCONNECTING:
                break;
            case OTHER:
                break;

        }
        if (state == OTHER) {
            break;
        }
    } while (!send_thread_should_stop);

}

static void *receive_thread() {
    pthread_mutex_trylock(&app_thread_sig_mutex);
    pthread_mutex_trylock(&send_thread_sig_mutex);
    do {
        ssize_t len;
        unsigned char packet_buffer[MAX_PACKET_SIZE];
        if ((len = recvfrom(socket_file_descriptor, packet_buffer, MAX_PACKET_SIZE, 0, NULL, NULL)) < 0) {
            printf("Recv Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        } else {
            switch (state) {

                case CLOSED:
                    break;
                case CONNECTING_START:
                    break;
                case CONNECTING_SYN_SENT: {
//                    ssize_t len;
//                    char packet_buffer[MAX_PACKET_SIZE];
//                    // receiver message from server
//                    if ((len = recvfrom(socket_file_descriptor, packet_buffer, MAX_PACKET_SIZE, 0, NULL, NULL)) < 0) {
//                        printf("Recv Error: %s (Errno:%d)\n", strerror(errno), errno);
//                        exit(0);
//                    } else {
                    pthread_mutex_trylock(&send_thread_sig_mutex);
                    last_packet_type = get_packet_type(packet_buffer);
                    last_received_sequence_number = get_packet_seq(packet_buffer);
//                    printf("%d %d %d\n", last_packet_type, last_received_sequence_number,
//                           next_expected_sequence_number);
                    if (last_packet_type == SYN_ACK && last_received_sequence_number == next_expected_sequence_number) {
                        printf("SYN_ACK packet #%d received\n", last_received_sequence_number);
                        change_state(CONNECTING_SYN_ACK_RECEIVED);
                        sequence_number++;
                        pthread_mutex_trylock(&send_thread_sig_mutex);
                        send_thread_should_wake = true;
                        pthread_cond_signal(&send_thread_sig);
                        pthread_mutex_unlock(&send_thread_sig_mutex);
                    } else {
                        printf("Type [%d] packet received\n", last_packet_type);
                    }
//                    }
                }
                    break;
                case CONNECTING_SYN_ACK_RECEIVED:
                    break;
                case CONNECTED:
                    break;
                case DATA_TRANSMITTING_START:
                    break;
                case DATA_TRANSMITTING_DATA_SENT:
                    pthread_mutex_trylock(&send_thread_sig_mutex);
                    last_packet_type = get_packet_type(packet_buffer);
                    last_received_sequence_number = get_packet_seq(packet_buffer);
//                    printf("%d %d %d\n", last_packet_type, last_received_sequence_number,
//                           next_expected_sequence_number);
                    if (last_packet_type == DATA && last_received_sequence_number == next_expected_sequence_number) {
                        printf("DATA packet #%d received\n", last_received_sequence_number);
                        change_state(DATA_TRANSMITTING_ACK_RECEIVED);
                        sequence_number = last_received_sequence_number + 1;
                        pthread_mutex_trylock(&send_thread_sig_mutex);
                        send_thread_should_wake = true;
                        pthread_cond_signal(&send_thread_sig);
                        pthread_mutex_unlock(&send_thread_sig_mutex);
                    } else {
                        printf("Type [%d] packet received\n", last_packet_type);
                    }
                    break;
                case DATA_TRANSMITTING_ACK_RECEIVED:
                    break;
                case DATA_TRANSMITTING_TIMEOUT:
                    break;
                case DISCONNECTING:
                    break;
                case OTHER:
                    break;

            }
        }
        if (state == OTHER) {
            break;
        }
    } while (!receive_thread_should_stop);

}

/* Connect Function Call (mtcp Version) */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr) {
    socket_file_descriptor = socket_fd;
    struct timeval tv;
    tv.tv_sec = 1;  /* 1 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
    setsockopt(socket_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(struct timeval));
    server_address = malloc(sizeof(struct sockaddr_in));
    memcpy(server_address, server_addr, sizeof(struct sockaddr_in));
    // initialize size variable which is used later
    address_length = sizeof(struct sockaddr_in);
    //create a large enough init buffer
    send_buffer = malloc(send_buffer_max_size);
    receive_buffer = malloc(receive_buffer_max_size);
    change_state(CONNECTING_START);

    //create the two thread
    pthread_create(&recv_thread_pid, NULL, (void *(*)(void *)) receive_thread, NULL);
    pthread_create(&send_thread_pid, NULL, (void *(*)(void *)) send_thread, NULL);
    //sleep(1);
    struct timespec ts;
    ts.tv_nsec = 100000000;
    ts.tv_sec = 0;
    nanosleep(&ts, NULL);

    pthread_mutex_lock(&app_thread_sig_mutex);
    while (!app_thread_should_wake) {
        pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);
        app_thread_should_wake = false;
    }
    pthread_mutex_unlock(&app_thread_sig_mutex);

    printf("Successfully connected to server\n");


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
    socket_file_descriptor = socket_fd;
    enqueue_send_buffer(buf, (size_t) buf_len);
    if (state != CONNECTED &&
        state != DATA_TRANSMITTING_START &&
        state != DATA_TRANSMITTING_DATA_SENT &&
        state != DATA_TRANSMITTING_ACK_RECEIVED &&
        state != DATA_TRANSMITTING_TIMEOUT) {
        return -1;
    }
    change_state(DATA_TRANSMITTING_START);

    pthread_mutex_trylock(&send_thread_sig_mutex);
    send_thread_should_wake = true;
    pthread_cond_signal(&send_thread_sig);
    pthread_mutex_unlock(&send_thread_sig_mutex);

    struct timespec ts;
    ts.tv_nsec = 100000000;
    ts.tv_sec = 0;
    nanosleep(&ts, NULL);

    pthread_mutex_lock(&app_thread_sig_mutex);
    while (!app_thread_should_wake) {
        pthread_cond_wait(&app_thread_sig, &app_thread_sig_mutex);
        app_thread_should_wake = false;
    }
    pthread_mutex_unlock(&app_thread_sig_mutex);

    if (sendto_status < 0) {
        return sendto_status;
    }
    printf("Successfully write to server\n");

    return buf_len;
}

/* Close Function Call (mtcp Version) */
void mtcp_close(int socket_fd) {

}

