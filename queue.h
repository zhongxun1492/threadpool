/* =============================================================================
* Filename : queue.h
* Summary  : Provide a thread-safe implementation of queue along with queue.c.
*			 This file mainly consists of the definition of struct and the 
*			 declaration of function and both of them are used in queue.c.
* Compiler : gcc
*
* Version  : 
* Update   : 
* Date     : 
* Author   : 
* Org      : 
*
* History  :
*
* ============================================================================*/

#ifndef __QUEUE_H__
#define __QUEUE_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "api.h"
#include "log.h"
#include "memory.h"

/*==============================================================================
* Struct definition
*=============================================================================*/
/* declaration of quit handler function */
typedef void (*quit_handler)(void* data);

/* single node in the queue */
typedef struct queue_item {
	struct queue_item *		last;		/* pointer of last item 			*/
	struct queue_item *		next;		/* pointer of next item 			*/
	void  *					data;		/* pointer of real data 			*/
	quit_handler 			quit_func;	/* quit handler function 			*/
} QItem;

/* struct of a queue */
typedef struct queue {
	struct queue_item *		head; 		/* point to the head item 			*/
	struct queue_item *		tail; 		/* point to the tail item 			*/
	pthread_mutex_t 		mutex; 		/* mutex used to ensure thread-safe */
	uint16_t 				max_num; 	/* maximum number of queue:0~65535	*/
	uint16_t 				cur_num; 	/* current number of queue:0~65535	*/
} Queue;

/*==============================================================================
* Function declaration
*=============================================================================*/
extern Queue* Queue_new(uint16_t max_num);
extern void Queue_add(Queue * queue, quit_handler quit_func, void * data);
extern void * Queue_pop(Queue* queue);
extern void Queue_del(Queue* queue, void* data);
extern void Queue_free(Queue** queue);
extern int8_t Queue_isempty(Queue* queue);
extern uint16_t Queue_num(Queue* queue);




#endif

