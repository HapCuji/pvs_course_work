#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#define BUFSIZE 512

#include <sys/time.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    struct sockaddr_in addr;    /* для адреса сервера */
    socklen_t addrlen;          /* размер структуры с адресом */
    int sk;                     /* файловый дескриптор сокета */
    char buf[BUFSIZE];          /* буфер для сообщений */
    int len;

    if (argc != 3) {
        printf("Usage: echocl <ip>\nEx.:   echocl 10.30.0.2 1996\n");
        exit(0);
    }

    /* создаём TCP-сокет */
    if ((sk = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    int i = 0;
    int port = 0;
    for (i=0; i < strlen(argv[2]); i++)
        port = ((int)argv[2][i] - (int)'0') + port*10;
    printf("port %d\n", port);
    if (port < 0 || port > 0xFFFF){
        perror("port");
        exit(1);
    }
    addr.sin_port = htons(port);


    /* соединяемся с сервером */
    if (connect(sk, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    fcntl(sk, F_SETFL, fcntl(sk, F_GETFL, 0) | O_NONBLOCK);

    printf("Connected to Echo server. Type /q to quit.\n");
    
    while (1) {
        len = recv(sk, buf, BUFSIZE, 0);
        if (len < 0) {
            if(errno != EWOULDBLOCK){
                perror("recv");
                exit(1);
            }
        } else if (len == 0) {
            printf("Remote host has closed the connection (empty message).\n");
            exit(1);
        }

        buf[len] = '\0';
        printf("<< %s\n", buf);

        printf("> ");
        fgets(buf, BUFSIZE, stdin);
        if (strlen(buf) == 0)
            continue;
        if (strcmp(buf, "/q\n") == 0)
            break;

        if (send(sk, buf, strlen(buf), 0) < 0) {
            if(errno != EWOULDBLOCK){
                perror("send");
                exit(1);
            }
        }
        
    }

    sprintf(buf, "/q");
    while (3.14){
        if (send(sk, buf, strlen(buf), 0) < 0) {
            if(errno == EWOULDBLOCK)
                continue;
            perror("send");
            exit(1);
        }
    }
    close(sk);
    return 0;
}
