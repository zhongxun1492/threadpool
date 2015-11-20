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


#include "threadpool.h"

extern void Server_time_chk(void * arg);

static ThreadInfo* ThreadPool_th_add(ThreadPool* th_pool, job_handler job_func, void *arg);
static void ThreadPool_close(ThreadPool* th_pool);
static void ThreadPool_th_finish(ThreadInfo* stop_th);
static void * ThreadPool_work(void * arg);
static void * ThreadPool_manage(void * arg);
static POOL_STS ThreadPool_get_status(ThreadPool* th_pool);
static int8_t ThreadPool_th_del(ThreadPool* th_pool);
static int8_t ThreadPool_th_creat(ThreadPool* th_pool, ThreadInfo*	start_th, 
								start_routine th_func, job_handler job_func, void *arg);



ThreadPool *	 th_pool;



ThreadPool * ThreadPool_new(uint16_t min_th_num, uint16_t max_th_num)
{
	ThreadPool * new_th_pool;

	new_th_pool = NULL;

	new_th_pool = (ThreadPool*)safe_malloc(sizeof(ThreadPool));
	new_th_pool->min_num = min_th_num;
	new_th_pool->max_num = max_th_num;
	new_th_pool->pool_stop_need = FALSE;
	new_th_pool->pool_wait_need = TRUE;
	new_th_pool->stop_wait_need = TRUE;
	new_th_pool->idle_queue = Queue_new(max_th_num * 2);
	new_th_pool->curt_queue = Queue_new(max_th_num * 2);
	new_th_pool->busy_threshold  = BUSY_THRESHOLD;
	new_th_pool->manage_interval = MANAGE_INTERVAL;
	pthread_mutex_init(&new_th_pool->pool_lock, NULL);
	pthread_cond_init(&new_th_pool->pool_cond, 	NULL);
	pthread_mutex_init(&new_th_pool->stop_lock, NULL);
	pthread_cond_init(&new_th_pool->stop_cond, 	NULL);
	
	log_debug(log_cat, "threadPool new success");
	
	return new_th_pool;
}



int8_t ThreadPool_start(ThreadPool* th_pool)
{
	int16_t		i;
	ThreadInfo*	sig_th;
	int8_t		return_err_1;
	int8_t		return_err_2;
	
	i	   = 0;
	sig_th = NULL;
	return_err_1 = EXEC_SUCS;
	return_err_2 = EXEC_SUCS;

	for (i = 0; i < th_pool->min_num; i++) {
		sig_th = NULL;
		sig_th = (ThreadInfo*)safe_malloc(sizeof(ThreadInfo));
		return_err_1 = ThreadPool_th_creat(th_pool, sig_th, ThreadPool_work, NULL, NULL);
		if (return_err_1) {
			safe_free((void**)&sig_th);
			break;
		}
	}

	if (!return_err_1) {
		log_debug(log_cat, "%d work thread create success", Queue_num(th_pool->curt_queue));
	} else {
		log_fatal(log_cat, "work thread create failed");
	}

	return_err_2 = ThreadPool_th_creat(th_pool, &(th_pool->mgr_th), ThreadPool_manage, Server_time_chk, NULL);
	if (!return_err_2) {
		log_debug(log_cat, "manage thread create success");
	} else {
		log_fatal(log_cat, "manage thread create failed");
	}

	usleep(1000000);

	return (return_err_1 || return_err_2);
}



void ThreadPool_run(ThreadPool* th_pool)
{
	pthread_mutex_lock(&(th_pool->stop_lock));
	log_debug(log_cat, "thread pool is running");
	while (th_pool->stop_wait_need)
		pthread_cond_wait(&(th_pool->stop_cond), &(th_pool->stop_lock));
	th_pool->stop_wait_need = TRUE;
	pthread_mutex_unlock(&(th_pool->stop_lock));
}



void ThreadPool_stop(ThreadPool* th_pool)
{
	log_debug(log_cat, "thread pool is going to be stop");
	th_pool->stop_wait_need = FALSE;
	pthread_cond_signal(&(th_pool->stop_cond));
}



static void ThreadPool_close(ThreadPool* th_pool)
{
	uint16_t 	i;
	ThreadInfo* sig_th;
	
	i 		= 0;
	sig_th 	= NULL;

	th_pool->pool_stop_need = TRUE;

	while ((sig_th = (ThreadInfo*)Queue_pop(th_pool->curt_queue))) {
		ThreadPool_th_finish(sig_th);
		log_debug(log_cat, "close job thread: %u success", sig_th->th_id);
		safe_free((void**)&sig_th);
		sig_th = NULL;
	}

	ThreadPool_th_finish(&(th_pool->mgr_th));

	log_debug(log_cat, "close manage thread: %u success", th_pool->mgr_th.th_id);
}



