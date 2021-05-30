#pragma once
#include "main.h"

char* getIPAddressString(uint32_t addr){
    uint8_t *uaddr = (uint8_t *)(&addr);
    char *data;
    asprintf(&data,"%d.%d.%d.%d",uaddr[0],uaddr[1],uaddr[2],uaddr[3]);
    return data;
}

int createListenerSocket(char *ipaddress,int port,int nconcurrent)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    inet_pton(AF_INET,ipaddress,&address.sin_addr.s_addr);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, nconcurrent) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

