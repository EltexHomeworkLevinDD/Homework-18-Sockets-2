#include "../../include/server/listener_thread.h"

void* listener_thread(void* args){
    struct ListenerThread* me = (struct ListenerThread*) args;

    // Создаём слушающий сетевой потоковый сокет
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1){
        perror("socket creation in listener_thread()");
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Назначаем сокету адрес
    if (bind(socket_fd, (const struct sockaddr*)&(me->endpoint), sizeof(struct sockaddr_in)) == -1){
        perror("Bind() in listener_thread()");
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Переводим в режим ожидания
    if (listen(socket_fd, SERV_BACKLOG) == -1){
        perror("listen() in listener_thread()");
        close(socket_fd);
        pthread_exit((void*)EXIT_FAILURE);
    }

    // Endpoint подключённого клиента 
    struct sockaddr_in client_enpoint;
    ServiceRequest request;
    socklen_t client_len = sizeof(struct sockaddr_in);
    // Активный дескриптор слушающего сокета
    int client_fd; 

    while(me->kill_thread_flag == 0){
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tis waiting\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
        // Блокирующе подключаем клиента
        client_fd = accept(socket_fd, (struct sockaddr*)&client_enpoint, &client_len);
        // Если accept неуспешно завершился
        if (client_fd == -1){
            // Если функция была прервана сигналом
            if (errno == EINTR){
                // Если был установлен флаг заверщения (Послан сигнал завершения)
                if (me->kill_thread_flag != 0){
                    // Завершить поток
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
            close(socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }
        if (me->kill_thread_flag != 0){
            break;
        }

        // Поместить заявку в очередь
        request.net_addr = client_enpoint.sin_addr.s_addr;
        request.net_port = client_enpoint.sin_port;
        request.sock_type = AF_INET;

        while(1){
            if (mq_send(me->pool->queue_des, (const char*)&request, sizeof(ServiceRequest), 1) == -1){
                // Если прерван сигналом
                if (errno == EINTR){
                    // Если установлен флаг завершения
                    if (me->kill_thread_flag != 0){
                        close(client_fd);
                        close(socket_fd);
                        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tis dead\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
                        return NULL;
                    }
                    else {
                        continue;
                    }
                }
                perror("mq_send() in listener_thread()");       
                close(client_fd);
                close(socket_fd);
                pthread_exit((void*)EXIT_FAILURE);
            }
            break;
        }


        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\treceived connection\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));


        // ПРОВЕРКИ НА ФАКТ ОТПРАВКИ ДАННЫХ НЕТ

        // Закрыть активный дескриптор слушающего сокета
        close(client_fd);
    }

    close(socket_fd);
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