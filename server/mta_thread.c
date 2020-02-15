#include "mta_server.h"

// like proc only thread
int init_thread(inst_thread_t * th, threads_var_t * t_data)
{
    pid_t pid_logger = t_data->log_id;
    int fd_server_socket = t_data->srv_sk;

    int status = 0;
    th->tid = pthread_self();
    th->log_id = pid_logger; // process_is == LOGGER_PROC ? proc->pid : logger_id;

    th->fd.server_sock = fd_server_socket;
    printf("fd_server_socket == %d; fd.listen == %d", fd_server_socket, th->fd.server_sock); // debug // check it
    
    th->in_work = true;
    th->n_sockets = 0;
    th->client = NULL;                          // now do not have sockets // FIRST client_list_t init by NULL
    th->socket_set_write = malloc(sizeof(*th->socket_set_write));
    th->socket_set_read = malloc(sizeof(*th->socket_set_read)); // not need for logger

                                                // workers or any others threads for logging
    char lgname[20];
    sprintf(lgname, "/process%d", pid_logger);
    // sleep(1);
    th->fd.logger = mq_open(lgname, O_WRONLY);  // for send message to logger
    // memset(lgname, 0x0, sizeof(lgname));// for what?

    sprintf(lgname, "/exit%d", pid_logger);
    th->fd.cmd = mq_open(lgname, O_RDONLY);     // for receive command

    if (th->fd.logger == -1 || th->fd.cmd == -1) {
        perror("mq_open() failed in thread");
        free(th->socket_set_read);
        free(th->socket_set_write);
        free(th);
        th = NULL;
    } 
    printf("Worker thread(%d): linked log queue\n", pthread_self());

    // th->gen = malloc(sizeof(general_threads_data_t));
    //the same :?: *th->gen = t_data->gen; // ? // as i know we does not sould malloc if we assignment address already contented the data
    th->gen = &t_data->gen;                     // general data mutex cond and the one queue for all thread
    // now we not malloc mem => we can't call free(th->gen)

    return status;
}


void free_thread(inst_thread_t * th){
    if (th != NULL) {
        printf("free_thread (%d)\n", th->tid);
        free(th->socket_set_read);
        free(th->socket_set_write);
        // free(th->gen);                       // we not alloc mem for it see in main()
        free(th);
        th = NULL;
    } 
}

void run_thread_worker(void * t_data){
    threads_var_t * data = (threads_var_t *) t_data;
    inst_thread_t * th = malloc(sizeof(*th));
    init_thread(th, data);                     // data->srv_sk, data->log_id // like in proc

    work_cicle(th);

    free_thread(th);        
}

void run_thread_controller(void * t_data){
    threads_var_t * data = (threads_var_t *) t_data;
    inst_thread_t * th;
    init_thread(th, data);                     // data->srv_sk, data->log_id

    thread_analitic_cicle(th);                  // can be only one thread
    
    free_thread(th);        
}

#define NUM_DEFAULT_FD          2 // how much i have :: cmd, logger (fd not in set, it use another proc), server socket

/*
1 - если все потоки будут по мьютексу, использовать accept() то при более 2 потоках будет высокая загрузка (говорят 50% но нажо проверить)
2 - если будет один поток делать accept(), то нужна безопасная не блокирующая очередь для получения работы
*/

