#include "../../include/server/listener_thread.h"

/*Ожидать и установить соединение (Для получения endpoint'a)
Возвращает
- 0 - в случае успеха
- -1 - в случае прерывания функции
- -2 - в случае ошибки в select()
- -3 - в случае ошибки в accept()
- -4 - в случае ошибки в recvfrom()*/
int accept_multiprotocol_connection(int stream_fd, int dgram_fd, ServiceRequest* request, int protocol){
    struct sockaddr_in endpoint;
    socklen_t size = sizeof(struct sockaddr_in);
    fd_set my_fds;
    int max_fd;
    char buffer[SERV_BUFFER_SZ];

    while (1){
        // Добавить дескрипторы в набор
        FD_ZERO(&my_fds);
        FD_SET(stream_fd, &my_fds);
        FD_SET(dgram_fd, &my_fds);
        max_fd = (stream_fd > dgram_fd) ? stream_fd : dgram_fd;

        // Ожидание событий на stream или dgram сокетах
        if (select(max_fd + 1, &my_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                return -1; // interrupted
            } else {
                perror("select() in accept_multiprotocol_connection()");
                return -2; // Ошибка select()
            }
        }

        // Ожидание событий на stream сокете
        if (FD_ISSET(stream_fd, &my_fds)) {
            int client_stream_fd = accept(stream_fd, (struct sockaddr*)&endpoint, &size);
            close(client_stream_fd);
            if (client_stream_fd == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Нет входящих соединений
                    continue;
                } else if (errno == EINTR) {
                    return -1; // interrupted
                } else {
                    perror("accept() in accept_multiprotocol_connection()");
                    return -3; // Ошибка accept()
                }
            } else {
                // Соединение принято
                (*request).net_addr = endpoint.sin_addr.s_addr;
                (*request).net_port = endpoint.sin_port;
                (*request).sock_type = SOCK_STREAM;
                (*request).protocol = protocol;
                return 0;
            }
        }

        // Ожидание событий на dgram сокете
        if (FD_ISSET(dgram_fd, &my_fds)) {
            memset(buffer, 0, SERV_BUFFER_SZ);
            int bytes_received = recvfrom(dgram_fd, (void*)buffer, SERV_BUFFER_SZ, 0, (struct sockaddr*)&endpoint, &size);
            if (bytes_received == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    // Нет входящих соединений
                    continue;
                } else if (errno == EINTR) {
                    return -1; // interrupted
                } else {
                    perror("recvfrom() in accept_multiprotocol_connection()");
                    return -4; // Ошибка recvfrom()
                }
            } else {
                // Соединение принято
                (*request).net_addr = endpoint.sin_addr.s_addr;
                (*request).net_port = endpoint.sin_port;
                (*request).sock_type = SOCK_DGRAM;
                (*request).protocol = protocol;
                return 0;
            }
        }
    }
}


void* listener_thread(void* args){
    struct ListenerThread* me = (struct ListenerThread*) args;
    int struct_len = sizeof(struct sockaddr_in);

    int stream_fd = prepare_stream_socket((struct sockaddr*)&(me->endpoint), struct_len, 0);
    if (stream_fd == -1){
        perror("prepare_stream_socket() in listener_thread()");
        pthread_exit((void*)EXIT_FAILURE);
    }

    int dgram_fd = prepare_dgram_socket((struct sockaddr*)&(me->endpoint), struct_len, 0);
    if (dgram_fd == -1){
        perror("prepare_dgram_socket() in listener_thread()");
        close(stream_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    ServiceRequest request;
    int res;

    while(me->kill_thread_flag == 0){
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tis waiting\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
        // Ожидать соединения (получить endpoint)
        res = accept_multiprotocol_connection(stream_fd, dgram_fd, &request, 0);
        if (res < 0) {
            // Функция прервана сигналом
            if (res == -1){
                // Установлен флаг завершения
                if (me->kill_thread_flag != 0){
                    // Завершить поток успешно
                    close(stream_fd);
                    close(dgram_fd);
                    fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tis dead\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
                    return 0;
                }
                else {
                    // Ещё раз
                    continue;
                }
            }
            else {
                // Завершить поток аварийно
                perror("accept_multiprotocol_connection() in listener_thread()");
                close(stream_fd);
                close(dgram_fd);
                pthread_exit((void*)EXIT_FAILURE);
            }
        }
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\treceived connection\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));

        // Адрес клиента получен, отправляем заявку в очередь
        while(1){
            if (mq_send(me->pool->queue_des, (const char*)&request, sizeof(ServiceRequest), 1) == -1){
                // Если прерван сигналом
                if (errno == EINTR){
                    // Если установлен флаг завершения
                    if (me->kill_thread_flag != 0){
                        close(stream_fd);
                        close(dgram_fd);
                        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tis dead\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
                        return NULL;
                    }
                    else {
                        continue;
                    }
                }
                else {
                    perror("mq_send() in listener_thread()");       
                    close(stream_fd);
                    close(dgram_fd);
                    pthread_exit((void*)EXIT_FAILURE);                    
                }
            }
            break;
        }

        // Заного
    }

    return NULL;
}

/*Мягко убить слушающий поток
(Мягко - разорвать соединение и завершиться)
Возвращает 
- 0
- EXIT_FAILURE в случае ошибки*/
int listener_terminate(ListenerThread* listener) {
    listener->kill_thread_flag = 1;
    // Послать сигнал для разблокировки блокирующих функций
    if (pthread_kill(listener->tid, SIGUSR1) != 0){
        perror("Critical pthread_kill() in main()");
        return EXIT_FAILURE;
    }
    return 0;
}