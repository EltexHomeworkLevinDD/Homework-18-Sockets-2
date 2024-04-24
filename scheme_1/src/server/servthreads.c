#include "../../include/server/servthreads.h"

pthread_mutex_t service_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t listener_mutex = PTHREAD_MUTEX_INITIALIZER;

/*Сам добавляет и удаляет себя из списка*/
void* listener_thread(void* _args){
    struct ListenerThreadArgs* args = (struct ListenerThreadArgs*) _args;

    // Создаём слушающий сетевой потоковый сокет
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1){
        perror("socket creation in listener_thread()");
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Добавляем себя в список
    ListenerNode* me = listener_list_create_node(pthread_self(), socket_fd, args->socket_endpoint);
    if (me == NULL){
        perror("Bind() in listener_thread()");
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }
    pthread_mutex_lock(&listener_mutex);
    list_append_node((List*)args->listener_list, (Node*)me);
    pthread_mutex_unlock(&listener_mutex);


    // Назначаем сокету адрес
    if (bind(socket_fd, (const struct sockaddr*)&me->socket_endpoint, sizeof(struct sockaddr_in)) == -1){
        perror("Bind() in listener_thread()");

        pthread_mutex_lock(&listener_mutex);
        // Убрать node из списка живых (Не удаляя его)
        list_remove_node((List*)(args->listener_list), (Node*)(me));
        // Добавить node в список мёртвых (Не создавая его)
        list_append_node((List*)(args->listener_dead_list), (Node*)(me));
        pthread_mutex_unlock(&listener_mutex);

        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Переводим в режим ожидания
    if (listen(socket_fd, SERV_BACKLOG) == -1){
        perror("listen() in listener_thread()");

        pthread_mutex_lock(&listener_mutex);
        list_remove_node((List*)(args->listener_list), (Node*)(me));
        list_append_node((List*)(args->listener_dead_list), (Node*)(me));
        pthread_mutex_unlock(&listener_mutex);
        
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Endpoint подключённого клиента 
    struct sockaddr_in client_enpoint;
    socklen_t client_len = sizeof(me->socket_endpoint);
    // Структура пдреса обслуживающего сокета
    struct sockaddr_in service_socket_endpoint;
    // Активный дескриптор слушающего сокета
    int client_fd; 

    while(me->kill_thread_flag == 0){
        // Блокирующе подключаем клиента
        client_fd = accept(socket_fd, (struct sockaddr*)&client_enpoint, &client_len);
        // Если accept неуспешно завершился
        if (client_fd == -1){
            // Если функция была прервана сигналом
            if (errno == EINTR){
                // Если был установлен флаг заверщения (Послан сигнал завершения)
                if (me->kill_thread_flag != 0){
                    // Завершить поток
                    pthread_mutex_lock(&listener_mutex);
                    list_remove_node((List*)(args->listener_list), (Node*)(me));
                    list_append_node((List*)(args->listener_dead_list), (Node*)(me));
                    pthread_mutex_unlock(&listener_mutex);
        
                    close(socket_fd);
                    return NULL;
                }
                // Если не был установлен флаг заверщения (Прерывание из-за другого сигнала)
                else {
                    // Перезапустить операцию
                    continue;
                }
            }
            // Если необработанная критическая ошибка
            perror("accept() in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }
        if (me->kill_thread_flag != 0){
            break;
        }

        // Если accept успешно завершился, требуется получить адрес, на который переподключится клиент

        // Создать сокет обслуживающего потока (для получения endpoint'a)
        int service_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (service_socket_fd == -1){
            perror("socket() for service in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // // Инициализация структуры адреса обслуживающего сокета (на любой доступный порт)
        // service_socket_endpoint.sin_family = AF_INET;
        // //service_socket_endpoint.sin_addr.s_addr = me->socket_endpoint.sin_addr.s_addr;
        // memcpy(&(service_socket_endpoint.sin_addr), &(me->socket_endpoint.sin_addr), sizeof(struct in_addr));
        // //service_socket_endpoint.sin_port = htons(SERV_PORT); 
        // service_socket_endpoint.sin_port = htons(0); 

        service_socket_endpoint.sin_family = AF_INET;
        inet_pton(AF_INET, SERV_IP_S, &(service_socket_endpoint.sin_addr));
        service_socket_endpoint.sin_port = 0;

        // Привязываем обслуживающий сокет 
        if (bind(service_socket_fd, (const struct sockaddr*)&service_socket_endpoint, sizeof(struct sockaddr_in)) == -1){
            perror("bind() client thread to any addr in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            close(service_socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Получаем адрес обслуживающего сокета
        socklen_t len = sizeof(struct sockaddr_in);
        if (getsockname(service_socket_fd, (struct sockaddr*)&service_socket_endpoint, &len) != 0){
            perror("getsockname() of new client thread enpoint in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            close(service_socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // char ipstr[INET_ADDRSTRLEN];
        // if (inet_ntop(AF_INET, (const void*)&(service_socket_endpoint.sin_addr.s_addr), ipstr, INET_ADDRSTRLEN) == NULL) {
        //     perror("inet_ntop() failed");
        //     exit(EXIT_FAILURE);
        // }
        // unsigned short portstr = ntohs(service_socket_endpoint.sin_port);

        // Адрес получен, нужно сообщить клиенту к кому переподключиться

        // Посылаем сообщение подключённому клиенту, на какой адрес ему нужно переподключиться
        ssize_t bytes_sended = send(client_fd, (const void*)&service_socket_endpoint, sizeof(struct sockaddr_in), 0);
        if (bytes_sended == -1){
            perror("send() new endpoint to client in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            close(service_socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Запускаем обслуживающий поток

        // Создаём структуру аргументов, будет освобождаться в service_thread
        ServiceThreadArgs* service_args = malloc(sizeof(ServiceThreadArgs));
        if (service_args == NULL){
            perror("malloc() for ServiceThreadArgs in listener_thread()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            close(service_socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Копируем данные
        service_args->socket_fd = service_socket_fd;
        service_args->service_list = args->service_list;
        service_args->service_dead_list = args->service_dead_list;
        memcpy((void*)&(service_args->socket_endpoint), (const void*)(&service_socket_endpoint), sizeof(struct sockaddr_in));

        // Запускаем поток клиента
        pthread_t service_tid;
        if (pthread_create(&service_tid, NULL, service_thread, (void*)service_args) != 0){
            perror("pthread_create()");

            pthread_mutex_lock(&listener_mutex);
            list_remove_node((List*)(args->listener_list), (Node*)(me));
            list_append_node((List*)(args->listener_dead_list), (Node*)(me));
            pthread_mutex_unlock(&listener_mutex);
        
            close(client_fd);
            close(socket_fd);
            close(service_socket_fd);
            free(service_args);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // ПРОВЕРКИ НА ФАКТ ОТПРАВКИ ДАННЫХ НЕТ

        // Закрыть активный дескриптор слушающего сокета
        close(client_fd);
    }

    pthread_mutex_lock(&listener_mutex);
    list_remove_node((List*)(args->listener_list), (Node*)(me));
    list_append_node((List*)(args->listener_dead_list), (Node*)(me));
    pthread_mutex_unlock(&listener_mutex);

    close(socket_fd);
    return NULL;
}

/*Сам добавляет и удаляет себя из списка*/
void* service_thread(void* _args){
    struct ServiceThreadArgs* args = (struct ServiceThreadArgs*) _args;

    // Добавляем себя в список
    ServiceNode* me = service_list_create_node(pthread_self(), args->socket_fd, 0, (const struct sockaddr_in*)&(args->socket_endpoint), NULL);
    if (me == NULL){
        perror("Bind() in service_thread()");
        close(args->socket_fd);
        free(_args);
        pthread_exit((void*)EXIT_FAILURE);
    }

    pthread_mutex_lock(&service_mutex);
    list_append_node((List*)args->service_list, (Node*)me);
    pthread_mutex_unlock(&service_mutex);

    // Переводим сокет в слушающий режим
    if (listen(args->socket_fd, SERV_BACKLOG) == -1){
        perror("listen() in service_thread()");

        pthread_mutex_lock(&service_mutex);
        list_remove_node((List*)(args->service_list), (Node*)(me));
        list_append_node((List*)(args->service_dead_list), (Node*)(me));
        pthread_mutex_unlock(&service_mutex);

        close(me->socket_fd);
        free(args);
        pthread_exit((void*)EXIT_FAILURE);
    }

    socklen_t client_len = sizeof(struct sockaddr_in);

    // Цикл для обработки прерываний
    while(1){
        // Блокирующе подключаем клиента
        me->client_fd = accept(me->socket_fd, (struct sockaddr*)&(me->client_endpoint), &client_len);
        if (me->client_fd == -1){
            // Если функция была прервана сигналом
            if (errno == EINTR){
                // Если был установлен флаг заверщения (Послан сигнал завершения)
                if (me->kill_thread_flag != 0){
                    // Завершить поток

                    pthread_mutex_lock(&service_mutex);
                    list_remove_node((List*)(args->service_list), (Node*)(me));
                    list_append_node((List*)(args->service_dead_list), (Node*)(me));
                    pthread_mutex_unlock(&service_mutex);

                    close(me->socket_fd);
                    free(args);
                    return NULL;
                }
                // Если не был установлен флаг заверщения (Прерывание из-за другого сигнала)
                else {
                    // Перезапустить операцию
                    continue;
                }
            }
            // Необработанная критическая ошибка
            perror("accept() in service_thread()");

            pthread_mutex_lock(&service_mutex);
            list_remove_node((List*)(args->service_list), (Node*)(me));
            list_append_node((List*)(args->service_dead_list), (Node*)(me));
            pthread_mutex_unlock(&service_mutex);

            close(me->socket_fd);
            free(args);
            pthread_exit((void*)EXIT_FAILURE);
        }

        break;
    }

    // Получение текущего времени
    time_t currentTime;
    time(&currentTime);

    // Преобразование текущего времени в локальное время
    struct tm *localTime = localtime(&currentTime);
    if (localTime == NULL) {
        perror("localtime() in service_thread()");

            pthread_mutex_lock(&service_mutex);
            list_remove_node((List*)(args->service_list), (Node*)(me));
            list_append_node((List*)(args->service_dead_list), (Node*)(me));
            pthread_mutex_unlock(&service_mutex);

        close(me->client_fd);
        close(me->socket_fd);
        free(args);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Форматирование локального времени для вывода
    char time_string[SERV_BUFFER_SZ];
    if (strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localTime) == 0) {
        perror("strftime() in service_thread()");

            pthread_mutex_lock(&service_mutex);
            list_remove_node((List*)(args->service_list), (Node*)(me));
            list_append_node((List*)(args->service_dead_list), (Node*)(me));
            pthread_mutex_unlock(&service_mutex);

        close(me->client_fd);
        close(me->socket_fd);
        free(args);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Отправляем текущее время
    ssize_t bytes_sended = send(me->client_fd, (const void*)&time_string, sizeof(struct sockaddr_in), 0);
    if (bytes_sended == -1){
        perror("send() current time in listener_thread()");

            pthread_mutex_lock(&service_mutex);
            list_remove_node((List*)(args->service_list), (Node*)(me));
            list_append_node((List*)(args->service_dead_list), (Node*)(me));
            pthread_mutex_unlock(&service_mutex);

        free(args);
        pthread_exit((void*)EXIT_FAILURE);
    }


    pthread_mutex_lock(&service_mutex);
    list_remove_node((List*)(args->service_list), (Node*)(me));
    list_append_node((List*)(args->service_dead_list), (Node*)(me));
    pthread_mutex_unlock(&service_mutex);

    // ПРОВЕРКИ НА ФАКТ ОТПРАВКИ ДАННЫХ НЕТ

    close(me->client_fd);
    close(me->socket_fd);
    free(args);
    return NULL;
}

/*
Освобождает некоторые ресурсы потоков (в том числе узлы списков)
Забирает статус мёртвых потоков
Предполагается, что он существует для множества мёртвых потоков*/
void* collector_thread(void* _args){
    CollectorThreadArgs* args = (CollectorThreadArgs*) _args;

    Node* node;
    Node *next_node;
    List* list;

    int* status;
    while(1){
        // Получить доступ к каждому элементу массива списка мёртвых слушающих потоков
        for (int i = 0; i < args->array_of_listener_dead_list_size; i++){
            list = (List*)(args->array_of_listener_dead_list[i]);
            
            // Пройтись по каждому элементу списка мёртвых слушающих потоков
            node = list->head;
            while(node != NULL){
                next_node = node->next;
                // Забрать статус потока
                if (pthread_join( ( (ListenerNode*) node)->tid, (void**)&status) != 0){
                    perror("Critical error in collector_thread() in join for ListenerNode");
                    // Продолжать работу нельзя, всё очень плохо, что-то сильно сломалось, аварийное завершение
                    exit(EXIT_FAILURE);
                }
                // Проверить статус потока
                if (status != NULL){
                    perror("The listening thread ended with an error");
                }

                // Удалить Node из списка и из памяти
                pthread_mutex_lock(&service_mutex);
                list_destroy_node(list, node);
                pthread_mutex_unlock(&service_mutex);

                // Обновляем указатель на текущий узел
                node = next_node;
            }
        }

        // Получить доступ к каждому элементу массива списка мёртвых обслуживающих потоков
        for (int i = 0; i < args->array_of_service_dead_list_size; i++){
            list = (List*)(args->array_of_service_dead_list[i]);
            
            // Пройтись по каждому элементу списка мёртвых обслуживающих потоков
            node = list->head;
            while (node != NULL) {
                // Сохраняем указатель на следующий узел
                next_node = node->next;
                // Забрать статус потока
                if (pthread_join(((ServiceNode *)node)->tid, (void**)&status) != 0) {
                    perror("Critical error in collector_thread() in join for ServiceNode");
                    // Продолжать работу нельзя, всё очень плохо, что-то сильно сломалось, аварийное завершение
                    exit(EXIT_FAILURE);
                }
                // Проверить статус потока
                if (status != NULL) {
                    perror("The service thread ended with an error");
                }

                // Удалить Node из списка и из памяти
                pthread_mutex_lock(&listener_mutex);
                list_destroy_node(list, node);
                pthread_mutex_unlock(&listener_mutex);


                // Обновляем указатель на текущий узел
                node = next_node;
            }
        }

        // Установлен флаг завершения, выйти
        if (args->kill_flag != NULL){
            break;
        }

        sleep(1);
    }

    return NULL;
}