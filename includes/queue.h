#ifndef QUEUE_H
# define QUEUE_H

# include "struct.h"

int	init_queue(t_packet_queue* q);
void enqueue(t_packet_queue* q, t_packet p);
t_packet dequeue(t_packet_queue* q);

#endif
