#ifndef LISTENER_THREAD_H
#define LISTENER_THREAD_H

#include "servpar.h"
#include "lists.h"
#include "service_thread.h"

struct ListenerThread { // Static structure
    struct sockaddr_in endpoint;
    pthread_t tid;
    int kill_thread_flag;
    struct ServicePool* pool;
};
typedef struct ListenerThread ListenerThread;

void* listener_thread(void* args);


#endif