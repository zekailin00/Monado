#include "offload_protocol.h"

#ifndef QUEUE_SIZE
#error "Need to define QUEUE_SIZE before including the file"
#endif
 
// A structure to represent a queue
typedef struct message_queue {
    int frontIdx;
    int rearIdx;
    int size;
    unsigned capacity;
    message_packet_t array[QUEUE_SIZE];
} message_queue_t;



// Queue is full when size becomes
// equal to the capacity
int isFull(message_queue_t* queue)
{
    return (queue->size == queue->capacity);
}
 
// Queue is empty when size is 0
int isEmpty(message_queue_t* queue)
{
    return (queue->size == 0);
}

// Function to add an item to the queue.
// It changes rear and size
bool enqueue(message_queue_t* queue, message_packet_t* item)
{
    if (isFull(queue))
        return false;
    queue->rearIdx = (queue->rearIdx + 1) % queue->capacity;
    queue->array[queue->rearIdx] = *item;
    queue->size = queue->size + 1;
    return true;
}
 
// Function to remove an item from queue.
// It changes front and size
bool dequeue(message_queue_t* queue, message_packet_t* packet)
{
    if (isEmpty(queue))
        return false;
    *packet = queue->array[queue->frontIdx];
    queue->frontIdx = (queue->frontIdx + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return true;
}
 
// Function to get front of queue
bool front(message_queue_t* queue, message_packet_t* packet)
{
    if (isEmpty(queue))
        return false;
    *packet = queue->array[queue->frontIdx];
    return true;
}
 
// Function to get rear of queue
bool rear(message_queue_t* queue, message_packet_t* packet)
{
    if (isEmpty(queue))
        return false;
    *packet = queue->array[queue->rearIdx];
    return true;
}
