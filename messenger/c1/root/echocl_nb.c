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

#include <pthread.h>
// pthread_create() - clreate child thread
// pthread_exit() - close current thread
// pthread_cancel() - close choosed thread :: but it can work not so, as you want
// exit() or return; in _main_ thread => will close all children's threads


#define true            1
#define false           0

typedef unsigned char bool;

typedef struct {
    bool server_failed;
    bool user_exit;
    int sk;
} general_threads_data_t;

void * user_controller(void* thread_data);

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
    #ifdef __USE_PROCESS__ // see last code part in prevision commit
    pid_t pid_cur = 0; // it is int type
    bool is_parent = false;
    pid_cur = fork(); // return 0 for new process
    if (pid_cur > 0)
        is_parent = true;
    pid_cur = getpid();
    #else
    // pthread_t thread_controller;
    pthread_t another_thread;
    // void * general_threads_data = NULL;
    general_threads_data_t general_threads_data;
    general_threads_data.server_failed = false;
    general_threads_data.user_exit = false;
    general_threads_data.sk = sk;
    // pthread_create(&another_thread, NULL, message_reciever, &general_threads_data);
    pthread_create(&another_thread, NULL, user_controller, &general_threads_data);

    #endif

    while(true)
    {
        #ifdef __PARENT_IS_USER_CONTROLLER__
               // only parent process                                    
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
        #else // __PARENT_IS_MESSAGE_RECIEVER__
            len = recv(sk, buf, BUFSIZE, 0);
            if (len < 0) {
                if(errno != EWOULDBLOCK){
                    printf("not EWOULDBLOCK len was < 0");
                    perror("recv");
                    exit(1);
                }
            } else if (len == 0) {
                printf("Remote host has closed the connection (empty message).\n");
                general_threads_data.server_failed = true;
                pthread_cancel(another_thread);
                pthread_join(another_thread, NULL);       // check it! // we must close user input!!
                break;
                // exit(1);
            } else if (len > 0){
                buf[len] = '\0';
                printf("(recieve from server):\n << %s\n", buf);
            }

            if (general_threads_data.user_exit == true)
            {
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
                pthread_join(another_thread, NULL); // like waitpid()
                break;
            }
        #endif
        
    }
    printf("client_socket will be closed\n");
    close(sk);

    #ifdef __USE_PROCESS__
    printf("killing proc %d \n", pid_cur);
    kill(pid_cur, SIGTERM);                 // pid_cur == getpid()
    if (is_parent)
        printf("killed ok\n");
    #else
    #endif

    // free(general_threads_data);
    // free(another_thread);


    return 0;
}


// ------------------
// below only threads func
// -----------------------
void * user_controller(void* thread_data)
{
    general_threads_data_t * data = (general_threads_data_t *) thread_data; // can just "= thread_data;"
    char buf[BUFSIZE];
    while(true)
    {
        if (data->server_failed == true)
            break;

        printf("($)\n");
        fgets(buf, BUFSIZE, stdin);
        if (strlen(buf) <= 1) // only enter \n
            continue;
        if (strncmp(buf, "/q", 2) == 0){
            printf("exit\n");
            data->user_exit = true;
            break;
        }

        if (send(data->sk, buf, strlen(buf), 0) < 0) {
            if(errno != EWOULDBLOCK){
                perror("send");
                exit(1);
            }
        }
    }
    printf("user controller is closed\n");

    return NULL;
}

// void message_reciever(void* thread_data);
// void message_reciever(void* thread_data)
// {
//     general_threads_data_t * data = (general_threads_data_t *) thread_data; // can just "= thread_data;"
//     char buf[BUFSIZE];
//     ssize_t len = 0;
//     while (1) {

//             len = recv(data->sk, buf, BUFSIZE, 0);
//             if (len < 0) {
//                 if(errno != EWOULDBLOCK){
//                     perror("recv");
//                     exit(1);
//                 }
//             } else if (len == 0) {
//                 printf("Remote host has closed the connection (empty message).\n");
//                 exit(1);
//             } else if (len > 0){
//                 buf[len] = '\0';
//                 printf("(recieve from server):\n << %s\n", buf);
//             }
//             // here must be check that parent process is ending!!!

//     } 
// }

// -------------------------