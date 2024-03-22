#include "offload_socket.h"
#include "offload_hmd.h"
#include "offload_protocol.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "queue.h"

#define SOCKET_CHECK(status) if (status < 0)    \
    {                                           \
        perror("Failed: " #status "\n");        \
        exit(EXIT_FAILURE);                     \
    }

#define STATUS_CHECK(status, msg) if (status)   \
    {                                           \
        perror("ERROR: " #msg "\n");            \
        exit(EXIT_FAILURE);                     \
    }


#ifndef NDEBUG
#define LOG(msg, ...) do {                  \
        printf(msg "\n", __VA_ARGS__);      \
    } while(0)
#else
#define LOG(msg)
#endif

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

message_queue_t tx_queue = {
    .frontIdx = 0,
    .rearIdx = QUEUE_SIZE - 1,
    .size = 0,
    .capacity = QUEUE_SIZE
};

message_queue_t rx_queue = {
    .frontIdx = 0,
    .rearIdx = QUEUE_SIZE - 1,
    .size = 0,
    .capacity = QUEUE_SIZE
};

pthread_mutex_t lock;

void tx_enqueue(message_packet_t* packet)
{
    pthread_mutex_lock(&lock);
    STATUS_CHECK(false == enqueue(&tx_queue, packet),
        "Failed to enqueue message: queue is full");
    pthread_mutex_unlock(&lock);
}

bool tx_dequeue(message_packet_t* packet)
{
    pthread_mutex_lock(&lock);
    if (dequeue(&tx_queue, packet))
    {
        pthread_mutex_unlock(&lock);
        return true;
    }
    pthread_mutex_unlock(&lock);
    return false;
}

void rx_enqueue(message_packet_t* packet)
{
    pthread_mutex_lock(&lock);
    STATUS_CHECK(false == enqueue(&rx_queue, packet),
        "Failed to enqueue message: queue is full");
    pthread_mutex_unlock(&lock);
}

bool rx_dequeue(message_packet_t* packet, enum socket_protocol protocol)
{
    pthread_mutex_lock(&lock);
    if (front(&rx_queue, packet) && packet->header.command == protocol)
    {
        dequeue(&rx_queue, packet);
        pthread_mutex_unlock(&lock);
        return true;
    }
    pthread_mutex_unlock(&lock);
    return false;
}

void *socket_thread(void* arg)
{
    struct offload_hmd* hmd = (struct offload_hmd*) arg;

    pthread_mutex_init(&lock, NULL);

    int server_fd, new_socket;
    int opt = 1;
 
    // Creating socket file descriptor
    SOCKET_CHECK((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0);
 
    // Forcefully attaching socket to the port
    SOCKET_CHECK(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT),
    };
    socklen_t addrlen = sizeof(address);
 
    // Forcefully attaching socket to the port
    SOCKET_CHECK(bind(server_fd, (struct sockaddr*)&address, sizeof(address)));
    SOCKET_CHECK(listen(server_fd, 3) < 0)
    SOCKET_CHECK((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0);

    message_packet_t packet;
    while (!hmd->stop)
    {
        // Handle TX
        if (tx_dequeue(&packet))
        {
            size_t size_sent = send(new_socket, &packet, sizeof(header_t), 0);
            STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

            if (packet.header.payload_size != 0)
            {
                size_t index = 0;
                while (index < packet.header.payload_size)
                {
                    size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
                    const char* data_ptr = packet.payload + index;
                    ssize_t bytes_sent = send(new_socket, data_ptr, chunk_size, 0);
    
                    STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
                    index += bytes_sent;
                }
            }
            LOG("DEBUG: [offload_socket.c] OUT cmd[%d] and size %d",
                packet.header.command, packet.header.payload_size);
        }

        // Handle RX
        while (recv(new_socket, &packet.header, sizeof(header_t), MSG_PEEK | MSG_DONTWAIT) > 0)
        {
            LOG("DEBUG: [offload_socket.c] IN  cmd[%d] and size %d",
                packet.header.command, packet.header.payload_size);

            ssize_t received = recv(new_socket, &packet.header, sizeof(header_t), 0);
            STATUS_CHECK(received != sizeof(header_t), "Error receiving header");

            if (packet.header.payload_size != 0)
            {
                int payload_size = packet.header.payload_size;
                packet.payload = (char *) malloc(payload_size);
                STATUS_CHECK(packet.payload == NULL, "Error allocating memory for message data");
            
                size_t bytes_received = 0;
                while (bytes_received < payload_size)
                {
                    size_t chunk_size = MIN(1024ul, payload_size - bytes_received);
                    char* data_ptr = packet.payload + bytes_received;
                    ssize_t chunk_bytes_received = recv(new_socket, data_ptr, chunk_size, 0);

                    STATUS_CHECK(chunk_bytes_received == -1, "Error receiving message data");
                    bytes_received += chunk_bytes_received;
                }
            }

            if (packet.header.command == CS_RSP_STALL)
            {
                LOG("DEBUG: [offload_socket.c] Hit stall, exiting, cmd[%d]", packet.header.command);
                exit(1);
            }

#ifndef NDEBUG
            if (packet.header.command == CS_RSP_POSE)
            {
                float* buf = (float*)packet.payload;
                LOG("DEBUG: [offload_socket.c] cmd[%d]: pose.translation <%f, %f, %f>",
                    packet.header.command, buf[4], buf[5], buf[6]);
            }
#endif
            rx_enqueue(&packet);
        }
    }
}