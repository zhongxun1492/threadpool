/* =============================================================================
* Filename : queue.c
* Summary  : Provide a thread-safe implementation of queue along with queue.h.
*			 This file mainly consists of the definition of functions which are 
*			 declared in queue.h.
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

#include "queue.h"

static void Queue_clear(Queue* queue);

Queue* Queue_new(uint16_t max_num)
{
	Queue *	new_queue;

	new_queue = NULL;

	new_queue = (Queue*)safe_malloc(sizeof(Queue));
	new_queue->head = NULL;
	new_queue->tail = NULL;
	new_queue->cur_num = 0;
	new_queue->max_num = max_num;
	pthread_mutex_init(&new_queue->mutex, NULL);

	log_debug(log_cat, "queue new success");

	return new_queue;
}



void Queue_add(Queue * queue, quit_handler quit_func, void * data)
{
	QItem * new_item;
	
	new_item = NULL;

	new_item = (QItem*)safe_malloc(sizeof(QItem));
	new_item->last = NULL;
	new_item->next = NULL;
	new_item->data = data;
	new_item->quit_func = quit_func;

	pthread_mutex_lock(&queue->mutex);

	if (queue->cur_num >= queue->max_num) {
		log_warn(log_cat, "queue is full");
		safe_free((void**)(&new_item));
	} else {
		if (queue->head == NULL) {
			queue->head = queue->tail = new_item;
		} else {
			queue->tail->next = new_item;
			new_item->last = queue->tail;
			queue->tail = new_item;
		}
		queue->cur_num++;
		log_debug(log_cat, "add a new item success");
	}
	
	pthread_mutex_unlock(&queue->mutex);
}



void * Queue_pop(Queue* queue)
{
	QItem * pop_item;
	void *	data;
	
	data	 = NULL;
	pop_item = NULL;
	
	pthread_mutex_lock(&queue->mutex);

	if (queue->cur_num <= 0) {
		log_warn(log_cat, "queue is empty");
	} else {
		pop_item = queue->head;
		queue->head = pop_item->next;
		if (queue->head == NULL) {
			queue->tail = NULL;
		} else {
			queue->head->last = NULL;
		}
		queue->cur_num--;
		data = pop_item->data;
		safe_free((void**)(&pop_item));
		log_debug(log_cat, "pop head item success");
	}
	
	pthread_mutex_unlock(&queue->mutex);

	return data;
}



void Queue_del(Queue* queue, void* data)
{
	QItem * 	trav_item;
	uint16_t	trav_cnt;
	
	pthread_mutex_lock(&queue->mutex);

	trav_item = queue->head;
	trav_cnt = 1;

	while (trav_item) {

		if (trav_item->data == data) {

			if (trav_cnt == 1) {
				if (queue->cur_num == 1) {
					queue->head = NULL;
					queue->tail = NULL;
				} else {
					queue->head = trav_item->next;
					queue->head->last = NULL;
				}
			} else if (trav_cnt == queue->cur_num) {
				queue->tail = trav_item->last;
				queue->tail->next = NULL;
			} else {
				trav_item->last->next = trav_item->next;
				trav_item->next->last = trav_item->last;
			}

			trav_item->last = trav_item->next = NULL;

			if (trav_item->quit_func != NULL) {
				trav_item->quit_func(trav_item->data);
			}
			safe_free((void**)(&trav_item));
			queue->cur_num--;

			log_debug(log_cat, "delete the item success");
			
			break;
		}
		
		trav_item = trav_item->next;
		
		trav_cnt++;
	}
	
	pthread_mutex_unlock(&queue->mutex);
}



static void Queue_clear(Queue* queue)
{
	QItem * 	sig_item;

	sig_item = NULL;

	pthread_mutex_lock(&queue->mutex);
	
	while ((sig_item = queue->head)) {
		
		queue->head = sig_item->next;
		
		if (queue->head != NULL) {
			queue->head->last = NULL;
		} else {
			queue->tail = NULL;
		}
		
		sig_item->last = sig_item->next = NULL;
		
		if (sig_item->quit_func != NULL) {
			sig_item->quit_func(sig_item->data);
		}
		
		safe_free((void**)(&sig_item));
		
		queue->cur_num--;
	}
	
	pthread_mutex_unlock(&queue->mutex);

	log_debug(log_cat, "queue clear success");
}



void Queue_free(Queue** queue)
{
	Queue_clear(*queue);

	pthread_mutex_destroy(&((*queue)->mutex));

	safe_free((void**)queue);

	log_debug(log_cat, "queue free success");
}



int8_t Queue_isempty(Queue* queue)
{
	return (queue->cur_num ? FALSE : TRUE);
}



uint16_t Queue_num(Queue* queue)
{
	return queue->cur_num;
}



