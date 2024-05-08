#include "../../include/server/service_thread.h"

// /*Подключиться connect() с таймаутом
// Блокирующая (Переводит сокет в неблокирующий режим, ждёт таймаута, возвращвет блокирующий режим)
// Возвращает
// - 0 - В случае успеха
// - -1 - Критическая ошибка в одной из функций
// - -2 - Истёк таймаут
// - -3 - Функция была прервана сигналом
// - Ошибку из параметров сокета при подключении*/
// int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, suseconds_t timeout_usec){
//     // Устанавливаем сокет в неблокирующий режим
//     // Получаем флаги сокета
//     int old_flags = fcntl(sockfd, F_GETFL, 0);
//     if (old_flags == -1) {
//         perror("fcntl() for get old flags in connect_with_timeout()");
//         return -1;
//     }
//     // Устанавливаем дополнительные нужные флаги (Неблокирующий режим)
//     int new_flags = old_flags | O_NONBLOCK;
//     if (fcntl(sockfd, F_SETFL, new_flags) == -1) {
//         perror("fcntl() for set new flags in connect_with_timeout()");
//         return -1;
//     }

//     // Пытаемся подключиться
//     if (connect(sockfd, addr, addrlen) == 0) {
//         // Сокет успешно подключен (быстрее, чем таймаут)
//         // Восстанавливаем флаги
//         if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//             perror("fcntl() for set old flags in connect_with_timeout()");
//             return -1;
//         }
//         return 0;
//     // Возникла ошибка
//     } else {
//         // Если возникла ошибка, и это ошибка не из-за неудачного подключения, то она критическая
//         if (errno != EINPROGRESS) {
//             perror("connect() in connect_with_timeout()");
//             // Восстанавливаем флаги
//             if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//                 perror("fcntl() for set old flags in connect_with_timeout()");
//                 return -1;
//             }
//             return -1;
//         }
//     }

//     // Создаём набор дескрипторов
//     fd_set write_fds;       
//     FD_ZERO(&write_fds);        // Инициализируем набор
//     FD_SET(sockfd, &write_fds); // Добавляем сокет в набор

//     // Создаём временную структуру
//     struct timeval timeout;
//     timeout.tv_sec = 0;              // Устанавливаем время ожидания в секундах
//     timeout.tv_usec = timeout_usec;  // Устанавливаем время ожидания в микросекундах

//     // Ожидаем готовности сокета к записи
//     int ret = select(sockfd + 1, NULL, &write_fds, NULL, &timeout);
//     // Ошибка
//     if (ret == -1) {
//         // Восстанавливаем флаги
//         if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//             perror("fcntl() for set old flags in connect_with_timeout()");
//             return -1;
//         }
//         // Если был прерван сигналом
//         if (errno == EINTR){
//             return -3;
//         }
//         else {
//             perror("select() in connect_with_timeout()");
//             return -1;
//         }

//     // Истёк таймаут
//     } else if (ret == 0) {
//         // Восстанавливаем флаги
//         if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//             perror("fcntl() for set old flags in connect_with_timeout()");
//             return -1;
//         }
//         // Истек таймаут
//         return -2;
//     // Подключение произошло до таймаута
//     } else {
//         // Проверяем, что сокет действительно подключен
//         int error;
//         socklen_t len = sizeof(error);
//         // Получаем информацию об ошибке в параметрах сокета на уровне сокета
//         if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
//             perror("getsockopt() in connect_with_timeout()"); 
//             // Восстанавливаем флаги
//             if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//                 perror("fcntl() for set old flags in connect_with_timeout()");
//                 return -1;
//             }
//             return -1;
//         }
//         if (error != 0) {
//             // Выводим сообщение об ошибке подключения
//             fprintf(stderr, "connect error: %s\n", strerror(error)); 
//             // Восстанавливаем флаги
//             if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//                 perror("fcntl() for set old flags in connect_with_timeout()");
//                 return -1;
//             }
//             return error;
//         }

//         // Восстанавливаем флаги
//         if (fcntl(sockfd, F_SETFL, old_flags) == -1) {
//             perror("fcntl() for set old flags in connect_with_timeout()");
//             return -1;
//         }

//         return 0;
//     }
// }

