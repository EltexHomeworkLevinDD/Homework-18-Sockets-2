#include "../../include/server/service_thread.h"

/*Создаёт tcp сокет и переводит его в слушающий режим, устанавливает O_NONBLOCK и SO_REUSEADDR
Возвращает
- дескриптор сокета
- -1 в случае провала*/
int prepare_stream_socket(struct sockaddr* endpoint, socklen_t size, int protocol){
    // Создаём слушающий сетевой потоковый сокет
    int socket_fd = socket(AF_INET, SOCK_STREAM, protocol);
    if (socket_fd == -1){
        perror("socket creation in prepare_stream_socket()");
        return -1;
    }

    // Получаем флаги сокета
    int old_flags = fcntl(socket_fd, F_GETFL, 0);
    if (old_flags == -1) {
        perror("fcntl() for get old flags in prepare_stream_socket()");
        return -1;
    }
    // Устанавливаем дополнительные нужные флаги (Неблокирующий режим)
    int new_flags = old_flags | O_NONBLOCK;
    if (fcntl(socket_fd, F_SETFL, new_flags) == -1) {
        perror("fcntl() for set new flags in prepare_stream_socket()");
        return -1;
    }

    // Устанавливаем возможность переиспользования адреса
    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt() in prepare_stream_socket()");
        close(socket_fd);
        return -1;
    }

    // Назначаем сокету адрес
    if (bind(socket_fd, (const struct sockaddr*)endpoint, size) == -1){
        perror("bind() in prepare_stream_socket()");
        close(socket_fd);
        return -1;
    }

    // Переводим в режим ожидания
    if (listen(socket_fd, SERV_BACKLOG) == -1){
        perror("listen() in prepare_stream_socket()");
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

/*Создаёт udp сокет, устанавливает O_NONBLOCK и SO_REUSEADDR
Возвращает
- дескриптор сокета
- -1 в случае провала*/
int prepare_dgram_socket(struct sockaddr* endpoint, socklen_t size, int protocol){
    // Создаём локальный потоковый сокет
    int socket_fd = socket(AF_INET, SOCK_DGRAM, protocol);
    if (socket_fd == -1){
        perror("socket creation");
        return -1;
    }

    // Получаем флаги сокета
    int old_flags = fcntl(socket_fd, F_GETFL, 0);
    if (old_flags == -1) {
        perror("fcntl() for get old flags in prepare_dgram_socket()");
        return -1;
    }
    // Устанавливаем дополнительные нужные флаги (Неблокирующий режим)
    int new_flags = old_flags | O_NONBLOCK;
    if (fcntl(socket_fd, F_SETFL, new_flags) == -1) {
        perror("fcntl() for set new flags in prepare_dgram_socket()");
        return -1;
    }

    // Устанавливаем возможность переиспользования адреса
    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt() in prepare_dgram_socket()");
        close(socket_fd);
        return -1;
    }

    // Назначаем сокету адрес
    if (bind(socket_fd, (const struct sockaddr*)endpoint, size) == -1){
        perror("Bind() in prepare_dgram_socket()");
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

/*Соедениться с клиентом
Возвращает
- 0 - в случае успеха
- -1 - в случае прерывания функции
- -2 - в случае ошибки в select()
- -3 - в случае ошибки в connect()
- -4 - в случае ошибки в sendto()*/
int connect_multiprotocol_connection(int stream_fd, int dgram_fd, struct sockaddr_in* client_endpoint, int sock_type){
    socklen_t size = sizeof(struct sockaddr_in);
    char buffer[SERV_BUFFER_SZ];

    while (1){

        // Ожидание событий на stream сокете
        if (sock_type == SOCK_STREAM) {
            if (connect(stream_fd, (struct sockaddr*)client_endpoint, size) == -1){
                printf("--HUI 1\n");
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Нет входящих соединений
                    continue;
                } else if (errno == EINTR) {
                    return -1; // interrupted
                } else {
                    perror("connect() in connect_multiprotocol_connection()");
                    return -3; // Ошибка connect()
                }
            } else {
                // Соединение принято
                return 0;
            }
        } else if (sock_type == SOCK_DGRAM){
            memset(buffer, 0, SERV_BUFFER_SZ);
            int bytes_received = sendto(dgram_fd, (void*)buffer, SERV_BUFFER_SZ, 0, (const struct sockaddr*)client_endpoint, size);
            if (bytes_received == -1) {
                printf("--HUI 2\n");
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Нет входящих соединений
                    continue;
                } else if (errno == EINTR) {
                    return -1; // interrupted
                } else {
                    perror("sendto() in connect_multiprotocol_connection()");
                    return -4; // Ошибка sendto()
                }
            } else {
                // Соединение принято
                return 0;
            }
        }
    }
}

