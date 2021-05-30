#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <kqueue/sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <aio.h>
#include <assert.h>

typedef struct kevent KEvent;
typedef struct sockaddr_in sockaddr_in;
typedef struct dirent dirent;
typedef struct stat Stat;

typedef struct SendFileData {
    int connFd;
    int fileFd;
    long size;
    long offset;
}SendFileData;
SendFileData* createSendFileData(int connFd,int fileFd,int size);
void addConnectionWriteEvent(SendFileData *sfd);

int kq;

// ------------------------------------------------------- HTML and Files -----------------------------------------------
char *base;
char *errTemplate;
char *listTemplate;
char *itemtemplate = 
"<tr>"
"<td><a class=\"%s\" href=\"%s%s\"> %s </a></td>"
"<td><span>%d</span></td>"
"<td><span>%d</span></td>"
"</tr>";

char *retrievePath(int connFd,int len){
    char *data = malloc(len+1);
    data[len]='\0';
    read(connFd,data,len);
    strtok(data," ");
    char *next = strtok(NULL," ");
    char *path;
    asprintf(&path,"%s",next);
    free(data);
    return path;
}

char *errResponse(char *path){
    char *a,*b,*c;
    char *status;
    int code;
    if (errno==ENOENT){
        code=404;
        status="Not Found";
        asprintf(&a,"%s %s","Not found: ",path);
    }
    else if (errno==ENOTDIR) {
        code = 422;
        status="Unprocessable Entity";
        asprintf(&a,"%s %s","Not a directory: ",path);
    }
    else {
        code = 403;
        status = "Forbidden";
        asprintf(&a,"%s %s","Forbidden: ",path);
    }
    asprintf(&b,errTemplate,a,a);
    free(a);
    asprintf(&c,"HTTP/1.1 %d %s\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n%s",code,status,strlen(b),b);
    free(b);
    return c;
}

Stat getFileStats(char *path){
    Stat stats;
    stat(path,&stats);
    return stats;
}
char *getExt(char *fname){
    for (size_t i=0;i<strlen(fname);i++){
        if (fname[i]=='.'){
            return fname +i+1;
        }
    }
    return NULL;
}

char *fileResponse(int connFd,char *path){
    char *fname = basename(path);
    char *a;
    int fileFd = open(path,O_RDONLY);
    int len = getFileStats(path).st_size;
    asprintf(&a,"HTTP/1.1 200 OK\r\nContent-Disposition: inline; filename=\"%s\"\r\nContent-Length: %d\r\n\r\n",fname,len);
    SendFileData *sfd = createSendFileData(connFd,fileFd,len);
    addConnectionWriteEvent(sfd);
    return a;
}

char *redirectResponse(char *path){
    char *a;
    asprintf(&a,"HTTP/1.1 302 Redirect\r\nLocation: %s/\r\n\r\n",path);
    return a;
}

