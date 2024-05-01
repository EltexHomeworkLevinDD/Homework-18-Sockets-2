#ifndef LISTS_H
#define LISTS_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <pthread.h>

// Общая структура узла списка (для перегрузки)
struct Node {
    struct Node* prev;
    struct Node* next;
};
typedef struct Node Node; 

// Общая структура списка (для перегрузки)
struct List {
    struct Node* head;
    struct Node* tail;
};
typedef struct List List; 

int list_init(List* list);
int list_append_node(List* list, Node* new_node);
void list_remove_node(List* list, Node* node);
void destroy_node(Node* node);
void list_destroy_node(List* list, Node* node);
void list_destroy(List* list);

// Определение структуры узла списка клиентского потока
struct ServiceNode {                     // Структура для потока, обслуживающего клиента
    struct ServiceNode* prev;
    struct ServiceNode* next;
    pthread_t tid;                      // Thread id
    int kill_thread_flag;               // Флаг завершения потока
    int socket_fd;                      // Пассивный дескриптор сокета (Для сокета)
    int client_fd;                      // Активный дескриптор сокета (Для записи)
    struct sockaddr_in socket_endpoint; // Endpoint пассивного сокета (Собственный)
    struct sockaddr_in client_endpoint; // Endpoint активного сокета (Клиента)
};
typedef struct ServiceNode ServiceNode;

// Определение структуры двусвязного списка потоков клиента
struct ServiceList {
    struct ServiceNode* head;
    struct ServiceNode* tail;
};
typedef struct ServiceList ServiceList;

ServiceNode* service_list_create_node(pthread_t tid, int socket_fd, int client_fd, 
    const struct sockaddr_in* socket_endpoint, const struct sockaddr_in* client_endpoint);

// Определение структуры узла списка слушающих потоков
struct ListenerNode {
    struct ListenerNode* prev;
    struct ListenerNode* next;
    pthread_t tid;                      // pthread_t слушающего потока
    int socket_fd;                      // Пассивный дескриптор сокета (Для сокета)
    int kill_thread_flag;               // Флаг завершения потока
    struct sockaddr_in socket_endpoint; // Endpoint пассивного сокета (Собственный)
};
typedef struct ListenerNode ListenerNode;

// Определение структуры двусвязного списка слушающих потока
struct ListenerList {
    struct ListenerNode* head;
    struct ListenerNode* tail;
};
typedef struct ListenerList ListenerList;

ListenerNode* listener_list_create_node(pthread_t tid, int socket_fd, const struct sockaddr_in* socket_endpoint);

#endif