void ThreadPool_free(ThreadPool** th_pool_ptr)
{
	ThreadPool_close(*th_pool_ptr);

	Queue_free(&((*th_pool_ptr)->idle_queue));
	Queue_free(&((*th_pool_ptr)->curt_queue));

	pthread_mutex_destroy(&((*th_pool_ptr)->pool_lock));
	pthread_cond_destroy(&((*th_pool_ptr)->pool_cond));
	pthread_mutex_destroy(&((*th_pool_ptr)->stop_lock));
	pthread_cond_destroy(&((*th_pool_ptr)->stop_cond));

	safe_free((void**)th_pool_ptr);

	log_debug(log_cat, "thread pool free success");
}



void ThreadPool_job_add(ThreadPool* th_pool, job_handler job_func, void *arg)
{
	ThreadInfo*		new_th;

	new_th = NULL;

	new_th = (ThreadInfo*)Queue_pop(th_pool->idle_queue);
	if (new_th != NULL) {
		new_th->job_func = job_func;
		new_th->job_arg  = arg;
		new_th->th_wait_need = FALSE;
		pthread_cond_signal(&(new_th->th_cond));
	} else {
		new_th = ThreadPool_th_add(th_pool, job_func, arg);
	}

	if (new_th != NULL) {
		log_debug(log_cat, "add job to thread pool success");
	} else {
		log_warn(log_cat, "add job to thread pool failed");
	}
}



static ThreadInfo* ThreadPool_th_add(ThreadPool* th_pool, job_handler job_func, void *arg)
{
	int8_t			return_err;
	ThreadInfo *	new_th;

	new_th 	   = NULL;
	return_err = EXEC_SUCS;

	if (th_pool->max_num <= Queue_num(th_pool->curt_queue)) {
		log_warn(log_cat, "thread pool is full, no more idle thread");
	} else {
		new_th = (ThreadInfo*)safe_malloc(sizeof(ThreadInfo));
		log_debug(log_cat, "%d job threads alive, more idle threads exist", Queue_num(th_pool->curt_queue));
	}

	if (new_th != NULL) {
		return_err = ThreadPool_th_creat(th_pool, new_th, ThreadPool_work, job_func, arg);
		if (return_err) {
			safe_free((void**)&new_th);
		}
	}

	if (new_th != NULL) {
		log_debug(log_cat, "add new job thread success");
	} else {
		log_warn(log_cat, "add new job thread failed");
	}

	return new_th;
}



static int8_t ThreadPool_th_del(ThreadPool* th_pool)
{
	int8_t		 return_err;
	ThreadInfo*  del_th;

	del_th 	   = NULL;
	return_err = EXEC_SUCS;

	if (Queue_num(th_pool->curt_queue) <= th_pool->min_num) {
		log_debug(log_cat, "already minimum job thread numbers");
		return_err = EXEC_FAIL;
	}

	if (!return_err) {
		del_th = (ThreadInfo*)Queue_pop(th_pool->idle_queue);
		if (del_th != NULL) {
			log_debug(log_cat, "get an idle job thread %u success", del_th->th_id);
			ThreadPool_th_finish(del_th);
			Queue_del(th_pool->curt_queue, del_th);
			log_debug(log_cat, "close job thread %u success", del_th->th_id);
			safe_free((void**)&del_th);
			return_err = EXEC_SUCS;
		} else {
			log_warn(log_cat, "all job threads are busy");
			return_err = EXEC_FAIL;
		}
	}

	if (!return_err) {
		log_debug(log_cat, "delete job thread success");
	} else {
		log_debug(log_cat, "delete job thread failed");
	}

	return return_err;
}



static int8_t ThreadPool_th_creat(ThreadPool* th_pool, ThreadInfo*	start_th,
						   start_routine th_func, job_handler job_func, void *arg)
{
	int8_t return_err;

	return_err = EXEC_SUCS;

	start_th->th_pool = th_pool;
	start_th->job_func = job_func;
	start_th->job_arg  = arg;
	start_th->th_wait_need = TRUE;
	start_th->th_exit_need = FALSE;
	pthread_cond_init(&(start_th->th_cond),  NULL);
	pthread_mutex_init(&(start_th->th_lock), NULL);

	return_err = pthread_create(&(start_th->th_id), NULL, th_func, (void*)start_th);
	if (!return_err) {
		pthread_mutex_lock(&(th_pool->pool_lock));
		while (th_pool->pool_wait_need)
			pthread_cond_wait(&(th_pool->pool_cond), &(th_pool->pool_lock));
		th_pool->pool_wait_need = TRUE;
		pthread_mutex_unlock(&(th_pool->pool_lock));
		log_debug(log_cat, "thread %u create success", start_th->th_id);
	} else {
		pthread_cond_destroy(&(start_th->th_cond));
		pthread_mutex_destroy(&(start_th->th_lock));
		return_err = EXEC_FAIL;
	}

	return return_err;
}



static void ThreadPool_th_finish(ThreadInfo* stop_th)
{
	int8_t	return_err;

	return_err = EXEC_SUCS;

	return_err = pthread_kill(stop_th->th_id, 0);
	if (!return_err) {
		log_debug(log_cat, "thread: %u is alive", stop_th->th_id);
		stop_th->th_exit_need = TRUE;
		stop_th->th_wait_need = FALSE;
		pthread_cond_signal(&(stop_th->th_cond));
		pthread_join(stop_th->th_id, NULL);
	} else {
		log_debug(log_cat, "thread: %u is dead or other error", stop_th->th_id);
	}
	pthread_mutex_destroy(&(stop_th->th_lock));
	pthread_cond_destroy(&(stop_th->th_cond));
}



