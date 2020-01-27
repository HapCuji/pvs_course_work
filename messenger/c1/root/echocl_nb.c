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
#include <signal.h> // for kill()

#define true            1
#define false           0

typedef unsigned char bool;

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
    
    // here will created two threads or proccess

    pid_t pid_cur = 0; // it is int type
    bool is_parent = false;
    pid_cur = fork(); // return 0 for new process
    if (pid_cur > 0)
        is_parent = true;
    pid_cur = getpid();

    while (1) {

        if (!is_parent)
        {
            len = recv(sk, buf, BUFSIZE, 0);
            if (len < 0) {
                if(errno != EWOULDBLOCK){
                    perror("recv");
                    exit(1);
                }
            } else if (len == 0) {
                printf("Remote host has closed the connection (empty message).\n");
                exit(1);
            } else if (len > 0){
                buf[len] = '\0';
                printf("recieve from server:\n << %s\n", buf);
            }
            // her must be check that parent process is ending!!!
            // her must be check that parent process is ending!!!
            // her must be check that parent process is ending!!!

        } 
        else 
        {       // only parent process
            printf("(ready to new command)\n");
            fgets(buf, BUFSIZE, stdin);
            if (strlen(buf) <= 1) // only enter \n
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
    }

    sprintf(buf, "/q");
    while (3.14){
        if (send(sk, buf, strlen(buf), 0) < 0) {
            if(errno == EWOULDBLOCK)
                continue;
            perror("send");
            exit(1);
        }
        else
            break;
    }
    close(sk);

    printf("killing proc %d \n", pid_cur);
    kill(pid_cur, SIGTERM);                 // pid_cur == getpid()
    if (is_parent)
        printf("killed ok\n");

    return 0;
}
