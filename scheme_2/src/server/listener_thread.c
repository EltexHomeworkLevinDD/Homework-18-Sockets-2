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
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\treceived connection\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));

        // Если accept успешно завершился, требуется получить адрес, на который переподключится клиент
        // Найти свободный (спящий) обслуживающий поток и пробудить
        ServiceNode* service = service_pool_threads_wake_up_head(me->pool);
        // Если свободных потоков нет
        if (service == NULL){
            fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\trefused to connect\n", INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port));
            close(client_fd);
            continue;
        }
        // Если произошла ошибка
        else if (service == ((ServiceNode*)EXIT_FAILURE)){
            perror("service_pool_threads_wake_up_head() in listener_thread()");       
            close(socket_fd);
            pthread_exit((void*)EXIT_FAILURE);
        }

        // Посылаем сообщение подключённому клиенту, на какой адрес ему нужно переподключиться
        while(1){
            ssize_t bytes_sended = send(client_fd, (const void*)&(service->socket_endpoint), sizeof(struct sockaddr_in), 0);
            if (bytes_sended == -1){
                perror("send() new endpoint to client in listener_thread()");
                close(client_fd);
                close(socket_fd);
                pthread_exit((void*)EXIT_FAILURE);
            }
            if (bytes_sended == -1){
                // Если функция была прервана сигналом
                if (errno == EINTR){
                    // Если был установлен флаг заверщения (Послан сигнал завершения)
                    if (me->kill_thread_flag != 0){
                        // Завершить поток
                        close(client_fd);
                        return NULL;
                    }
                    // Если не был установлен флаг заверщения (Прерывание из-за другого сигнала)
                    else {
                        // Перезапустить операцию
                        close(client_fd);
                        continue;
                    }
                }
                // Необработанная критическая ошибка
                perror("send() in listener_thread()");
                close(client_fd);
                close(socket_fd);
                pthread_exit((void*)EXIT_FAILURE);
            }
            break;
        }
        fprintf(stderr, "Listener\t\t\tip[%*s]\tport[%5d]\tredirected the connection to Service\tpool[%p]\tip[%*s]\tport[%5d]\n", 
            INET_ADDRSTRLEN, SERV_IP_S, ntohs(me->endpoint.sin_port),
            (void*)(me->pool), INET_ADDRSTRLEN, SERV_IP_S, ntohs(service->socket_endpoint.sin_port));
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