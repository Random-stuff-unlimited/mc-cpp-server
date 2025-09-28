#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void init_queue(t_packet_queue* q)
{
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void enqueue(t_packet_queue* q, t_packet p)
{
    t_node *node = malloc(sizeof(t_node));
    node->packet = p;
    node->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;
    q->tail = node;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

t_packet dequeue(t_packet_queue* q)
{
    t_node *node;
    t_packet p;

    pthread_mutex_lock(&q->lock);
    while (!q->head)
        pthread_cond_wait(&q->cond, &q->lock);

    node = q->head;
    q->head = node->next;
    if (!q->head) q->tail = NULL;
    pthread_mutex_unlock(&q->lock);

    p = node->packet;
    free(node);
    return p;
}