/*Отправить данные независимо от протокола
- 0 - в случае успеха
- -1 - в случае прерывания операции
- -2 - в случае необработанной ошибки*/
int send_multiprotocol(int sockfd, const void *data, size_t data_size, struct sockaddr_in* endpoint, int sock_type) {
    ssize_t bytes_sent;

    // Отправка данных по потоковому сокету
    if (sock_type == SOCK_STREAM) {
        bytes_sent = send(sockfd, data, data_size, 0);
    }
    // Отправка данных по датаграммному сокету
    else if (sock_type == SOCK_DGRAM) {
        bytes_sent = sendto(sockfd, data, data_size, 0, (struct sockaddr *)endpoint, sizeof(struct sockaddr_in));
    }

    if (bytes_sent == -1){
        if (errno == EINTR){
            return -1;
        }
        else {
            return -2;
        }
    }

    return 0;
}

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
    int stream_fd;
    int dgram_fd;

    // Создать endpoint
    socklen_t len = sizeof(struct sockaddr_in);
    me->socket_endpoint.sin_family = AF_INET;
    inet_pton(AF_INET, SERV_IP_S, &(me->socket_endpoint.sin_addr));
    me->socket_endpoint.sin_port = 0;
    me->socket_endpoint.sin_zero[0] = '\0';

    // Создать dgram сокет
    dgram_fd = socket(AF_INET, SOCK_DGRAM, received_request.protocol);
    if (dgram_fd == -1){
        perror("socket() for dgram in service_thread()");

        pthread_mutex_lock(&(pool->mutex));
        list_append_node((List*)&(pool->dead_list), (Node*)(me));
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, 0);
        pthread_mutex_unlock(&(pool->mutex));

        pthread_exit((void*)EXIT_FAILURE);
    }

    // Устанавливаем возможность переиспользования адреса
    int optval = 1;
    if (setsockopt(dgram_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt() for dgram in service_thread()");

        pthread_mutex_lock(&(pool->mutex));
        list_append_node((List*)&(pool->dead_list), (Node*)(me));
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, 0);
        pthread_mutex_unlock(&(pool->mutex));

        close(dgram_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Получаем адрес обслуживающего сокета
    if (getsockname(dgram_fd, (struct sockaddr*)&(me->socket_endpoint), &len) != 0){
        perror("getsockname() for dgram in service_thread()");

        pthread_mutex_lock(&(pool->mutex));
        list_append_node((List*)&(pool->dead_list), (Node*)(me));
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, 0);
        pthread_mutex_unlock(&(pool->mutex));
        
        close(dgram_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    int port_number = ntohs(me->socket_endpoint.sin_port);

    // Цикл для обработки прерываний
    while(me->kill_thread_flag == 0){
        // Создать stream сокет
        // Создаём слушающий сетевой потоковый сокет
        stream_fd = socket(AF_INET, SOCK_STREAM, received_request.protocol);
        if (stream_fd == -1){
            perror("socket creation in service_thread()");

            pthread_mutex_lock(&(pool->mutex));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));

            close(dgram_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Устанавливаем возможность переиспользования адреса
        int optval = 1;
        if (setsockopt(stream_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            perror("setsockopt() in service_thread()");

            pthread_mutex_lock(&(pool->mutex));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));

            close(dgram_fd);
            close(stream_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Назначаем сокету адрес
        if (bind(stream_fd, (const struct sockaddr*)&(me->socket_endpoint), len) == -1){
            perror("bind() in service_thread()");

            pthread_mutex_lock(&(pool->mutex));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));

            close(dgram_fd);
            close(stream_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Добавляем себя в список готовых
        pthread_mutex_lock(&(pool->mutex));
        list_append_node((List*)&(pool->ready_list), (Node*)me);
        fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis ready\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, 0);
        pthread_mutex_unlock(&(pool->mutex));

        printf("--READY\n");

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
                    close(dgram_fd);
                    close(stream_fd);
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
            perror("mq_receive() in service_thread()");

            list_remove_node((List*)&(pool->ready_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)(me));
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            close(dgram_fd);
            close(stream_fd);
            pthread_mutex_unlock(&(pool->mutex));

            pthread_exit((void*)EXIT_FAILURE);
        }

        printf("--QUEUE pick up\n");

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
            close(dgram_fd);
            close(stream_fd);
            return NULL;
        }
        
        // Ждём, время, перед подключением
        if (usleep(100000) == -1){
            pthread_mutex_lock(&(pool->mutex));
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(dgram_fd);
            close(stream_fd);

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

        int res;

        me->client_endpoint.sin_addr.s_addr = received_request.net_addr;
        me->client_endpoint.sin_family = AF_INET;
        me->client_endpoint.sin_port = received_request.net_port;
        me->client_endpoint.sin_zero[0] = '\0';

        printf("--READY TO CONNECT\n");

        res = connect_multiprotocol_connection(stream_fd, dgram_fd, (struct sockaddr_in*)&(me->client_endpoint), received_request.sock_type);
        if (res < 0) {
            pthread_mutex_lock(&(pool->mutex));
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(dgram_fd);
            close(stream_fd);
            // Если функция прервана
            if (res == -1){
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

        printf("--CONNECTED\n");

        int multi_fd = received_request.sock_type == SOCK_STREAM ? stream_fd : dgram_fd;

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
            close(dgram_fd);
            close(stream_fd);

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
            close(dgram_fd);
            close(stream_fd);
            
            pthread_exit((void*)EXIT_FAILURE);
        }

        res = send_multiprotocol(multi_fd, (const void*)&time_string, SERV_BUFFER_SZ, &(me->client_endpoint), received_request.sock_type);
        if (res < 0) {
            pthread_mutex_lock(&(pool->mutex));
            // Добавляем себя в список мёртвых
            list_remove_node((List*)&(pool->active_list), (Node*)(me));
            list_append_node((List*)&(pool->dead_list), (Node*)me);
            fprintf(stderr, "Service\tpool[%p]\tip[%*s]\tport[%5d]\tis dead\n", (void*)pool, INET_ADDRSTRLEN, SERV_IP_S, port_number);
            pthread_mutex_unlock(&(pool->mutex));
            close(dgram_fd);
            close(stream_fd);
            // Если функция прервана
            if (res == -1){
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

        printf("--TIME SENDED, END\n");

        close(stream_fd);
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
