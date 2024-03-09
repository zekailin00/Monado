#include "offload_socket.h"
#include "offload_hmd.h"
#include "offload_protocol.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080

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

// #define LOG(msg) do {           \
//         printf("%s\n", msg);    \
//     } while(0)
#define LOG(msg)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define PACKET_BUFFER_SIZE 32
message_packet_t message_packet_rx[PACKET_BUFFER_SIZE];
message_packet_t message_packet_tx[PACKET_BUFFER_SIZE];
int rx_index = 0, tx_index = 0;
pthread_mutex_t rx_lock, tx_lock;

void tx_enqueue(message_packet_t* packet)
{
    pthread_mutex_lock(&tx_lock);
    message_packet_tx[tx_index++] = *packet;
    pthread_mutex_unlock(&tx_lock);
}

bool tx_dequeue(message_packet_t* packet)
{
    pthread_mutex_lock(&tx_lock);
    if (tx_index > 0)
    {
        *packet = message_packet_tx[tx_index-1];
        tx_index--;
        pthread_mutex_unlock(&tx_lock);
        return true;
    }
    pthread_mutex_unlock(&tx_lock);
    return false;
}

void rx_enqueue(message_packet_t* packet)
{
    pthread_mutex_lock(&rx_lock);
    message_packet_rx[rx_index++] = *packet;
    pthread_mutex_unlock(&rx_lock);
}

bool rx_dequeue(message_packet_t* packet, enum socket_protocol protocol)
{
    pthread_mutex_lock(&rx_lock);

    if (rx_index > 0 && message_packet_rx[rx_index-1].header.command == protocol)
    {
        *packet = message_packet_rx[rx_index-1];
        rx_index--;
        // printf("prot: %d; pcak: %d\n", protocol, packet->header.command);
        pthread_mutex_unlock(&rx_lock);
        return true;
    }

    pthread_mutex_unlock(&rx_lock);
    return false;
}

void *socket_thread(void* arg)
{
    struct offload_hmd* hmd = (struct offload_hmd*) arg;

    pthread_mutex_init(&tx_lock, NULL);
    pthread_mutex_init(&rx_lock, NULL);

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
    
                    LOG("DEBUG: sent "); //TODO: print number
                    STATUS_CHECK(bytes_sent == -1, "DEBUG: failed to send everything");
                    index += bytes_sent;
                }
                // free(packet.payload); FIXME:
            }
        }

        // Handle RX
        while (recv(new_socket, &packet.header, sizeof(header_t), MSG_PEEK | MSG_DONTWAIT) > 0)
        {
            LOG("DEBUG: new data is arriving");

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

            rx_enqueue(&packet);
        }
    }
}