void thread_analitic_cicle(inst_thread_t * th)
{
    th->n_sockets = NUM_DEFAULT_FD;

    FD_ZERO(th->socket_set_read);  // do we should free last value?
    FD_ZERO(th->socket_set_write);
    FD_SET(th->fd.server_sock, th->socket_set_read);   // the same server socket for all 
    FD_SET(th->fd.cmd, th->socket_set_read);           // the same socket for all threads and proc
        
    char logbuf[BUFSIZE]; // we will recieve already in client struct
    bool must_be_signal_to_thread = false;
    bool is_new_client = false;
    struct sockaddr client_addr;                        /* для адреса клиента */
    socklen_t addrlen = sizeof(client_addr);            /* размер структуры с адресом */
    int cl_socket;
    
    struct timeval timeout;
    timeout.tv_sec = TIME_FOR_WAITING_ACCEPT_SEC;          
    timeout.tv_usec = TIME_FOR_WAITING_ACCEPT_NSEC;        
    
    while(th->in_work){


        int flag_sk = select(FD_SETSIZE, th->socket_set_read, th->socket_set_write, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            push_error(ERROR_SELECT);
        } else if ((flag_sk == 0) && (!FD_ISSET(th->fd.server_sock, th->socket_set_read)) && (!FD_ISSET(th->fd.cmd, th->socket_set_read)) ){ // wait more then time
            // if server ready 
            // but socket just one client socket not ready (after passed time) 
            // then ask user - "is need close server or try again?"
            
            // before code below we must be sure that nothing was disconnected, or if so - must delet connect without crash server
                sprintf(logbuf, " time out for controller thread \n");
                log_queue(th->fd.logger, logbuf);
            
        }else{ 
            /*IS is right way use accept only one time in cicle? or we must try find new connection while(accept != 0) ? */ 
            if (FD_ISSET(th->fd.server_sock, th->socket_set_read)){
                // 1 - was accept?
                // accept return the unical socket num => we can not worring about other threads
                // because we will have onl one in this thread
                if ((cl_socket = accept(th->fd.server_sock, (struct sockaddr *) &client_addr, &addrlen)) < 0) {
                    push_error(ERROR_ACCEPT);
                }
                else
                {
                    is_new_client = true;
                }
            }


            // todo "cmd" control for exit and gracefull_exit
            /* May be make cancel any threads by it thread? is it right way? */

        }

        pthread_mutex_lock(th->gen->mutex_queue);

        if (is_new_client){
            add_client_to_queue(th->gen->sock_q, cl_socket, client_addr);
            is_new_client = false;
        }

        if (th->gen->sock_q->next != NULL){
            must_be_signal_to_thread = true;
        }

        if (must_be_signal_to_thread){
            pthread_cond_signal(th->gen->is_work);
        }

        pthread_mutex_unlock(th->gen->mutex_queue);


        // we must have control for signal, because 
        // when queue is empty => threads will be in ....... coming to pthread_cond_wait()
        // in this (the same) time :: if we send signal => it be losted!! ... and any thread calls by pthread_cond_wait() - would do not have an effect (after it)
        // => we must check if queue is not empty send signal again
        // 

    }

}

