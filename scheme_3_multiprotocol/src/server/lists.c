#include "../../include/server/lists.h"

/*Инициализация списка клиентов
Возвращает
- 0
- -1 в случае ошибки*/
int list_init(List* list) {
    if (list == NULL) {
        return -1; 
    }
    list->head = NULL;
    list->tail = NULL;
    return 0; 
}

/*Функция добавления нового узла в конец списка
Возвращает
- 0
- -1 в случае ошибки*/
int list_append_node(List* list, Node* new_node) {
    if (list == NULL || new_node == NULL) {
        perror("Invalid argument in list_append_node()");
        return -1; 
    }

    new_node->prev = list->tail;
    new_node->next = NULL;

    if (list->tail != NULL) {
        list->tail->next = new_node;
    }

    if (list->head == NULL) {
        list->head = new_node;
    }

    list->tail = new_node;
    return 0;
}

/*Удаление узла из списка
(НЕ освобождает Node)*/
void list_remove_node(List* list, Node* node) {
    if (node == NULL) {
        return; // Ошибка: неверный указатель на узел
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

}

// Освободить ресурсы, выделенные list_create_node
void destroy_node(Node* node){
    if (node != NULL){
        free(node);
    }
}

/*Удаление узла из списка
(освобождает Node, удаление NULL указателя будет успешным)*/
void list_destroy_node(List* list, Node* node) {
    list_remove_node(list, node);
    destroy_node(node);
}


// Уничтожение списка клиентов, освобождает все node
void list_destroy(List* list) {
    if (list == NULL) {
        return;
    }

    // Node* current = list->head;
    // while (current != NULL) {
    //     free(current);
    //     current = current->next;
    // }

    Node* current = list->head;
    Node* next = current;
    while (next != NULL) {
        next = current->next;
        free(current);
    }

    list->head = NULL;
    list->tail = NULL;
}

/*Создать ServiceNode, освобождайте самостоятельно
- Не обязательно задавать client_endpoint (можно передать NULL)
- Не обязательно задавать client_fd (можно передать 0)
- Инициализирует kill_thread_flag 0

Возвращает
- указатель на созданный Node
- NULL в случае ошибки*/
ServiceNode* service_list_create_node(pthread_t tid, int socket_fd, int client_fd, 
    const struct sockaddr_in* socket_endpoint, const struct sockaddr_in* client_endpoint) {

    ServiceNode* node = (ServiceNode*)malloc(sizeof(ServiceNode));
    if (node == NULL){
        perror("malloc() in client_list_create_node()");
        return NULL;
    }

    // Копирование данных
    node->tid = tid;
    node->socket_fd = socket_fd;
    node->client_fd = client_fd;

    if (socket_endpoint == NULL)
        memset(&(node->socket_endpoint), 0, sizeof(struct sockaddr_in));
    else
        memcpy(&(node->socket_endpoint), socket_endpoint, sizeof(struct sockaddr_in));

    if (client_endpoint == NULL)
        memset(&(node->client_endpoint), 0, sizeof(struct sockaddr_in));
    else
        memcpy(&(node->client_endpoint), client_endpoint, sizeof(struct sockaddr_in));

    node->kill_thread_flag = 0;

    node->prev = NULL;
    node->next = NULL;

    return node;
}

/*Создать ListenerNode, освобождайте самостоятельно
- Инициализирует kill_thread_flag 0

Возвращает
- указатель на созданный Node
- NULL в случае ошибки*/
ListenerNode* listener_list_create_node(pthread_t tid, int socket_fd, const struct sockaddr_in* socket_endpoint) {
    if (socket_endpoint == NULL) {
        perror("Invalid argument in listener_list_create_node()");
        return NULL;
    }

    ListenerNode* node = (ListenerNode*)malloc(sizeof(ListenerNode));
    if (node == NULL) {
        perror("malloc() in listener_list_create_node()");
        return NULL;
    }

    // Копирование данных
    node->tid = tid;
    node->socket_fd = socket_fd;
    memcpy(&(node->socket_endpoint), socket_endpoint, sizeof(struct sockaddr_in));
    node->kill_thread_flag = 0;
    
    node->prev = NULL;
    node->next = NULL;

    return node;
}
