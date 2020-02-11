#include "mta_server.h"

#define UPPERCASE_STR_N(str, n) for(int k = 0; k<n; k++) { *(str+k) = toupper(*(str + k)); }
#define UPPERCASE_STR_SK_to_EK(str, sk, ek) for(int k = sk; k<ek; k++) { *(str+k) = toupper(*(str + k)); }

// like proc only thread
int init_thread(struct inst_thread_t * th, int fd_server_socket, pid_t pid_logger)
{
    int status = 0;
    th->tid = gettid();
    th->log_id = pid_logger; // process_is == LOGGER_PROC ? proc->pid : logger_id;

    th->fd.listen = fd_server_socket;
    printf("fd_server_socket == %d; fd.listen == %d", fd_server_socket, th->fd.listen); // debug // check it
    
    th->in_work = true;
    th->n_sockets = 0;
    th->client = NULL;  // not have sockets
    th->socket_set_write = malloc(sizeof(*proc->socket_set_write));
    th->socket_set_read = malloc(sizeof(*proc->socket_set_read)); // not need for logger

    // workers or any others threads for logging
    char lgname[20];
    sprintf(lgname, "/process%d", pid_logger);
    // sleep(1);
    th->fd.logger = mq_open(lgname, O_WRONLY);  // for send message to logger
    // memset(lgname, 0x0, sizeof(lgname));// for what?

    sprintf(lgname, "/exit%d", pid_logger);
    th->fd.cmd = mq_open(lgname, O_RDONLY);   // for receive command

    if (th->fd.logger == -1 || th->fd.cmd == -1) {
        perror("mq_open() failed");
        free(th);
        th = NULL;
    } 
    printf("Worker thread(%d): linked log queue\n", gettid());

    return status;
}


void run_thread(void * t_data){
    general_threads_data_t * data = (general_threads_data_t *) t_data;
    struct inst_thread_t th;
    init_thread(&th, data->srv_sk, data->log_id);
    work_cicle(&th);
}

