#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../../include/server/servpar.h"

int main() {
    // Создаём локальный датаграммный сокет
    int own_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (own_fd == -1) {
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

    // Отправляем датаграмму на слушающий сервер (для подключения)
    char buffer[SERV_BUFFER_SZ];
    ssize_t bytes_sent = sendto(own_fd, (const void*)&buffer, SERV_BUFFER_SZ, 0, (const struct sockaddr*)&server_addr, sizeof(struct sockaddr_in));
    if (bytes_sent == -1) {
        perror("sendto()");
        close(own_fd);
        exit(EXIT_FAILURE);
    }

    // Принимаем датаграмму с обслуживающего сервера (для подключения)
    struct sockaddr_in serv;
    socklen_t serv_len = sizeof(serv);
    ssize_t bytes_received = recvfrom(own_fd, (void*)&buffer, SERV_BUFFER_SZ, 0, (struct sockaddr*)&serv, &serv_len);
    switch (bytes_received) {
        // Ошибка
        case -1:
            perror("recvfrom()");
            close(own_fd);
            exit(EXIT_FAILURE);
            break;
        // Обрабатываем данные
        default:
            break;
    }

    // Принимаем датаграмму с обслуживающего сервера (данные)
    bytes_received = recvfrom(own_fd, (void*)&buffer, SERV_BUFFER_SZ, 0, (struct sockaddr*)&serv, &serv_len);
    switch (bytes_received) {
        // Ошибка
        case -1:
            perror("recvfrom()");
            close(own_fd);
            exit(EXIT_FAILURE);
            break;
        // Обрабатываем данные
        default:
            printf("Received ctime: '%s'\n", buffer);
            break;
    }

    close(own_fd);

    return 0;
}
