#include "offload_socket.h"
#include "offload_hmd.h"
#include "offload_protocol.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <queue>
#include <map>
#include <mutex>

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
#define CONNECTION_CHECK(status) if (status <= 0)           \
    {                                                       \
        LOG("Device disconntected, status: %ld", status);   \
        return;                                             \
    }

#ifndef NDEBUG
#define LOG(msg, ...) do {                  \
        printf(msg "\n", __VA_ARGS__);      \
    } while(0)
#else
#define LOG(msg)
#endif

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))


std::map<int, std::queue<message_packet_t>> rx_queues;
std::queue<message_packet_t> tx_queue;
std::mutex lock;
bool deviceConnected = false;

void tx_enqueue(message_packet_t* packet)
{
    if (!deviceConnected)
        return;
    std::lock_guard<std::mutex> guard(lock);
    tx_queue.push(*packet);
}

bool tx_dequeue(message_packet_t* packet)
{
    std::lock_guard<std::mutex> guard(lock);
    bool isEmpty = tx_queue.empty();
    if (!isEmpty)
    {
        *packet = tx_queue.front();
        tx_queue.pop();
    }
    return !isEmpty;
}

void rx_enqueue(message_packet_t* packet)
{
    std::lock_guard<std::mutex> guard(lock);
    rx_queues[packet->header.command].push(*packet);
}

bool rx_dequeue(message_packet_t* packet, enum socket_protocol protocol)
{
    std::lock_guard<std::mutex> guard(lock);
    bool isEmpty = rx_queues[protocol].empty();
    if (!isEmpty)
    {
        *packet = rx_queues[protocol].front();
        rx_queues[protocol].pop();
    }
    return !isEmpty;
}

char* allocate_payload(unsigned int size)
{
    return (char*)malloc(size);
}

void free_payload(char* payload)
{
    free(payload);
}

void process_device(struct offload_hmd *hmd, int new_socket);

void *socket_thread(void* arg)
{
    struct offload_hmd* hmd = (struct offload_hmd*) arg;

    int server_fd, new_socket;
    int opt = 1;
 
    // Creating socket file descriptor
    SOCKET_CHECK((server_fd = socket(AF_INET, SOCK_STREAM, 0)));
 
    // Forcefully attaching socket to the port
    SOCKET_CHECK(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;
    
    socklen_t addrlen = sizeof(address);
 
    // Forcefully attaching socket to the port
    SOCKET_CHECK(bind(server_fd, (struct sockaddr*)&address, sizeof(address)));
    SOCKET_CHECK(listen(server_fd, 3))

    // Loop waiting for one device connection
    while ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen))
        && !hmd->stop)
    {
        deviceConnected =true;
        process_device(hmd, new_socket);
        deviceConnected = false;
        rx_queues.clear();
        tx_queue = std::queue<message_packet_t>();
    }

    pthread_exit(NULL);
}

void process_device(struct offload_hmd *hmd, int new_socket)
{
    SOCKET_CHECK(new_socket);
    LOG("New device connected with ID: %d", new_socket);
    message_packet_t packet;

    // # send firesim step 
    packet.header.command = CS_DEFINE_STEP;
    packet.header.payload_size = sizeof(int);
    packet.payload = (char*) &SIM_STEP_SIZE;
    tx_enqueue(&packet);

    // # grant token to RoseBridge 
    packet.header.command = CS_GRANT_TOKEN;
    packet.header.payload_size = 0;
    tx_enqueue(&packet);

    while (!hmd->stop)
    {
        // Handle TX
        if (tx_dequeue(&packet))
        {
            ssize_t size_sent = send(new_socket, &packet, sizeof(header_t), 0);
            CONNECTION_CHECK(size_sent);
            STATUS_CHECK(size_sent != sizeof(header_t), "DEBUG: failed to send header");

            if (packet.header.payload_size != 0)
            {
                size_t index = 0;
                while (index < packet.header.payload_size)
                {
                    size_t chunk_size = MIN(1024ul, packet.header.payload_size - index);
                    const char* data_ptr = packet.payload + index;
                    ssize_t bytes_sent = send(new_socket, data_ptr, chunk_size, 0);

                    CONNECTION_CHECK(bytes_sent);
                    index += bytes_sent;
                }
            }
            LOG("DEBUG: [offload_socket.c] OUT cmd[%d] and size %d",
                packet.header.command, packet.header.payload_size);
        }

        // Handle RX
        ssize_t connect_status;
        while ((connect_status = recv(new_socket, &packet.header, sizeof(header_t), MSG_PEEK | MSG_DONTWAIT)) > 0)
        {
            LOG("DEBUG: [offload_socket.c] IN  cmd[%d] and size %d",
                packet.header.command, packet.header.payload_size);

            ssize_t received = recv(new_socket, &packet.header, sizeof(header_t), 0);
            CONNECTION_CHECK(received);
            STATUS_CHECK(received != sizeof(header_t), "Error receiving header");

            if (packet.header.payload_size != 0)
            {
                int payload_size = packet.header.payload_size;
                packet.payload = (char *) malloc(payload_size);
                STATUS_CHECK(packet.payload == NULL, "Error allocating memory for message data");
            
                int bytes_received = 0;
                while (bytes_received < payload_size)
                {
                    size_t chunk_size = MIN(1024, payload_size - bytes_received);
                    char* data_ptr = packet.payload + bytes_received;
                    ssize_t chunk_bytes_received = recv(new_socket, data_ptr, chunk_size, 0);

                    CONNECTION_CHECK(chunk_bytes_received);
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
        if (connect_status == 0) {
            LOG("Device disconntected, status: %ld", connect_status);
            return;
        }
    }
    LOG("Device disconnected with ID: %d", new_socket);
}