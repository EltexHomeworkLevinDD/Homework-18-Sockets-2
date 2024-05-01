#include "../../include/server/service_thread.h"

void* service_thread(void* _pool){
    struct ServicePool* pool = (struct ServicePool*) _pool;

    // Добавляем себя в список
    ServiceNode* me = service_list_create_node(pthread_self(), 0, 0, NULL, NULL);
    if (me == NULL){
        perror("Bind() in service_thread() was NOT trackable");
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Создать сокет обслуживающего потока (для получения endpoint'a)
    me->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (me->socket_fd == -1){
        perror("socket() for service in service_thread() was NOT trackable");
        pthread_exit((void*)EXIT_FAILURE);
    }

    me->socket_endpoint.sin_family = AF_INET;
    inet_pton(AF_INET, SERV_IP_S, &(me->socket_endpoint.sin_addr));
    me->socket_endpoint.sin_port = 0;

    // Привязываем обслуживающий сокет на адрес сервера и любой порт
    if (bind(me->socket_fd, (const struct sockaddr*)&me->socket_endpoint, sizeof(struct sockaddr_in)) == -1){
        perror("bind() client thread to any addr in service_thread() was NOT trackable");
        close(me->socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Получаем адрес обслуживающего сокета
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(me->socket_fd, (struct sockaddr*)&(me->socket_endpoint), &len) != 0){
        perror("getsockname() of new client thread enpoint in service_thread() was NOT trackable");
        close(me->socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Переводим сокет в слушающий режим
    if (listen(me->socket_fd, SERV_BACKLOG) == -1){
        perror("listen() in service_thread() was NOT trackable");
        close(me->socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    socklen_t client_len = sizeof(struct sockaddr_in);

    int port_number = ntohs(me->socket_endpoint.sin_port);

    // Добавляем себя в список ожидающих
    pthread_mutex_lock(&(pool->mutex));
    list_append_node((List*)&(pool->ready_list), (Node*)me);
    fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
    pthread_mutex_unlock(&(pool->mutex));

    sigset_t mask;
    sigemptyset(&mask);
    int sig = SIGUSR1;
    sigaddset(&mask, sig);
    //sigprocmask(SIG_BLOCK, (const sigset_t*)&mask, NULL);

    // Цикл для обработки прерываний
    while(me->kill_thread_flag == 0){
        
        sigwait((const sigset_t*)&mask, &sig);

        // Проверяем, есть ли сигнал завершения
        pthread_mutex_lock(&(pool->mutex));
        if (me->kill_thread_flag == 0){
            // Добавляем себя в список активных
            list_remove_node((List*)&(pool->ready_list), (Node*)(me));
            list_append_node((List*)&(pool->active_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis active\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
        }
        else {
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->ready_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            break;
        }

        // Цикл для прерывания
        while(1){
            // Блокирующе подключаем клиента
            me->client_fd = accept(me->socket_fd, (struct sockaddr*)&(me->client_endpoint), &client_len);

            pthread_mutex_lock(&(pool->mutex));
            if (me->client_fd == -1){
                // Если функция была прервана сигналом
                if (errno == EINTR){
                    // Если был установлен флаг заверщения (Послан сигнал завершения)
                    if (me->kill_thread_flag != 0){
                        // Завершить поток
                        list_remove_node((List*)&(pool->active_list), (Node*)(me));
                        list_append_node((List*)&(pool->dead_list), (Node*)(me));
                        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
                        close(me->socket_fd);
                        pthread_mutex_unlock(&(pool->mutex));
                        return NULL;
                    }
                    // Если не был установлен флаг заверщения (Прерывание из-за другого сигнала)
                    else {
                        // Перезапустить операцию
                        // Добавляем себя в список готовых
                        list_remove_node((List*)&(pool->active_list), (Node*)(me));
                        list_append_node((List*)&(pool->ready_list), (Node*)me);
                        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
                        pthread_mutex_unlock(&(pool->mutex));
                        continue;
                    }
                }
                // Необработанная критическая ошибка
                perror("accept() in service_thread()");

                list_remove_node((List*)&(pool->active_list), (Node*)(me));
                list_append_node((List*)&(pool->dead_list), (Node*)(me));
                fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
                close(me->socket_fd);
                pthread_mutex_unlock(&(pool->mutex));

                pthread_exit((void*)EXIT_FAILURE);
            }
            pthread_mutex_unlock(&(pool->mutex));
            break;
        }

        // Получение текущего времени
        time_t currentTime;
        time(&currentTime);

        // Преобразование текущего времени в локальное время
        struct tm *localTime = localtime(&currentTime);
        if (localTime == NULL) {
            perror("localtime() in service_thread()");

            pthread_mutex_lock(&(pool->mutex));
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            close(me->client_fd);
            close(me->socket_fd);
            pthread_mutex_unlock(&(pool->mutex));

            pthread_exit((void*)EXIT_FAILURE);
        }

        // Форматирование локального времени для вывода
        char time_string[SERV_BUFFER_SZ];
        if (strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localTime) == 0) {
            perror("strftime() in service_thread()");

            pthread_mutex_lock(&(pool->mutex));
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            close(me->client_fd);
            close(me->socket_fd);
            pthread_mutex_unlock(&(pool->mutex));
            
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Отправляем текущее время
        ssize_t bytes_sended;
        while (1){
            pthread_mutex_lock(&(pool->mutex));
            bytes_sended = send(me->client_fd, (const void*)&time_string, sizeof(struct sockaddr_in), 0);
            if (bytes_sended == -1){
                // Если функция была прервана сигналом
                if (errno == EINTR){
                    // Если был установлен флаг заверщения (Послан сигнал завершения)
                    if (me->kill_thread_flag != 0){
                        // Завершить поток
                        list_remove_node((List*)&(pool->active_list), (Node*)(me));
                        list_append_node((List*)&(pool->dead_list), (Node*)(me));
                        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
                        close(me->client_fd);
                        close(me->socket_fd);
                        pthread_mutex_unlock(&(pool->mutex));
                        return NULL;
                    }
                    // Если не был установлен флаг заверщения (Прерывание из-за другого сигнала)
                    else {
                        // Перезапустить операцию
                        pthread_mutex_unlock(&(pool->mutex));
                        continue;
                    }
                }
                // Необработанная критическая ошибка
                perror("send() in service_thread()");

                list_remove_node((List*)&(pool->active_list), (Node*)(me));
                list_append_node((List*)&(pool->dead_list), (Node*)(me));
                fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
                close(me->client_fd);
                close(me->socket_fd);
                pthread_mutex_unlock(&(pool->mutex));

                pthread_exit((void*)EXIT_FAILURE);
            }
            pthread_mutex_unlock(&(pool->mutex));
            break;
        }

        // ПРОВЕРКИ НА ФАКТ ОТПРАВКИ ДАННЫХ НЕТ
        close(me->client_fd);

        // Переводим себя в список ожидающих
        pthread_mutex_lock(&(pool->mutex));
        list_remove_node((List*)&(pool->active_list), (Node*)(me));
        list_append_node((List*)&(pool->ready_list), (Node*)(me));
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        pthread_mutex_unlock(&(pool->mutex));
    }

    return NULL;
}

/*Разбудить первый попавшийся поток
Thread Safety
Возвращает 
- Указатель на структуру потока
- NULL, если свободных потоков нет
- (ServiceNode*)EXIT_FAILURE в случае ошибки*/
ServiceNode* service_pool_threads_wake_up_head(ServicePool* pool){
    ServiceNode* ready;
    pthread_mutex_lock(&(pool->mutex));
    // Найти первый свободный поток
    ready = pool->ready_list.head;
    if (ready != NULL){
        // Послать сигнал для разблокировки (без флага)
        if (pthread_kill(ready->tid, SIGUSR1) != 0){
            perror("pthread_kill() in wake_up_random_thread()");
            pthread_mutex_unlock(&(pool->mutex));;
            return (ServiceNode*)EXIT_FAILURE;
        }
    }

    pthread_mutex_unlock(&(pool->mutex));
    return ready;
}

/*Мягко убить все потоки в пуле
(Мягко - разорвать соединение и завершиться, перейдя в мёртвый лист)
Thread Safety
Возвращает 
- 0
- EXIT_FAILURE в случае ошибки*/
int service_pool_threads_terminate(ServicePool* pool) {
    ServiceNode* current;
    ServiceNode* next;
    pthread_mutex_lock(&(pool->mutex));
    current = pool->ready_list.head;
    next = current;
    // Послать сигнал завершения всем обслуживающим потокам
    while(current != NULL){
        next = current->next;
        // Установить флаг заверщения
        current->kill_thread_flag = 1;
        // Послать сигнал для разблокировки блокирующих функций
        if (pthread_kill(current->tid, SIGUSR1) != 0){
            perror("Critical pthread_kill() in main()");
            pthread_mutex_unlock(&(pool->mutex));
            return EXIT_FAILURE;
        }
        current = next;
    }
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

/*Создать пул потоков
Thread Safety
Возвращает
- указатель на структуру
- NULL в случае ошибки*/
ServicePool* service_pool_initialize(){
    ServicePool* pool = (ServicePool*)malloc(sizeof(ServicePool));
    if (pool == NULL){
        perror("malloc() for ServicePool* in create_service_pool()");
        return NULL;
    }
    list_init((List*)&(pool->active_list));
    list_init((List*)&(pool->ready_list));
    list_init((List*)&(pool->dead_list));

    if (pthread_mutex_init(&(pool->mutex), NULL) != 0) {
        perror("pthread_mutex_init() for mutex in create_service_pool()");
        free(pool);
        return NULL;
    }
    return pool;
}

/*Создать count потоков в пуле (можно добавить)
Thread safety
Возвращает
- 0
- EXIT_FAILURE в случае ошибки*/
int service_pool_threads_create(ServicePool* pool, int count){
    pthread_t service_tid;
    for (int i = 0; i < count; i++){
        if (pthread_create(&service_tid, NULL, service_thread, (void*)pool) != 0){
            perror("pthread_create() in service_pool_threads_create()");    
            return EXIT_FAILURE;
        }
    }

    return 0;
}

/*Освободить пул (Освобождает память пула, НЕ удяляет node списков, НЕ удаляет потоки)
NO Thread Safety!*/
int service_pool_free(ServicePool* pool){
    pthread_mutex_destroy(&(pool->mutex));
    free(pool);
    return 0;
}

/*Забрать статусы мёртвых потоков и освободить их ресурсы
Thread Safety
Возвращает
- 0
- EXIT_FAILURE в случае ошибки*/
int service_pool_threads_collect(ServicePool* pool){
    int* status;
    ServiceNode* current;
    ServiceNode* next;

    pthread_mutex_lock(&(pool->mutex));
    current = pool->dead_list.head;
    next = current;

    while(current != NULL){
        next = current->next;
        // Забрать статус
        if (pthread_join(current->tid, (void**)&status) != 0){
            perror("Critical pthread_join() for service in main()");
            pthread_mutex_unlock(&(pool->mutex));
            return EXIT_FAILURE;
        }
        if (status == NULL){
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tcompleted normally\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, ntohs(current->socket_endpoint.sin_port));
        }
        else {
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tcompleted emergency\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, ntohs(current->socket_endpoint.sin_port));
        }
        // Освободить Node
        list_destroy_node((List*)&(pool->dead_list), (Node*)current);
        current = next;
    }
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}