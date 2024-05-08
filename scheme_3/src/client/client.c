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

    // Устанавливаем опцию SO_REUSEADDR для переиспользования адреса
    int optval = 1;
    if (setsockopt(own_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    // Заполняем структуру адреса сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_zero[0] = '\0';
    inet_pton(AF_INET, SERV_IP_S, &server_addr.sin_addr);

    socklen_t len = sizeof(struct sockaddr_in);

    // Неблокирующе пытаемся соедениться
    if (connect(own_fd, (const struct sockaddr*)&server_addr, len) == -1){
        perror("connect()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in own_endpoint;
    if (getsockname(own_fd, (struct sockaddr*)&own_endpoint, &len) == -1){
        perror("getsockname()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    close(own_fd);

    // Пересоздаём сокет

    own_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (own_fd == -1){
        perror("socket creation");
        exit(EXIT_FAILURE);
    }  

    // Устанавливаем опцию SO_REUSEADDR для переиспользования адреса
    optval = 1;
    if (setsockopt(own_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    if (bind(own_fd, (const struct sockaddr*)&own_endpoint, len) == -1){
        perror("bind()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    // Начинаем прослушивание входящих соединений
    if (listen(own_fd, 1) == -1) {
        perror("listen()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv;
    socklen_t serv_len = sizeof(serv);
    int server_fd = accept(own_fd, (struct sockaddr*)&serv, &serv_len);
    if (server_fd == -1){
        perror("accept()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    char recv_buffer[SERV_BUFFER_SZ];
    ssize_t bytes_received = recv(server_fd, (void*)&recv_buffer, SERV_BUFFER_SZ, 0);
    switch (bytes_received)
    {
    // Ошибка
    case -1:
        perror("recv()");
        close(server_fd);
        close(own_fd);
        exit(EXIT_FAILURE);
        break;
    // Соединение разорвано второй стороной
    case 0:
        close(server_fd);
        close(own_fd);
        printf("The connection is broken by the second party\n");
        exit(EXIT_SUCCESS);
        break;
    // Обрабатываем данные
    default:
        printf("Received ctime: '%s'\n", recv_buffer);
        break;
    }

    close(server_fd);
    close(own_fd);

    return 0;
}