void* service_thread(void* _pool){
    struct ServicePool* pool = (struct ServicePool*) _pool;

    // Добавляем себя в список
    ServiceNode* me = service_list_create_node(pthread_self(), 0, 0, NULL, NULL);
    if (me == NULL){
        perror("Bind() in service_thread() was NOT trackable");
        pthread_exit((void*)EXIT_FAILURE);
    }

    sigset_t mask;
    sigemptyset(&mask);
    int sig = SIGUSR1;
    sigaddset(&mask, sig);
    //sigprocmask(SIG_BLOCK, (const sigset_t*)&mask, NULL);

    ServiceRequest received_request;
    // Цикл для обработки прерываний
    while(me->kill_thread_flag == 0){

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

        int port_number = ntohs(me->socket_endpoint.sin_port);

        // Добавляем себя в список готовых
        pthread_mutex_lock(&(pool->mutex));
        list_append_node((List*)&(pool->ready_list), (Node*)me);
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        pthread_mutex_unlock(&(pool->mutex));
        
        // Блокирующе ждём сообщения из очереди
        unsigned int prio;
        if (mq_receive(pool->queue_des, (char*)&received_request, sizeof(ServiceRequest), &prio) == -1){
            // Если функция была прервана сигналом
            pthread_mutex_lock(&(pool->mutex));
            if (errno == EINTR){
                // Если был установлен флаг заверщения (Послан сигнал завершения)
                if (me->kill_thread_flag != 0){
                    // Завершить поток
                    list_remove_node((List*)&(pool->ready_list), (Node*)(me));
                    list_append_node((List*)&(pool->dead_list), (Node*)(me));
                    fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
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
            perror("accept() in service_thread()");

            list_remove_node((List*)&(pool->ready_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            close(me->socket_fd);
            pthread_mutex_unlock(&(pool->mutex));

            pthread_exit((void*)EXIT_FAILURE);
        }

        // Проверяем, есть ли сигнал завершения
        pthread_mutex_lock(&(pool->mutex));
        list_remove_node((List*)&(pool->ready_list), (Node*)(me));
        if (me->kill_thread_flag == 0){
            // Добавляем себя в список активных
            list_append_node((List*)&(pool->active_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis active\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
        }
        else {
            // Добавляем себя в список мёртвых
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(me->socket_fd);
            return NULL;
        }

        me->client_endpoint.sin_addr.s_addr = received_request.net_addr;
        me->client_endpoint.sin_port = received_request.net_port;
        me->client_endpoint.sin_family = received_request.sock_type;
        me->client_endpoint.sin_zero[0] = '\0';

        if (usleep(100000) == -1){
            pthread_mutex_lock(&(pool->mutex));
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(me->socket_fd);

            if (errno == EINTR){
                if (me->kill_thread_flag != 0){
                    return NULL;
                }else {
                    pthread_exit((void*)EXIT_FAILURE);
                }
            }
            else {
                pthread_exit((void*)EXIT_FAILURE);
            }
        }

        if (connect(me->socket_fd, (struct sockaddr*)&(me->client_endpoint), len) == -1){
            pthread_mutex_lock(&(pool->mutex));
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(me->socket_fd);

            pthread_exit((void*)EXIT_FAILURE);
        }

        // // Блокирующе подключаемся к клиенту
        // int res = connect_with_timeout(me->socket_fd, (const struct sockaddr*)&(me->client_endpoint), client_len, CONNECT_TIMEOUT_USEC);
        // printf("RES: %d\n", res);
        // if (res == 0){
        //     ;
        // }
        // // Критическая ошибка
        // else if (res == -1){
        //     // Перемещаетмя в список мёртвых
        //     pthread_mutex_lock(&(pool->mutex));
        //     list_remove_node((List*)&(pool->active_list), (Node*)(me));
        //     list_append_node((List*)&(pool->dead_list), (Node*)me);
        //     fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead (critical)\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        //     pthread_mutex_unlock(&(pool->mutex));

        //     pthread_exit((void*)EXIT_FAILURE);
        // }
        // // Истёк таймаут
        // else if (res == -2){
        //     // Перемещаетмя в список готовых
        //     pthread_mutex_lock(&(pool->mutex));
        //     list_remove_node((List*)&(pool->active_list), (Node*)(me));
        //     list_append_node((List*)&(pool->ready_list), (Node*)me);
        //     fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready (timeout)\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        //     pthread_mutex_unlock(&(pool->mutex));

        //     continue;
        // }
        // // Прервана сигналом
        // else if (res == -3){
        //     // Если установлен флаг завершния
        //     if (me->kill_thread_flag != 0){
        //         pthread_mutex_lock(&(pool->mutex));
        //         list_remove_node((List*)&(pool->active_list), (Node*)(me));
        //         list_append_node((List*)&(pool->dead_list), (Node*)me);
        //         fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready (interrupted)\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        //         pthread_mutex_unlock(&(pool->mutex));

        //         return NULL;
        //     }
        //     else {
        //         continue;
        //     }
        // }
        // // Ошибка при подключении (connect())
        // else {
        //     // Перемещаетмя в список мёртвых
        //     pthread_mutex_lock(&(pool->mutex));
        //     list_remove_node((List*)&(pool->ready_list), (Node*)(me));
        //     list_append_node((List*)&(pool->dead_list), (Node*)me);
        //     fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead (other)\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        //     pthread_mutex_unlock(&(pool->mutex));

        //     pthread_exit((void*)EXIT_FAILURE);
        // }

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
            pthread_mutex_unlock(&(pool->mutex));
            close(me->socket_fd);

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
            pthread_mutex_unlock(&(pool->mutex));
            close(me->socket_fd);
            
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Отправляем текущее время
        ssize_t bytes_sended;
        while (1){
            pthread_mutex_lock(&(pool->mutex));
            bytes_sended = send(me->socket_fd, (const void*)&time_string, sizeof(struct sockaddr_in), 0);
            if (bytes_sended == -1){
                // Если функция была прервана сигналом
                if (errno == EINTR){
                    // Если был установлен флаг заверщения (Послан сигнал завершения)
                    if (me->kill_thread_flag != 0){
                        // Завершить поток
                        list_remove_node((List*)&(pool->active_list), (Node*)(me));
                        list_append_node((List*)&(pool->dead_list), (Node*)(me));
                        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);                 
                        pthread_mutex_unlock(&(pool->mutex));
                        close(me->socket_fd);
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
                pthread_mutex_unlock(&(pool->mutex));
                close(me->socket_fd);

                pthread_exit((void*)EXIT_FAILURE);
            }
            pthread_mutex_unlock(&(pool->mutex));
            break;
        }

        // ПРОВЕРКИ НА ФАКТ ОТПРАВКИ ДАННЫХ НЕТ
        

        // // Переводим себя в список ожидающих
        // pthread_mutex_lock(&(pool->mutex));
        // list_remove_node((List*)&(pool->active_list), (Node*)(me));
        // list_append_node((List*)&(pool->ready_list), (Node*)(me));
        // fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
        // pthread_mutex_unlock(&(pool->mutex));

        close(me->socket_fd);
    }

    return NULL;
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

/*Создать пул для потоков (создаёт очередь)
Возвращает
- указатель на структуру
- NULL в случае ошибки*/
ServicePool* service_pool_initialize(const char* queue_path){
    ServicePool* pool = (ServicePool*)malloc(sizeof(ServicePool));
    if (pool == NULL){
        perror("malloc() for ServicePool* in create_service_pool()");
        return NULL;
    }
    list_init((List*)&(pool->active_list));
    list_init((List*)&(pool->ready_list));
    list_init((List*)&(pool->dead_list));

    if (pthread_mutex_init(&(pool->mutex), NULL) != 0) {
        perror("pthread_mutex_init() for placement_mutex in create_service_pool()");
        free(pool);
        return NULL;
    }

    // Удаляем файл, если он существует
    // if (remove(queue_path) != 0) {
    //     perror("remove() in service_pool_initialize()\n");
    //     return NULL;
    // }
    // if (unlink(queue_path) != 0){
    //     perror("unlink() in service_pool_initialize()\n");
    //     return NULL;
    // }

    // // Создаем новый файл
    // FILE* queue_fd = fopen(queue_path, "a+");
    // if (queue_fd == NULL) {
    //     perror("fopen(queue_path), creation\n");
    //     return NULL;
    // }
    // // Закрываем файл после создания
    // fclose(queue_fd);

    // Инициализировать очередь
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    attr.mq_maxmsg = MAX_REQUESTS;
    attr.mq_msgsize = sizeof(struct ServiceRequest);
    // Копируем путь
    strncpy(pool->queue_path, queue_path, QUEUE_PATH_MAX);
    pool->queue_path[QUEUE_PATH_MAX-1] = 0;
    // Создаём очередь
    pool->queue_des = mq_open(pool->queue_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    if (pool->queue_des == ((mqd_t)-1)) {
        perror("mq_open() for queue creation in service_pool_initialize()\n");
        return NULL;
    }
    
    return pool;
}

/*Освободить пул (Освобождает память пула, НЕ удяляет узлы списков, НЕ удаляет потоки)
NO Thread Safety!
Возвращает 0 в случае успеха, EXIT_FAILURE при провале*/
int service_pool_free(ServicePool* pool){
    pthread_mutex_destroy(&(pool->mutex));

    if (mq_close(pool->queue_des) != 0){
        perror("mq_close() in service_pool_free()\n");
        return EXIT_FAILURE;
    }
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
