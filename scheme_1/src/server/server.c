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
#include "../../include/server/servthreads.h"

// Разблокирует блокирующую функию
void unblock_handler(int signum) {
    (void)signum;
}

#define N 1

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

    // Заполняем структуру собственного адреса
    struct sockaddr_in listener_endpoint;
    listener_endpoint.sin_family = AF_INET;
    listener_endpoint.sin_zero[0] = '\0';
    if (inet_pton(AF_INET, SERV_IP_S, &listener_endpoint.sin_addr) != 1){
        perror("Critical inet_pton() in main()");
        exit(EXIT_FAILURE);
    }
    listener_endpoint.sin_port = htons(SERV_PORT);

    // Создать список живых случающих потоков
    ListenerList listener_list;
    list_init((List*)&listener_list);

    // Создать список мёртвх случающих потоков
    ListenerList listener_dead_list;
    list_init((List*)&listener_dead_list);

    // Создать список живых обслуживающих потоков
    ServiceList service_list;
    list_init((List*)&service_list);

    // Создать список мёртвх обслуживающих потоков
    ServiceList service_dead_list;
    list_init((List*)&service_dead_list);

    // Создать и запустить случающий поток
    pthread_t listener_tid;
    ListenerThreadArgs listener_args = {
        .listener_list = &listener_list,
        .listener_dead_list = &listener_dead_list,
        .service_list = &service_list,
        .service_dead_list = &service_dead_list,
        .socket_endpoint = &listener_endpoint
    };

    if (pthread_create(&listener_tid, NULL, listener_thread, (void*)&listener_args) != 0){
        perror("Critical pthread_create()");
        exit(EXIT_FAILURE);
    }

    // Создаём массивы указателей на списки мёртвых потоков для передачи в сборщик
    ListenerList* array_of_listener_dead_list[N];
    array_of_listener_dead_list[0] = &listener_dead_list;

    ServiceList* array_of_service_dead_list[N];
    array_of_service_dead_list[0] = &service_dead_list;

    // Создать поток-сборщик
    CollectorThreadArgs collector_args = {
        .array_of_listener_dead_list = array_of_listener_dead_list,
        .array_of_listener_dead_list_size = N,
        .array_of_service_dead_list = array_of_service_dead_list,
        .array_of_service_dead_list_size = N,
        .kill_flag = NULL
    };
    pthread_t collector_tid;
    if (pthread_create(&collector_tid, NULL, collector_thread, (void*)&collector_args) != 0){
        perror("Critical pthread_create()");
        exit(EXIT_FAILURE);
    }

    // Ввод для возможности завершения

    char ch = fgetc(stdin);
    (void)ch;

    ListenerNode* lcurrent;
    ListenerNode* lnext;
    // Послать сигнал завершения всем слушающим потокам
    lcurrent = listener_list.head;
    while(lcurrent != NULL){
        lnext = lcurrent->next;
        // Установить флаг заверщения
        lcurrent->kill_thread_flag = 1;
        // Послать сигнал для разблокировки блокирующих функций
        if (pthread_kill(lcurrent->tid, SIGUSR1) != 0){
            perror("Critical pthread_kill() in main()");
            exit(EXIT_FAILURE);
        }
        lcurrent = lnext;
    }

    ServiceNode* scurrent;
    ServiceNode* snext;
    // Послать сигнал завершения всем обслуживающим потокам
    scurrent = service_list.head;
    while(scurrent != NULL){
        snext = scurrent->next;
        // Установить флаг заверщения
        scurrent->kill_thread_flag = 1;
        // Послать сигнал для разблокировки блокирующих функций
        if (pthread_kill(lcurrent->tid, SIGUSR1) != 0){
            perror("Critical pthread_kill() in main()");
            exit(EXIT_FAILURE);
        }
        scurrent = snext;
    }

    // Послать сигнал завершения (Просто флаг) потоку-сборщику
    int kill_flag_value = 1;
    collector_args.kill_flag = &kill_flag_value;

    // Ждём завершения потока-сборщка
    int* status;
    if (pthread_join(collector_tid, (void**)&status) != 0){
        perror("Critical error in collector_thread() in join for ListenerNode");
        // Продолжать работу нельзя, всё очень плохо, что-то сильно сломалось, аварийное завершение
        exit(EXIT_FAILURE);
    }
    // Проверить статус потока
    if (status != NULL){
        perror("The collector thread ended with an error");
    }

    list_destroy((List*)&listener_list);
    list_destroy((List*)&listener_dead_list);
    list_destroy((List*)&service_list);
    list_destroy((List*)&service_dead_list);

    return 0;
}

