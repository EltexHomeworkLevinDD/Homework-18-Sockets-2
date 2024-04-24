#ifndef SERVTHREADS_H
#define SERVTHREADS_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>
#include <malloc.h>

#include "servpar.h"
#include "lists.h"

#define SERVIVE_TRASH_SZ    32
#define LISTENER_TRASH_SZ   8

struct CollectorThreadArgs { // Static structure
    struct ListenerList** array_of_listener_dead_list;   // Укащатель на массив списков слушающих потоков
    struct ServiceList** array_of_service_dead_list;     // Укащатель на массив списков обслуживающих потоков
    const int array_of_listener_dead_list_size; // Нельзя изменить размер
    const int array_of_service_dead_list_size;  // Нельзя изменить размер
    int* kill_flag;
};
typedef struct CollectorThreadArgs CollectorThreadArgs;

void* collector_thread(void* _args);



struct ListenerThreadArgs { // Static structure
    struct ListenerList* listener_list;     // Static pointer
    struct ListenerList* listener_dead_list;// Static pointer
    struct ServiceList* service_list;       // Static pointer
    struct ServiceList* service_dead_list;  // Static pointer
    struct sockaddr_in* socket_endpoint;    // Static pointer
};
typedef struct ListenerThreadArgs ListenerThreadArgs;

void* listener_thread(void* _args);
extern pthread_mutex_t listener_mutex;


struct ServiceThreadArgs {  // Dynamic structure (must be free())
    struct ServiceList* service_list;       // Static pointer
    struct ServiceList* service_dead_list;  // Static pointer
    int socket_fd;                          // Пассивный дескриптор сокета 
    struct sockaddr_in socket_endpoint;     // Static field
};
typedef struct ServiceThreadArgs ServiceThreadArgs;

void* service_thread(void* _args);
extern pthread_mutex_t service_mutex;

#endif