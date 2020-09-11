#ifndef __MYTHREAD_H
#define __MYTHREAD_H

#include <stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#include "confparse.h"

#define LL_ADD(item, list) { \
	item->prev = NULL; \
	item->next = list; \
	list = item; \
}

#define LL_REMOVE(item, list) { \
	if (item->prev != NULL) item->prev->next = item->next; \
	if (item->next != NULL) item->next->prev = item->prev; \
	if (list == item) list = item->next; \
	item->prev = item->next = NULL; \
}

typedef struct worker {	
	pthread_t thread;	
	int terminate;	
	struct workqueue *workqueue;	
	struct worker *prev;	
	struct worker *next;
} worker_t;

typedef struct job {	
	void (*job_function)(struct job *job);	
	void *user_data;
	int  sockfd;
	CONF *conf;
	struct sockaddr_in cli;	
	struct job *prev;	
	struct job *next;
} job_t;

typedef struct workqueue {	
	struct worker *workers;	
	struct job *waiting_jobs;	
	pthread_mutex_t jobs_mutex;	
	pthread_cond_t jobs_cond;
} workqueue_t;


static void *worker_function(void *ptr);

int workqueue_init(workqueue_t *workqueue, int numWorkers);	

void workqueue_shutdown(workqueue_t *workqueue);	

void workqueue_add_job(workqueue_t *workqueue, job_t *job);	

workqueue_t workqueue;
#endif
