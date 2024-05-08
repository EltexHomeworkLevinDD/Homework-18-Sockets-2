#ifndef SERVPAR_H
#define SERVPAR_H

#define SERV_IP_S       "127.0.0.1"
#define SERV_PORT       44555
#define SERV_BACKLOG    5
#define SERV_BUFFER_SZ  100
#define SERV_PRECLOSE_TIMEOUT 10000 // [mcs] (10000 = 10 ms)
#define MAX_REQUESTS    10
#define CONNECT_TIMEOUT_USEC 10000

#endif//SERVPAR_H