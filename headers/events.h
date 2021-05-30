#pragma once
#include "main.h"

void addListenerEvent(int kq,int socketFd){
    KEvent event;
    EV_SET(&event, socketFd, EVFILT_READ, EV_ADD, 0 , 0 , NULL);
    kevent(kq,&event,1,NULL,0,NULL);
}

void addConnectionEvent(int kq,int connFd,sockaddr_in* address){
    KEvent event;
    EV_SET(&event,connFd,EVFILT_READ,EV_ADD,0,0,address);
    //printf("%d\n",connFd);
    int ret = kevent(kq,&event,1,NULL,0,NULL);
    if (ret == -1) {
        printf("%d\n",errno);
        err(EXIT_FAILURE, "kevent register");
        
    }
      
    if (event.flags & EV_ERROR)
        errx(EXIT_FAILURE, "Event error: %s", strerror(event.data));
}