char *listResponse(DIR *dp,char *path){
    char *buf =0;
    int n = 0;
    dirent *ep;
    while (ep = readdir(dp))
    {
        if (strcmp(ep->d_name,".")==0){
            continue;
        }
        char *fullpath;asprintf(&fullpath,"%s%s%s",base,path,ep->d_name);
        Stat stats = getFileStats(fullpath);
        free(fullpath);
        char *a;
        int an;
        if (ep->d_type==DT_DIR){
            an =asprintf(&a,itemtemplate,"",ep->d_name,"/",ep->d_name,stats.st_size,stats.st_mtim);
        }
        else {
            an =asprintf(&a,itemtemplate,"",ep->d_name,"",ep->d_name,stats.st_size,stats.st_mtim); 
        }
        buf = realloc(buf,n+an+1);
        strcpy(buf+n,a);
        n+=an;
        free(a);
    }
    char *ret;
    asprintf(&ret,listTemplate,path,path,buf);
    free(buf);
    char *reth;
    asprintf(&reth,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n%s",strlen(ret),ret);
    free(ret);
    return reth;
}

void load(char *path,char **loc){
    FILE *f = fopen(path,"r");
    fseek(f,0,SEEK_END);
    int len = ftell(f);
    fseek(f,0,SEEK_SET);
    *loc =malloc(len+1);
    (*loc)[len] = '\0';
    fread(*loc,1,len,f);
}

void connHandler(KEvent event){
    int size = event.data;
    int connFd = event.ident;
    char *path = retrievePath(connFd,size);
    char *fullpath;
    asprintf(&fullpath,"%s%s",base,path);
    char *resp;
    DIR *dp =opendir(fullpath);
    if (!dp && errno==ENOTDIR)
    {
        resp = fileResponse(connFd,fullpath);
    }
    else if (dp && path[strlen(path)-1]!='/'){
        resp = redirectResponse(path);
    }
    else if (dp) {
        resp = listResponse(dp,path);
    }
    else {
        resp = errResponse(path);
    }
    free(path);
    free(fullpath);
    closedir(dp);
    printf("%s\n",resp);
    write(connFd,resp,strlen(resp));
    free(resp);
    // close(connFd);
}

// ------------------------------------------------------- SOCKET -----------------------------------------------

//  struct kevent {
// 	     uintptr_t ident;	     /*	identifier for this event */
// 	     short     filter;	     /*	filter for event */
// 	     u_short   flags;	     /*	action flags for kqueue	*/
// 	     u_int     fflags;	     /*	filter flag value */
// 	     intptr_t  data;	     /*	filter data value */
// 	     void      *udata;	     /*	opaque user data identifier */
//      };



SendFileData* createSendFileData(int connFd,int fileFd,int size){
    SendFileData *sfd = malloc(sizeof(SendFileData));
    sfd->connFd = connFd;
    sfd->fileFd = fileFd;
    sfd->size = size;
    sfd->offset = 0;
    return sfd;
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

void addListenerEvent(int socketFd){
    KEvent event;
    EV_SET(&event, socketFd, EVFILT_READ, EV_ADD, 0 , 0 , NULL);
    kevent(kq,&event,1,NULL,0,NULL);
}

void addConnectionReadEvent(int connFd,sockaddr_in* address){
    KEvent event;
    EV_SET(&event,connFd,EVFILT_READ,EV_ADD,0,0,address);
    kevent(kq,&event,1,NULL,0,NULL);
}

void addConnectionWriteEvent(SendFileData *sfd){
    KEvent event;
    EV_SET(&event,sfd->connFd,EVFILT_WRITE,EV_ADD|EV_ONESHOT,0,0,NULL);
    kevent(kq,&event,1,NULL,0,NULL);
    printf("done\n");
}

int main(int argc,char **argv){
    DIR *dp;
    if (argc==1){
        base = getcwd(NULL,0);
    }
    else if (dp=opendir(argv[1])){
        base = argv[1];
        closedir(dp);
    }
    else {
        printf("Cannot open %s\n",argv[1]);
        return 1;
    }
    
    printf("Serving on %s\n",base);

    load("templates/err.html",&errTemplate);
    load("templates/list.html",&listTemplate);

    kq = kqueue();
    int listenSocketFd = createListenerSocket("0.0.0.0",8080,10);
    addListenerEvent(listenSocketFd);

    for (int i=0;i<10;i++){
        KEvent event;
        kevent(kq,NULL,0,&event,1,NULL);
        if (event.ident==listenSocketFd){
            printf("Accept\n");
            int newSocket;
            sockaddr_in *address = malloc(sizeof(address));
            socklen_t addrlen = sizeof(address);
            if ((newSocket = accept(listenSocketFd, address,&addrlen)) < 0){
                printf("Accept error\n");
            }
            else {
                int flags = fcntl(newSocket, F_GETFL, 0);
                assert(flags >= 0);
                fcntl(newSocket, F_SETFL, flags | O_NONBLOCK);
                addConnectionReadEvent(newSocket,address);
            }
            printf("Accepted\n");
        }
        else if(event.filter==EVFILT_WRITE){
            SendFileData *sfd = (SendFileData*)event.udata;
            sendfile(sfd->connFd,sfd->fileFd,&sfd->offset,1000);
            if (sfd->offset<sfd->size){
                addConnectionWriteEvent(sfd);
            }
            else {
                close(sfd->fileFd);
            }
        }
        else if (event.flags & EV_EOF){
            if (event.udata){
                printf("close conn\n");
                free(event.udata);
            }
            event.flags = EV_DELETE;
            kevent(kq,&event,1,NULL,0,NULL);
        }
        else {
            // int size = event.data;
            // char *data;
            // sockaddr_in address = *(sockaddr_in*)event.udata;
            // char *ip = getIPAddressString(address.sin_addr.s_addr);
            // int l =asprintf(&data,"I recieved a message of size %d from <%d>%s:%d",size,event.ident,ip,address.sin_port);
            // printf("%s\n",data);
            // char *recv = malloc(size)+1;
            // read(event.ident,recv,size);
            // recv[size] = '\0';
            // printf("%s\n",recv);
            // write(event.ident,data,l);
            connHandler(event);
        }
    }
    free(errTemplate);
    free(listTemplate);
}

