#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

//定义服务器IP地址
#define SERVER_IP INADDR_ANY
//定义listen等待队列
#define WAIT_QUEUE 5

typedef struct Arg{
    int fd;
    struct sockaddr_in addr;
}Arg;

int StartUp(char* port){
    //创建服务器套接字
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){ 
        printf("create server_sock failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        return 2;
    }
    //初始化
    struct sockaddr_in service_socket;

    bzero(&service_socket, sizeof(service_socket));
    service_socket.sin_family = AF_INET;
    service_socket.sin_port = htons(atoi(port));
    service_socket.sin_addr.s_addr = htonl(SERVER_IP);
    socklen_t service_socket_len = sizeof(service_socket);
    //绑定端口号和IP地址
    if((bind(server_sock, (struct sockaddr*)(&service_socket), service_socket_len)) < 0){
        printf("bind failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        close(server_sock);
        return 3;
    }
    //设置为监听状态
    if((listen(server_sock, WAIT_QUEUE)) < 0){
        printf("listen failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        close(server_sock);
        return 4;
    }
    return server_sock;
}

void Processer(int sock, struct sockaddr_in* socket){
    char buf[4096];
    bzero(buf, sizeof(buf));
    //等待客户端的数据
    int num = recv(sock, buf, sizeof(buf), 0);
    if(num > 0){
        buf[num] = '\0';
        printf("%s",buf);
        bzero(buf,sizeof(buf));
        const char* echo_buf = "<h1>Hello My Girl</h1>";
        //buf中的信息就是HTTP响应报头的格式。
        sprintf(buf, "HTTP/1.0 200 OK\nContent-Length:%lu\n\n%s",strlen(echo_buf),echo_buf);
        //发送数据给客户端
        int num1 = send(sock, buf, strlen(buf), 0);
        if(num1 < 0){
            printf("send failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        }
    }
    close(sock);
}

void* CreateWorker(void *ptr){
    Arg* arg = (Arg*)ptr;
    Processer(arg->fd, &arg->addr);
    free(arg);
    return NULL;
}


int WaitAccept(int sock, struct sockaddr_in* client_socket){
    socklen_t client_socket_len = sizeof(&client_socket);
    //随时等待接入的客户端
    int client_sock = accept(sock, (struct sockaddr*)(client_socket), &client_socket_len);
    if(client_sock < 0){
        printf("accept failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        printf("wait next accept\n");
        sleep(2);
        return -1;
    }
    //获取接入客户端的IP地址和端口号并输出
    /*INET_ADDRSTRLEN 16*/
    char client_ip_buf[INET_ADDRSTRLEN];
    const char* ptr = inet_ntop(AF_INET, &(client_socket->sin_addr), client_ip_buf, sizeof(client_ip_buf));
    if(ptr == NULL){
        printf("inet_ntop failed, errno : %d, error_string: %s\n", errno, strerror(errno));
        return -1;
    }
    return client_sock;
}


// ./service port
int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage: %s port\n", argv[0]);
        return 1;
    }

    int listen_sock = StartUp(argv[1]);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //进入循环等待链接
    while(1){
        struct sockaddr_in client_socket;
        int client_sock = WaitAccept(listen_sock,&client_socket);
        if(client_sock == -1){
            continue;
        }
        pthread_t tid = 0;
        Arg* arg = (Arg*)malloc(sizeof(Arg));
        arg->fd = client_sock;
        arg->addr = client_socket;
        pthread_create(&tid, NULL, CreateWorker, (void*)arg);
        pthread_detach(tid);
    }
    return 0;
}