static void * ThreadPool_work(void * arg)
{	
	ThreadInfo *	work_th;
	ThreadPool * 	th_pool;

	work_th = (ThreadInfo*)arg;
	th_pool = work_th->th_pool;

	Queue_add(th_pool->curt_queue, NULL, work_th);
	
	if (work_th->job_func == NULL) {
		Queue_add(th_pool->idle_queue, NULL, work_th);
	}
	
	th_pool->pool_wait_need = FALSE;
	pthread_cond_signal(&(th_pool->pool_cond));

	while ((!th_pool->pool_stop_need) && (!work_th->th_exit_need)) {
		
		if (work_th->job_func != NULL) {
			log_debug(log_cat, "------ job thread %u is running    |", work_th->th_id);
			work_th->job_func(work_th->job_arg);
			work_th->job_func = NULL;
			work_th->job_arg = NULL;
			Queue_add(th_pool->idle_queue, NULL, work_th);
		}

		pthread_mutex_lock(&(work_th->th_lock));
		
		log_debug(log_cat, "<<<<<< job thread %u start waiting |", work_th->th_id);
		//log_debug(log_cat, "<---<---<---<---<---<---<---<---<---<---<---<---<");
		
		while (work_th->th_wait_need)
			pthread_cond_wait(&(work_th->th_cond), &(work_th->th_lock));
		
		work_th->th_wait_need = TRUE;
		
		pthread_mutex_unlock(&(work_th->th_lock));
		
		//log_debug(log_cat, ">--->--->--->--->--->--->--->--->--->--->--->--->");
		log_debug(log_cat, ">>>>>> job thread %u  end  waiting |", work_th->th_id);
	}

	log_warn(log_cat, "job thread %u is going to exit", work_th->th_id);

	return NULL;
}



static void * ThreadPool_manage(void * arg)
{
	ThreadInfo*		manage_th;
	ThreadPool*		th_pool;
	struct timespec wait_moment;

	manage_th = (ThreadInfo*)arg;
	th_pool   = manage_th->th_pool;
	memset(&wait_moment, 0x00, sizeof(struct timespec));

	th_pool->pool_wait_need = FALSE;
	pthread_cond_signal(&(th_pool->pool_cond));

	while ((!th_pool->pool_stop_need) && (!manage_th->th_exit_need)) {

		log_debug(log_cat, "------------------------------------|");
		
		if (ThreadPool_get_status(th_pool) == IDLE) {
			while (!(ThreadPool_th_del(th_pool)))
				;	
		}

		if (manage_th->job_func != NULL) {
			log_debug(log_cat, "manage thread %u is running |", manage_th->th_id);
			manage_th->job_func(manage_th->job_arg);
		}

		time(&(wait_moment.tv_sec));
		wait_moment.tv_sec += th_pool->manage_interval;
		pthread_mutex_lock(&(manage_th->th_lock));
		
		log_debug(log_cat, "manage thread %u is waiting |", manage_th->th_id);
		log_debug(log_cat, "------------------------------------|");
		
		if (manage_th->th_wait_need)
			pthread_cond_timedwait(&(manage_th->th_cond), &(manage_th->th_lock), &wait_moment);
		
		manage_th->th_wait_need = TRUE;
		
		pthread_mutex_unlock(&(manage_th->th_lock));
	}

	log_warn(log_cat, "manage thread %u is going to exit", manage_th->th_id);
	
	return NULL;
}



static POOL_STS ThreadPool_get_status(ThreadPool* th_pool)
{
	float 		busy_num;
	uint16_t 	curt_num;
	uint16_t	idle_num;

	busy_num = 0.0;
	curt_num = Queue_num(th_pool->curt_queue);
	idle_num = Queue_num(th_pool->idle_queue);

	busy_num = curt_num - idle_num;
	log_debug(log_cat, "current:%u, busy:%u, idle:%u", curt_num, (uint16_t)busy_num, idle_num);

	if ((float)(busy_num / curt_num) < th_pool->busy_threshold) {
		log_debug(log_cat, "thread pool is idle");
		return IDLE;
	} else {
		log_debug(log_cat, "thread pool is busy");
		return BUSY;
	}
}



float ThreadPool_get_threshold(ThreadPool* th_pool)
{
	return th_pool->busy_threshold;
}



void ThreadPool_set_threshold(ThreadPool* th_pool, float busy_threshold)
{
	if (busy_threshold <= 1.0 && busy_threshold > 0.0) {
		th_pool->busy_threshold = busy_threshold;
	} else {
		log_warn(log_cat, "error busy_threshold");
	}
}



uint8_t ThreadPool_get_interval(ThreadPool* th_pool)
{
	return th_pool->manage_interval;
}



void ThreadPool_set_interval(ThreadPool* th_pool, uint8_t manage_interval)
{
	th_pool->manage_interval = manage_interval;
}