#define NUM_DEFAULT_FD          2 // how much i have :: cmd, logger (fd not in set, it use another proc), server socket
/*
i will have one sets of sokets (one client's list for all my threads)
- and will have counter what now is called
- every thread will use mutex for changing counter // не не мьютекс здесь не целесообразно, их надо 1000 штук выходит, т.к. создаютсяони одновремменно - лучше просто бууду использовать общюю переданную переменную (вроде бы это закконо)
?? do need - another thread (not here) will be make write to file, if (data is ready) or data is too
*/
void work_cicle(struct inst_thread_t * th)
{
    // init data thread
    struct timeval timeout;
    timeout.tv_sec = TIME_FOR_WAITING_MESSAGE_SEC;          // 10 sec +
    timeout.tv_usec = TIME_FOR_WAITING_MESSAGE_NSEC;        // 800000;  // + 0.8 sec
    th->n_socket = NUM_DEFAULT_FD;   // ? // add 2 already
    char logbuf[BUFSIZE]; // we will recieve already in client struct
    //
    socklen_t addrlen;          /* размер структуры с адресом */
    struct sockaddr client_addr;  /* для адреса клиента */
    int cl_socket;
    
    int flag_recv = 0;
    ssize_t len_recieved = 0;
    // 
    while (th->in_work) {


        FD_ZERO(&th->socket_set_read);  // do we should free last value?
        FD_ZERO(&th->socket_set_write);
        FD_SET(th->fd.server_sock, &th->socket_set_read);   // the same server socket for all 
        FD_SET(th->fd.cmd, &th->socket_set_read);           // the same socket for all threads and proc
        // sockets[th->n_sockets] = server_sk;
        // ready_set = socket_set;
        
        // every new cicle we will clear all sets, because it is more easy then update sets, 
        // if was deleted we just clear by state - fd, close fd, free all data fd
        for (struct client_list_t * i = th->client; i != NULL; i = i->next){
            /*C code notebene: as i know when i assign from pointer (any_type tmp = *(any_type *) p) the all struct data (from '*p') will be memcopy to new variable (to 'tmp') every time, it is not good.// when i use (*)p = (*)other => i copy only address for ppointer*/
            if (i->is_writing) FD_SET(i->fd, &th->socket_set_write); 
            else FD_SET(i->fd, &th->socket_set_read);
        }
        
        int flag_sk = select(FD_SETSIZE, &th->socket_set_read, &th->socket_set_write, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            push_error(ERROR_SELECT);
        } else if ((flag_sk == 0) && (!FD_ISSET(th->fd.server_sock, &th->socket_set_read)) && (!FD_ISSET(th->fd.cmd, &th->socket_set_read)) ){ // wait more then time
            // if server ready 
            // but socket just one client socket not ready (after passed time) 
            // then ask user - "is need close server or try again?"
            
            // before code below we must be sure that nothing was disconnected, or if so - must delet connect without crash server
                printf("continue wait for new connection? press ':q' or \"enter\" to again use it.\nnow n = %d ? >", n_socket);
                fgets(logbuf, BUFSIZE, stdin);              // what behavior of this code ?
                // printf("buffer is - %s\n", logbuf);
                strcat(logbuf, " - (ps user write) \n");
                log_queue(th->fd.logger, logbuf);
                if (strncmp(logbuf, ":q", 2) == 0)
                    break;
            
        }else{ 
                /*IS is right way use accept only one time in cicle? or we must try find new connection while(accept != 0) ? */ 
            if (FD_ISSET(th->fd.server_sock, &th->socket_set_read)){
                // 1 - was accept?
                addrlen = sizeof(client_addr);
                // accept return the unical socket num => we can not worring about other threads
                // because we will have onl one in this thread
                if ((cl_socket = accept(th->fd.server_sock, (struct sockaddr *) &client_addr, &addrlen)) < 0) {
                    push_error(ERROR_ACCEPT);
                }
                    // if server ready and have client not in set  - we need to add it to 
                if (!FD_ISSET(cl_socket, &th->socket_set_read)){
                    // 2 - was new socket
                    if (fcntl(cl_socket, F_SETFL, fcntl(client_sk, F_GETFL, 0) | O_NONBLOCK) < 0) 
                        push_error(ERROR_FCNTL_FLAG);
                    // sockets[n_socket] = client_sk;
                    init_new_client(&th->client, cl_socket, &client_addr); // check it
                    th->n_sockets += 1;

                    sprintf(logbuf, "new client_sk accepted %s\n", client_addr.sa_data);
                    log_queue(th->fd.logger, logbuf);

                    // i suppose that we can send first message already here! like greeting
                    th->client->cur_state = CLIENT_STATE_START; // must be sended greeting
                    // below delet::
                    sprintf(buf, REPLY_HELLO); //hello from our server (socket)
                    if (send(th->client->fd, buf, strlen(buf), 0) < 0) { // check it!
                        if (errno != EWOULDBLOCK) {
                            th->client->cur_state = CLIENT_STATE_CLOSED;
                            // disconnect(sockets, &socket_set, n_socket-1);
                        }// else // below not uncomment!!  we can have state with blocked in any state // not one explain
                            // th->client->cur_state = CLIENT_STATE_BLOCKED;// NEED TRY SEND AGAIN! => mb need send answer not right here? check it
                    } 
                    else // all ok sended next state
                    {
                        th->client->cur_state = CLIENT_STATE_INITIALIZED; // next_state(&th->client->cur_state);
                        th->client->is_writing = false; // next will be recieving next message (helo / ehlo)
                    }
                }
            }
            for(struct client_list_t * cl_i = th->client; cl_i != NULL; cl_i = cl_i->next){ // i == 0 - server socket
                if (FD_ISSET(cl_i->fd, &th->socket_set_read)){
                    len_recieved = recv(cl_i->fd, cl_i->buf, sizeof(cl_i->buf), flag_recv);
                    if (len_recieved < 0) {
                        perror("recv");
                        if (errno == EWOULDBLOCK){
                            // NEED TRY AGAIN - WAIT NEXT session
                        } else {
                            cl_i->cur_state = CLIENT_STATE_CLOSED; // just error
                        }
                        continue;
                    } else if (len_recieved == 0) {
                        cl_i->cur_state = CLIENT_STATE_CLOSED;
                        // printf("Will disconnect with zero message (empty).\n");
                        // disconnect(sockets, &socket_set, i);
                        continue;
                    }else{
                        printf("message exchange form socket %s:\n<> %s </>\n", cl_i->addr, cl->buf);
                        cl_i->is_writing = true;
                    }
                }
                if (FD_ISSET(cl_i->fd, &th->socket_set_write)){
                        // buf[len_recieved] = '\0';
                        // work with message :: in buff
                        if (strncmp(UPPERCASE_STR_N(buf, 2), "/q", 2) == 0){
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
    } // while

    // free in main yeh?

}


int init_new_client(struct client_list_t *th_listp, int client_sock, struct sockaddr * new_addr)
{
    int status = 0;
    struct client_list_t * new_client = (struct client_list_t *) malloc(sizeof(struct client_list_t));

    // init parameter of client
    new_client->addr = new_addr;
    new_client->cur_state = CLIENT_STATE_START;
    new_client->fd = client_sock;
    new_client->is_writing = true; // we recieve it and must send answer (but not sure, it if we can't send already)
    // init data of message
    new_client->data = (struct client_msg_t *) malloc(1*sizeof(struct client_msg_t));
    // new_client->data->file_to_save = malloc(MAX_FILE_SIZE*sizeof(char)); // i thinck it not here
    new_client->data->was_start_writing = false; // true only for long message

    // ADD item to begining of list threads socket (clients)
    new_client->next = th_listp;
    th_listp = new_client;
    // th_listp->next = not changing == last value

    return status;
}

client_state next_state(client_state cur_state)
{
    return (cur_state == CLIENT_STATE_DONE)? CLIENT_STATE_DONE : cur_state + 1;
}