void work_cicle(inst_thread_t * th)
{
    // init data thread
    struct timeval timeout;
    timeout.tv_sec = TIME_FOR_WAITING_MESSAGE_SEC;          // x sec +
    timeout.tv_usec = TIME_FOR_WAITING_MESSAGE_NSEC;        // 800000;  // + 0.8 sec
    th->n_sockets = 1; // NUM_DEFAULT_FD;                          // ? // add 2 already
    char logbuf[BUFSIZE];                                   // we will recieve already in client struct
    
    struct sockaddr client_addr;                            /* для адреса клиента */
    // socklen_t addrlen = sizeof(client_addr);          /* размер структуры с адресом */ // need only for accept
    int cl_socket = 0;


    int flag_send = 0;
    int flag_recv = 0;
    ssize_t len_recieved = 0; // now "cl_i->busy_len_in_buf"
    
    while (th->in_work) {

        pthread_mutex_lock(th->gen->mutex_queue);

        /*Осторожно! 
        если очередь не пуста, но мы уже имеем работу, мы можем взять себе еще клиентов. 
        Но!! нет уверености, что клиенты будут поровну распределены, 
        особенно обидно, если какой-то поток будет спать, а тут будет крутится работа.
        Все зависит от планировщика системы и, в часности, его работы с мьютексами 
        и еще работы с мьютексами в pthread_cond_wait().

        Однако. Мне кажется работать с мьютексами правильнее, чем ловить пустые accept(), 
        после успешного select() (на одном и томже серверном сокете).
        хотя если мы работаем с процесами (fork()), то наверное это будет лучшим решением.
        */

        if ((th->gen->sock_q->next != NULL) || (th->n_sockets == 0)){
            /*if we can get new client *//*or*//*if we don't have work*/
            while(th->gen->sock_q->next == NULL)
                pthread_cond_wait(th->gen->is_work, th->gen->mutex_queue);            // block any work thread while we do't have more client

            th_queue_t * new_client = pop_client_from_queue(th->client);
            if (new_client->next == NULL){                      // WAS returned right vallue
                client_addr = new_client->cl_addr;
                cl_socket = new_client->fd_cl;
                free(new_client);                               // check it!
            } else
                push_error(ERROR_QUEUE_POP);
        }
        // else if (th->gen->sock_q->next == NULL && th->n_sockets != 0) 
        //      we already have work => do it;

        pthread_mutex_unlock(th->gen->mutex_queue);


        FD_ZERO(th->socket_set_read);                      // do we should free last value?
        FD_ZERO(th->socket_set_write);
        FD_SET(th->fd.cmd, th->socket_set_read);           // the same socket for all threads and proc
        
        // every new cicle we will clear all sets, because it is more easy then update sets, 
        // if was deleted we just clear by state - fd, close fd, free all data fd
        for (client_list_t * i = th->client; i != NULL; /*i = i->next*/){

            if (i->cur_state != CLIENT_STATE_CLOSED){
                /*C code notebene: as i know when i assign from pointer 
                    (any_type tmp = *(any_type *) p) the all struct data (from '*p') will be memcopy to new variable (to 'tmp') every time,
                    , it is not good. // when i use (*)p = (*)other => i copy only address for ppointer
                */
                if (i->is_writing) FD_SET(i->fd, th->socket_set_write); 
                else FD_SET(i->fd, th->socket_set_read);
                i = i->next;
            } 
            else {
                if (i->next != NULL) {                      // if not last client::
                    i = i->next;
                    close_client_by_state(i->last);         // after it, "i" will be content last (prev == (i-1)) value, 
                } else                                      
                    close_client_by_state(i);               // here i == NULL (was deleted last client) 
            }                                               
        }
        
        int flag_sk = select(FD_SETSIZE, &th->socket_set_read, th->socket_set_write, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            push_error(ERROR_SELECT);
        } else if ((flag_sk == 0) && (!FD_ISSET(th->fd.cmd, th->socket_set_read)) ){ // wait more then time
            
            // before code below we must be sure that nothing was disconnected, 
            // or if so - must delet connect without crash server!

            sprintf(logbuf, "Time over for thread %d, continue work with %d nums of client.\n", (pthread_self()), th->n_sockets);
            log_queue(th->fd.logger, logbuf);
            
        }else{ 
            /* Below part code work with clients sockets. We will try use the same cl->buffer for receive and (after prepare) for send*/
                // if server ready and have client not in set  - we need to add it to 
            if ((cl_socket != 0) && !FD_ISSET(cl_socket, th->socket_set_read)){
                // 2 - was new socket
                if (fcntl(cl_socket, F_SETFL, fcntl(cl_socket, F_GETFL, 0) | O_NONBLOCK) < 0) 
                    push_error(ERROR_FCNTL_FLAG);
                // sockets[n_socket] = client_sk;
                init_new_client(th->client, cl_socket, client_addr); // check it (client is pointer already like in arg init() )
                th->n_sockets += 1;

                sprintf(logbuf, "new client_sk accepted %s\n", client_addr.sa_data);
                log_queue(th->fd.logger, logbuf);

                // i suppose that we can send first message already here! like greeting
                th->client->cur_state = CLIENT_STATE_START; // must be sended greeting
                // below delet::
/*
                sprintf(th->client->buf, REPLY_HELLO); // hello from our server (socket)
                if (send(th->client->fd, th->client->buf, strlen(th->client->buf), 0) < 0) { // check it!
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
*/

                cl_socket = 0;
            }
            
            for(client_list_t * cl_i = th->client; cl_i != NULL; cl_i = cl_i->next){ // i == 0 - server socket
                /* Read message to clientt buf */
                if (FD_ISSET(cl_i->fd, th->socket_set_read)){
                    
                    cl_i->busy_len_in_buf = 0; /* every new transfer buffer must be clean // check it ! may be memset?*/
                    len_recieved = recv(cl_i->fd, cl_i->buf, sizeof(cl_i->buf), flag_recv); 
                    // len_recieved = recv(cl_i->fd, (cl_i->buf+cl_i->busy_len_in_buf), (sizeof(cl_i->buf) - cl_i->busy_len_in_buf), flag_recv);
                    if (len_recieved < 0) {
                        perror("recv");
                        if (errno == EWOULDBLOCK){
                            // NEED TRY AGAIN - WAIT NEXT session
                        } else {
                            cl_i->cur_state = CLIENT_STATE_CLOSED;      // just error
                        }
                        continue;
                    } else if (len_recieved == 0) {
                        cl_i->cur_state = CLIENT_STATE_CLOSED;
                        // printf("Will disconnect with zero message (empty).\n");
                        // disconnect(sockets, &socket_set, i);
                        continue;
                    }else{

                        /* if we receive anything -> work more as can */

                        cl_i->busy_len_in_buf += len_recieved;

                        flags_parser_t flag_parser = parse_message_client(cl_i);
                        while ( (errno != EWOULDBLOCK) ) {
                            if (flag_parser == ANSWER_READY_to_SEND)
                                break;

                            len_recieved = recv(cl_i->fd, (cl_i->buf+cl_i->busy_len_in_buf), (sizeof(cl_i->buf) - cl_i->busy_len_in_buf), flag_recv);
                            // cl_i->busy_len_in_buf = recv(cl_i->fd, cl_i->buf, sizeof(cl_i->buf), flag_recv);
                            if (len_recieved <= 0) {
                                if (errno != EWOULDBLOCK){
                                    cl_i->cur_state = CLIENT_STATE_CLOSED;
                                }
                                break;                                  // we must save not whoe data from client
                            }

                            cl_i->busy_len_in_buf += len_recieved;
                            flag_parser = parse_message_client(cl_i);
                        }
                        // .. cl_i->is_writing = true; // must be inside parser check it




                        sprintf(logbuf, "message receive form socket %s:\n", cl_i->addr, cl_i->buf);
                        log_queue(th->fd.logger, logbuf);
                    }
                }
                /* do main work:: send ok, save message in file or send error to client socket */
                if (FD_ISSET(cl_i->fd, th->socket_set_write)){
                        
                        if(cl_i->cur_state == CLIENT_STATE_START){
                            /* just send OK message after accept() */
                            strcpy(cl_i->buf, REPLY_START); /*be sure, strcpy() added in the end '/0' simbol 1*/
                            if ( send(cl_i->fd, cl_i->buf, strlen(cl_i->buf), flag_send) < 0){
                                if (errno == EWOULDBLOCK)
                                    continue;                                       /*will be try do it again*/
                                else
                                    cl_i->cur_state = CLIENT_STATE_CLOSED;
                            }
                            cl_i->cur_state = CLIENT_STATE_INITIALIZED;
                            cl_i->is_writing = false;                               // THEN will be read from fd
                        } else {
                            /* main part */
                            if ( send(cl_i->fd, cl_i->buf, strlen(cl_i->buf), flag_send) < 0){
                                if (errno == EWOULDBLOCK)
                                    continue;                                       /*will be try do it again*/
                                else
                                    cl_i->cur_state = CLIENT_STATE_CLOSED;
                            } 
                            cl_i->cur_state = CLIENT_STATE_WHATS_NEWS;
                            cl_i->is_writing = false;
                        }

                        // delet below !


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
            } // select (flag from select)
    } // while

    // free in main yeh?

}
