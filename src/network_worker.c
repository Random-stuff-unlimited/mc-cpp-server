#include "network_worker.h"
#include "struct.h"
#include "queue.h"
#include <stddef.h>
#include <stdio.h>

void    *network_worker(void *arg)
{
    t_server	*server = (t_server *)arg;
    while (1) {
        t_packet p = dequeue(&server->packet_queue);
        if (!p.player || !p.player->connected) continue;

        pthread_mutex_lock(&p.player->lock);
        // add packet 
        printf("Packet reçu de %s (len=%d)\n", p.player->username, p.len);
        pthread_mutex_unlock(&p.player->lock);
    }
    return NULL;
}