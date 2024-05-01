#ifndef SERVICE_THREAD_H
#define SERVICE_THREAD_H

#define _GNU_SOURCE // Удалить потом

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <malloc.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "servpar.h"
#include "lists.h"

struct ServicePool {    // Thread args
    struct ServiceList active_list;
    struct ServiceList ready_list;
    struct ServiceList dead_list;
    pthread_mutex_t mutex;
};
typedef struct ServicePool ServicePool;

void* service_thread(void* _pool);

ServicePool* service_pool_initialize();
int service_pool_free(ServicePool* pool);

int service_pool_threads_create(ServicePool* pool, int count);
int service_pool_threads_terminate(ServicePool* pool);
int service_pool_threads_collect(ServicePool* pool);
ServiceNode* service_pool_threads_wake_up_head(ServicePool* pool);


#endif