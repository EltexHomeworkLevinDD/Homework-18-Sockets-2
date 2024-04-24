#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../../include/server/servpar.h"


int main(){
    // Создаём локальный потоковый сокет
    int own_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (own_fd == -1){
        perror("socket creation");
        exit(EXIT_FAILURE);
    }

    // Заполняем структуру адреса сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_zero[0] = '\0';
    inet_pton(AF_INET, SERV_IP_S, &server_addr.sin_addr);

    // Неблокирующе пытаемся соедениться
    if (connect(own_fd, (const struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr)) == -1){
        perror("connect()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }
    // Блокирующе получаем новый endpoint
    struct sockaddr_in new_endpoint;
    ssize_t bytes_received = recv(own_fd, (void*)&new_endpoint, sizeof(struct sockaddr_in), 0);
    switch (bytes_received)
    {
    // Ошибка
    case -1:
        perror("recv");
        close(own_fd);
        exit(EXIT_FAILURE);
        break;
    // Соединение разорвано второй стороной
    case 0:
        close(own_fd);
        printf("The connection is broken by the second party\n");
        exit(EXIT_SUCCESS);
        break;
    // Обрабатываем данные
    default:
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &(new_endpoint.sin_addr.s_addr), ip_str, INET_ADDRSTRLEN) == NULL) {
            perror("inet_ntop() failed");
            exit(EXIT_FAILURE);
        }
        unsigned short port_str = ntohs(new_endpoint.sin_port);

        printf("Received ip: '%s':%hu\n", ip_str, port_str);
        break;
    }

    close(own_fd);

    // Пересоздаём сокет

    own_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (own_fd == -1){
        perror("socket creation");
        exit(EXIT_FAILURE);
    }  

    sleep(1);

    // Неблокирующе пытаемся подключиться к новому endpoint'у
    if (connect(own_fd, (const struct sockaddr*)&new_endpoint, (socklen_t)sizeof(new_endpoint)) == -1){
        perror("connect()");
        close(own_fd);
        exit(EXIT_FAILURE);
    } 

    char recv_buffer[SERV_BUFFER_SZ];
    bytes_received = recv(own_fd, (void*)&recv_buffer, SERV_BUFFER_SZ, 0);
    switch (bytes_received)
    {
    // Ошибка
    case -1:
        perror("recv");
        close(own_fd);
        exit(EXIT_FAILURE);
        break;
    // Соединение разорвано второй стороной
    case 0:
        close(own_fd);
        printf("The connection is broken by the second party\n");
        exit(EXIT_SUCCESS);
        break;
    // Обрабатываем данные
    default:
        printf("Received ctime: '%s'\n", recv_buffer);
        break;
    }

    close(own_fd);

    return 0;
}