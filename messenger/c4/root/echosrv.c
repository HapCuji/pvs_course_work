/* Простейший TCP-сервер, отправляющий все полученные данные обратно клиенту */
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

// #include <stdin.h>
#include <time.h>
#include <fcntl.h>
#include <sys/time.h>

#include <ctype.h> // isdigit

#define BUFSIZE         512
#define MAX_SOCKETS     10 // FD_SETSIZE

#define TIME_FOR_WAITING_MESSAGE_SEC    20         
#define TIME_FOR_WAITING_MESSAGE_NSEC   800000  // add it to top value

void data_init(void);
void disconnect(int *socket_list, fd_set * socket_set, int deleted_i); //, int * n_socket)
int sockets[MAX_SOCKETS];
unsigned int n_socket;

struct client_info {
    /* data */
    char *name;
};

struct client_info clients[MAX_SOCKETS];


void data_init(void){
    n_socket = 0;
}

int main(int argc, char **argv) {
    struct sockaddr_in addr;    /* для своего адреса */
    struct sockaddr_in sender;  /* для адреса клиента */
    socklen_t addrlen;          /* размер структуры с адресом */
    int server_sk;                     /* файловый дескриптор сокета */
    int client_sk;                 /* сокет клиента */
    char temp_buf[BUFSIZE];

    if (argc != 2) {
        printf("Usage: echosrv <port>\nEx.:   echosrv 1996\n");
        exit(0);
    }

    /* создаём TCP-сокет */
    if ((server_sk = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* указываем адрес и порт */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    int i = 0;
    int port = 0;
    for (i=0; i < strlen(argv[1]); i++)
        port = ((int)argv[1][i] - (int)'0') + port*10;
    printf("port %d\n", port);
    if (port < 0 || port > 0xFFFF){
        perror("port");
        exit(1);
    }
    addr.sin_port = htons(port);

    /* связываем сокет с адресом и портом */
    if (bind(server_sk, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    
    fd_set socket_set, ready_set;
    int len; // for receive
    char buf[BUFSIZE];
    // int client_sk, sk;
    int flag_sk = 0;
    int flag_recv = 0;
    // errno - use it when check EWOULDBLOCK after recv or any more
    // time set wait to time nothing
    struct timeval timeout;
    timeout.tv_sec = 10;       // 100 sec +
    timeout.tv_usec = 800000;  // + 0.8 sec


    /* accept блокируется, пока не появится входящее соединение */
    // addrlen = sizeof(sender);
    // if ((client_sk = accept(server_sk, (struct sockaddr *) &sender, &addrlen)) < 0) {
    //     perror("accept");
    //     exit(1);
    // }

    FD_ZERO(&socket_set);
    // fcntl(server_sk, F_SETFL, fcntl(server_sk, F_GETFL, 0) | O_NONBLOCK);
    FD_SET(server_sk, &socket_set);
    sockets[n_socket] = server_sk; // zero == 0
    n_socket += 1;

    /* переводим сокет в состояние ожидания соединений  */
    if (listen(server_sk, SOMAXCONN) < 0) {
        perror("listen");
        exit(1);
    };
    printf("Echo server listening --- press Ctrl-C to stop\n");
    
    while (1) {
        
        ready_set = socket_set;

        timeout.tv_sec = TIME_FOR_WAITING_MESSAGE_SEC;          // 10 sec +
        timeout.tv_usec = TIME_FOR_WAITING_MESSAGE_NSEC;        // 800000;  // + 0.8 sec

        flag_sk = select(FD_SETSIZE, &ready_set, NULL, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            perror("select error");
            exit(1);
        }else if ((flag_sk == 0) && (!FD_ISSET(server_sk, &ready_set))){ // wait more then time
            // before code below we must be sure that nothing was disconnected, or if so - must delet connect without crash server
                printf("continue wait for new connection? press ':q' or \"enter\" to again use it.\nnow n = %d ? >", n_socket);
                fgets(buf, BUFSIZE, stdin);
                if (strcmp(buf, ":q\n") == 0)
                    break;
            
        }else{ 
            if (FD_ISSET(server_sk, &ready_set)){
                printf("new ready server inside select\n");
                // was accept?
                addrlen = sizeof(sender);
                if ((client_sk = accept(server_sk, (struct sockaddr *) &sender, &addrlen)) < 0) {
                    perror("accept");
                    exit(1);
                }

                if (!FD_ISSET(client_sk, &socket_set)){
                    // was new socket
                    fcntl(client_sk, F_SETFL, fcntl(client_sk, F_GETFL, 0) | O_NONBLOCK);
                    sockets[n_socket] = client_sk;
                    FD_SET(client_sk, &socket_set);
                    n_socket += 1;
                    printf("new client_sk accepted\n");

                    sprintf(buf, "Write about you - use '/me: ' + any string.\nSend - use '/send: <user_number>: <message>'.\nwhat here - use '/?'.\n");
                    if (send(sockets[n_socket-1], buf, strlen(buf), 0) < 0) {
                        perror("Was disconnected without normal work start");
                        disconnect(sockets, &socket_set, n_socket-1);
                        continue;
                    }
                }
                // else continue below
                printf("end ready server\n");
            }
            if (n_socket > 1) {
                for(i = 1; i < n_socket; i++){ // i == 0 - server socket
                    if (FD_ISSET(sockets[i], &ready_set)){
                        len = recv(sockets[i], buf, BUFSIZE, flag_recv);
                        if (len < 0) {
                            perror("recv");
                            if (errno == EWOULDBLOCK){
                                // was none message from sk
                                continue;
                            } else {
                                // just error
                                break;
                            }
                        } else if (len == 0) {
                            printf("Will disconnect with zero message (empty).\n");
                            disconnect(sockets, &socket_set, i);
                            continue;
                        }else{
                            buf[len] = '\0';
                            printf("message exchange form socket %d:\n<> %s </>\n", i, buf);
                            if (strcmp(buf, "/q\n") == 0){
                                printf("%s #%d want disconnecting\n", clients[i].name, i);
                                disconnect(sockets, &socket_set, i);
                                continue;
                            } else if(strncmp(buf, "/?", 2) == 0){
                                sprintf(buf, "Here is users %d:\n", n_socket - 1);
                                int user_i;
                                for (user_i=1; user_i < n_socket; user_i++){
                                    sprintf(temp_buf, "#%d sock %d - %s\n", user_i, sockets[user_i], clients[user_i].name);
                                    strcat(buf, temp_buf);
                                }
                                
                                if (send(sockets[i], buf, strlen(buf), 0) < 0) {
                                    // here must be disconnect this socket
                                    perror("Was disconnected without normal work");
                                    disconnect(sockets, &socket_set, i);
                                    continue;
                                }
                            }else if(strncmp(buf, "/me:", 4) == 0){
                                free(clients[i].name); // delet old
                                int len_i;
                                for(len_i = 4; len_i < strlen(buf); len_i++){
                                    temp_buf[len_i-4] = buf[len_i];
                                }
                                temp_buf[len_i] = '\0';
                                clients[i].name = malloc(strlen(temp_buf)*sizeof(char));
                                // memcpy(clients[i].name, temp_buf, strlen(temp_buf));
                                strcpy(clients[i].name, temp_buf);

                                sprintf(buf, "Ok\n");
                                if (send(sockets[i], buf, strlen(buf), 0) < 0) {
                                    perror("Was disconnected without normal work");
                                    disconnect(sockets, &socket_set, i);
                                    continue;
                                }
                            }else if(strncmp(buf, "/send:", 6) == 0){
                                int user_num = 0;
                                int len_i, new_len;
                                for (len_i = 6; (len_i < strlen(buf)) && (buf[len_i] !=':'); len_i++){
                                    if (isdigit(buf[len_i]))
                                        user_num = user_num*10 + ((int) buf[len_i] - (int) '0');
                                }
                                if (len_i == strlen(buf)){
                                    printf("error syntaxis\n");
                                    sprintf(buf, "error syntaxis to %d.\n", user_num);
                                    if (send(sockets[i], buf, strlen(buf), 0) < 0) {    // err - source user
                                        perror("Was disconnected without normal work");
                                        disconnect(sockets, &socket_set, user_num);
                                    }
                                    continue;
                                }
                                if (user_num >= n_socket) {
                                    printf("socket num not exist!\n");
                                    sprintf(buf, "error num - %d not exist!\n", user_num);
                                    if (send(sockets[i], buf, strlen(buf), 0) < 0) {    // err - source user
                                        perror("Was disconnected without normal work");
                                        disconnect(sockets, &socket_set, user_num);
                                    }
                                    continue;
                                }
                                // for (new_len = 0; (new_len < BUFSIZE) && (len_i < strlen(buf)); len_i++) temp_buf[new_len++] = buf[len_i];
                                strncpy( temp_buf, (buf+len_i), (strlen(buf) - len_i) );
                                temp_buf[(strlen(buf) - len_i)] = '\0'; // strlen(temp_buf)

                                // sprintf(buf, "%s", temp_buf);
                                if ((user_num < MAX_SOCKETS) && ((strlen(buf) - len_i) > 1 ) ){
                                    if (send(sockets[user_num], temp_buf, strlen(temp_buf), 0) < 0) { // ok - destination user 
                                        perror("Was disconnected without normal work");
                                        disconnect(sockets, &socket_set, user_num);
                                        continue;
                                    }
                                    sprintf(buf, "Mess success to %d.\n", user_num);
                                    if (send(sockets[i], buf, strlen(buf), 0) < 0) {    // ok - source user 
                                        perror("Was disconnected without normal work");
                                        disconnect(sockets, &socket_set, user_num);
                                        continue;
                                    }
                                } 
                                else
                                {
                                    sprintf(buf, "None sended to %d.\n", user_num);
                                    if (send(sockets[i], buf, strlen(buf), 0) < 0) {    // err - source user
                                        perror("Was disconnected without normal work");
                                        disconnect(sockets, &socket_set, user_num);
                                        continue;
                                    }
                                }
                                
                            }else{
                                // here will check for what message and sending
                                
                                if (send(sockets[i], buf, strlen(buf), 0) < 0) {
                                    perror("send");
                                    break;
                                }
                            }
                            
                        }
                        
                    }
                }
            }
        }
        
    }

    /* must be reachable code */
    close(server_sk);
    return 0;
}

// input from what we delet == pointer (set, list) and what delet == count (i)
void disconnect(int *socket_list, fd_set *socket_set, int deleted_i){ //, int * n_socket)

    if (deleted_i >= n_socket){
        printf("socket not exist, error argument!\n");
        perror("close\n");
    } else {
        printf("Remote host has closed the connection\n");
        close(socket_list[deleted_i]);
        FD_CLR(socket_list[deleted_i], socket_set);
        int clr_i;
        free(clients[deleted_i].name);
        for(clr_i = deleted_i; clr_i < n_socket; clr_i++){ // 1client => n==2 => must be n==1 and clear i==1 but i==2 is None!
            sockets[clr_i] = (clr_i == (n_socket - 1))? (int)NULL : sockets[clr_i+1]; // [n_socket] is NONE
            clients[clr_i].name = (clr_i == (n_socket - 1))? (char*)NULL : clients[clr_i+1].name;
        }
        n_socket -= 1;

        printf("Done disconnect %d\n", deleted_i);
    }
}