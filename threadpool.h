/* =============================================================================
* Filename : threadpool.h
* Summary  : 
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

#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

/*==============================================================================
* Header files
*=============================================================================*/
#include "api.h"
#include "log.h"
#include "memory.h"
#include "queue.h"
#include <unistd.h>

/*==============================================================================
* Macro definitions
*=============================================================================*/
#define BUSY_THRESHOLD 		(0.5)	/* busy threshold of the thread pool:
									   (busy thread) / (all thread)		  	  */
#define MANAGE_INTERVAL 	(60)	/* tp manage thread sleep interval    	  */

#define JOB_TH_CNT_MIN		2
#define JOB_TH_CNT_MAX		20


/*==============================================================================
* Struct definitions
*=============================================================================*/
/* declaration of job handler function */
typedef void (*job_handler)(void* job_arg);

/* declaration of thread work function */
typedef void *(*start_routine)(void *);

/* single thread information */
typedef struct thread_info {
	pthread_t 		th_id;			/* thread id 							  */
	pthread_cond_t 	th_cond;		/* control the thread running or sleeping */
	pthread_mutex_t th_lock;		/* control synchronization among threads  */
	BOOL			th_wait_need;	/* whether the thread need to waited	  */
	BOOL			th_exit_need;	/* whether the thread need to exit		  */
	void *			job_arg;		/* argument of the function				  */
	job_handler 	job_func;		/* function of job handling				  */
	struct thread_pool* th_pool;	/* thread pool which the thread is in	  */
} ThreadInfo;

/* definition of thread pool */
typedef struct thread_pool {
	uint16_t		min_num;			/* min thread number in the pool	  */
	uint16_t		max_num;			/* max thread number in the pool	  */
	pthread_mutex_t pool_lock;			/* mutex of whole thread pool		  */
	pthread_cond_t 	pool_cond;			/* cond of whole thread pool 		  */
	BOOL			pool_wait_need;		/* whether the pool need to waited	  */
	pthread_mutex_t stop_lock;			/* stop mutex of whole thread pool	  */
	pthread_cond_t 	stop_cond;			/* stop cond to exit the pool 		  */
	BOOL			stop_wait_need;		/* whether the thread need to waited  */
	Queue *			idle_queue; 		/* idle queue of the threads		  */
	Queue *			curt_queue; 		/* current queue of the threads		  */
	BOOL 			pool_stop_need; 	/* whether stop the threading pool	  */
	float 			busy_threshold;		/* busy threshold of the thread pool  */
	uint8_t 		manage_interval;	/* manage thread sleep interval    	  */
	struct thread_info 	mgr_th; 		/* manage thread 			  		  */
} ThreadPool;

/* status of the thread pool */
typedef enum pool_status {
	IDLE,								/* thread pool is idle 				  */
	BUSY								/* thread pool is busy				  */
} POOL_STS;




extern ThreadPool *	 th_pool;

/*==============================================================================
* Function declarations
*=============================================================================*/
extern ThreadPool * ThreadPool_new(uint16_t min_th_num, uint16_t max_th_num);
extern int8_t ThreadPool_start(ThreadPool* th_pool);
extern void ThreadPool_run(ThreadPool* th_pool);
extern void ThreadPool_stop(ThreadPool* th_pool);
extern void ThreadPool_free(ThreadPool** th_pool_ptr);
extern void ThreadPool_job_add(ThreadPool* th_pool, job_handler job_func, void *arg);
extern void ThreadPool_set_threshold(ThreadPool* th_pool, float busy_threshold);
extern void ThreadPool_set_interval(ThreadPool* th_pool, uint8_t manage_interval);
extern float ThreadPool_get_threshold(ThreadPool* th_pool);
extern uint8_t ThreadPool_get_interval(ThreadPool* th_pool);














#endif
