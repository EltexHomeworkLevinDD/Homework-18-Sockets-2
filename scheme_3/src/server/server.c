#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../include/server/servpar.h"
#include "../../include/server/lists.h"
#include "../../include/server/service_thread.h"
#include "../../include/server/listener_thread.h"

#define QUEUE_PATH "/server_queue_key_file1"

// Разблокирует блокирующую функию
void unblock_handler(int signum) {
    (void)signum;
}

int main(){
    // Создаём маску сигналов
    struct sigaction sa = { 0 };
    sa.sa_handler = unblock_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_flags = 0;

    // Устанавливаем обработчик для SIGUSR1
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Critical sigaction() in main()");
        exit(EXIT_FAILURE);
    }

    // Создать структуру пула обслуживающих потоков
    ServicePool* pool = service_pool_initialize((const char*)QUEUE_PATH);
    if (pool == NULL){
        perror("Critical service_pool_initialize() in main()");
        exit(EXIT_FAILURE);
    }
    // Запустить потоки в пуле
    if (service_pool_threads_create(pool, 5) == EXIT_FAILURE){
        perror("Critical service_pool_threads_create() in main()");
        exit(EXIT_FAILURE);
    }

    // Создать endpoint слушающего потока
    ListenerThread* listener = (ListenerThread*) malloc(sizeof(ListenerThread));
    if (listener == NULL){
        perror("Critical malloc() for ListenerThread in main()");
        exit(EXIT_FAILURE);
    }

    listener->kill_thread_flag = 0;
    listener->pool = pool;
    listener->endpoint.sin_family = AF_INET;
    listener->endpoint.sin_zero[0] = '\0';
    if (inet_pton(AF_INET, SERV_IP_S, &(listener->endpoint.sin_addr)) != 1){
        perror("Critical inet_pton() in main()");
        exit(EXIT_FAILURE);
    }
    listener->endpoint.sin_port = htons(SERV_PORT);

    // Запустить слушающий поток
    if (pthread_create( &(listener->tid) , NULL, listener_thread, (void*)listener) != 0){
        perror("pthread_create() for service_thread() in main()");
        exit(EXIT_FAILURE);
    }

    char ch = fgetc(stdin);
    (void)ch;

    // Послать сигнал завершения всем обслуживающим потокам
    if (service_pool_threads_terminate(pool) != 0){
        perror("Critical service_pool_threads_terminate() in main()");
        exit(EXIT_FAILURE);
    }

    // Послать сигнал завершения всем слушающим потокам (одному)
    listener->kill_thread_flag = 1;
    // Послать сигнал для разблокировки блокирующих функций
    if (pthread_kill(listener->tid, SIGUSR1) != 0){
        perror("Critical pthread_kill() for listener in main()");
        return EXIT_FAILURE;
    }

    sleep(1);

    // Забрать статусы обслуживающих потоков и освободить их ресурсы
    if (service_pool_threads_collect(pool) != 0){
        perror("Critical service_pool_threads_collect() in main()");
        exit(EXIT_FAILURE);
    }

    int *status;
    // Забрать статусы слушающих потоков и освободить их ресурсы
    if (pthread_join(listener->tid, (void**)&status) != 0){
        perror("Critical pthread_join() for service in main()");
        pthread_mutex_unlock(&(pool->mutex));
        return EXIT_FAILURE;
    }
    if (status == NULL){
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tcompleted normally\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(listener->endpoint.sin_port));
    }
    else {
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tcompleted emergency\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(listener->endpoint.sin_port));
    }

    service_pool_free(pool);
    free(listener);

    return 0;
}

