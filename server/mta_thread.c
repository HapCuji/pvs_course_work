#include "mta_server.h"

// like proc only thread
int init_thread(inst_thread_t * th, threads_var_t * t_data)
{
    pid_t pid_logger = t_data->log_id;
    int fd_server_socket = t_data->srv_sk;

    int status = 0;
    th->tid = pthread_self();
    th->log_id = pid_logger;                                        // process_is == LOGGER_PROC ? proc->pid : logger_id;

    th->fd.server_sock = fd_server_socket;
    
    char logbuf[300];
    sprintf(logbuf, "fd_server_socket == %d; fd.listen == %d", fd_server_socket, th->fd.server_sock); // debug // check it
    
    th->in_work = true;
    th->n_sockets = 0;
    th->client = NULL;                                              // now do not have sockets // FIRST client_list_t init by NULL
    th->socket_set_write = malloc(sizeof(*th->socket_set_write));   // not need for logger
    th->socket_set_read = malloc(sizeof(*th->socket_set_read));     

                                                                    // workers or any others threads for logging
    char fname_cmd[20];
    /* Here logger must be already runing */
    sprintf(fname_cmd, "/process%d", NUM_LOGGER_NAME);
    th->fd.logger = mq_open(fname_cmd, O_WRONLY);                      // for send message to logger

    struct mq_attr attr;
        attr.mq_flags = attr.mq_curmsgs = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = BUFSIZE;
    sprintf(fname_cmd, "/exit%d", NUM_CMD_THREAD);                      //  .. , th->log_id);
    th->fd.cmd = mq_open(fname_cmd, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
        if (th->fd.cmd == -1){
            // mq_close(th->fd.cmd);                                    // how close -1 ?
            mq_unlink(fname_cmd);
            th->fd.cmd = mq_open(fname_cmd, O_EXCL | O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
        }   
    // sprintf(fname_cmd, "/exit%d", NUM_CMD_NAME);
    // th->fd.cmd = mq_open(fname_cmd, O_RDONLY);                         // for receive command

    if (th->fd.logger == -1 || th->fd.cmd == -1) {
        perror("mq_open() failed in thread");
        th->in_work = false;
        t_data->gen.cancel_work = true;
        return ERROR_LINKED_CMD_LOG;                                // can be push_error()
    } else
        printf("Worker thread(%lu): linked log queue\n", pthread_self());


    pthread_mutex_lock(t_data->gen.mutex_queue);                    // check it !? must i for all threads copy mutex data, but with it mutex they can get the really general data?
    // th->gen = malloc(sizeof(general_threads_data_t));
    //the same :?: *th->gen = t_data->gen; // ? // as i know we does not sould malloc if we assignment address already contented the data
    th->gen = &(t_data->gen);                     // general data mutex cond and the one queue for all thread
    // now we not malloc mem => we can't call free(th->gen)
    pthread_mutex_unlock(t_data->gen.mutex_queue);

    return status;
}


void free_thread(inst_thread_t * th){
    if (th != NULL) {
        
        printf("free_thread (%d)\n", (int)th->tid);
        free(th->socket_set_read);
        free(th->socket_set_write);
        // free(th->gen);                                   // we not alloc mem for it see in main()
        while(th->client != NULL)
            free_one_client_in_list(&(th->client));           // already is pointer on pointer for list
        free(th);
        th = NULL;
    } 
}

void * run_thread_worker(void * t_data){
    threads_var_t * data = (threads_var_t *) t_data;
    inst_thread_t * th = malloc(sizeof(*th));
    init_thread(th, data);                     // data->srv_sk, data->log_id // like in proc

    work_cicle(th);

    free_thread(th);   

    return NULL;     
}

void * run_thread_controller(void * t_data){
    threads_var_t * data = (threads_var_t *) t_data;
    inst_thread_t * th = malloc(sizeof(*th));
    init_thread(th, data);                     // data->srv_sk, data->log_id

    thread_analitic_cicle(th);                  // can be only one thread
    
    /*only in controller*/
    if (th->fd.cmd != -1)
        mq_close(th->fd.cmd);
    char fname_log_cmd[20];
    sprintf(fname_log_cmd, "/exit%d", NUM_CMD_THREAD);
    mq_unlink(fname_log_cmd);
  

    free_thread(th); 

    return NULL;     
}

#define NUM_DEFAULT_FD_WORKER           0 // 1 // how much i have :: cmd
#define NUM_DEFAULT_FD_CONTROLLER       2 // how much i have :: cmd, logger (fd not in set, it use another proc) /(OR) server socket

/*
1 - если все потоки будут по мьютексу, использовать accept() то при более 2 потоках будет высокая загрузка (говорят 50% но нажо проверить)
2 - если будет один поток делать accept(), то нужна безопасная не блокирующая очередь для получения работы
*/

void thread_analitic_cicle(inst_thread_t * th)
{
    th->n_sockets = NUM_DEFAULT_FD_CONTROLLER;

        
    char logbuf[BUFSIZE]; // we will recieve already in client struct
    bool must_be_signal_to_thread = false;
    bool is_new_client = false;
    struct sockaddr client_addr;                        /* для адреса клиента */
    socklen_t addrlen = sizeof(client_addr);            /* размер структуры с адресом */
    int cl_socket;
    
    struct timeval timeout;
    int cnt_closed_cmd = 0;
    bool close_other_threads = false;

    while(th->in_work){

        FD_ZERO(th->socket_set_read);  // do we should free last value?
        FD_ZERO(th->socket_set_write);
        FD_SET(th->fd.server_sock, th->socket_set_read);   // the same server socket for all 
        FD_SET(th->fd.cmd, th->socket_set_read);           // the same socket for all threads and proc

        timeout.tv_sec  = close_other_threads? 0 : TIME_FOR_WAITING_ACCEPT_SEC;          
        timeout.tv_usec = close_other_threads? 1 : TIME_FOR_WAITING_ACCEPT_NSEC;        
        
        int flag_sk = select(FD_SETSIZE, th->socket_set_read, th->socket_set_write, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            push_error(ERROR_SELECT);
        } else if ((flag_sk == 0) && (!FD_ISSET(th->fd.server_sock, th->socket_set_read)) && (!FD_ISSET(th->fd.cmd, th->socket_set_read)) ){ // wait more then time
            // if server ready 
            // but socket just one client socket not ready (after passed time) 
            // then ask user - "is need close server or try again?"
            
            // before code below we must be sure that nothing was disconnected, or if so - must delet connect without crash server
                if (!close_other_threads){
                    sprintf(logbuf, " Time out for controller thread.");
                    log_queue(th->fd.logger, logbuf);
                    gracefull_exit(NUM_CMD_THREAD);                 // threads is first cancel
                }
            
        }else{ 
            /*IS is right way use accept only one time in cicle? or we must try find new connection while(accept != 0) ? */ 
            if (FD_ISSET(th->fd.server_sock, th->socket_set_read)){
                // 1 - was accept?
                // accept return the unical socket num => we can not worring about other threads
                // because we will have onl one in this thread
                if ((cl_socket = accept(th->fd.server_sock, (struct sockaddr *) &client_addr, &addrlen)) < 0) {
                    push_error(ERROR_ACCEPT);
                } else {
                    is_new_client = true;
                }
                sprintf(logbuf, "Controller thread new client? ");
                log_queue(th->fd.logger, logbuf);
            }

            // todo "cmd" control for exit and gracefull_exit
            /* May be make cancel any threads by it thread? is it right way? */

            if (FD_ISSET(th->fd.cmd, th->socket_set_read)) {
                unsigned int priority = THREAD_PRIORITY;
                if (mq_receive(th->fd.cmd, logbuf, BUFSIZE, &priority) >= 0) {
                    if (strcmp(logbuf, EXIT_MSG_CMD_THREAD) == 0) {
                        sprintf(logbuf, "Controller(%lu): accept command on close.", th->tid);
                        log_queue(th->fd.logger, logbuf);
                        close_other_threads = true;
                        cnt_closed_cmd++;
                    } // else 
                        // printf("Controller(%d): received message <%s>\n", getpid(), msg);
                } else {
                    sprintf(logbuf, "Controller thread was new cmd?");
                    log_queue(th->fd.logger, logbuf);
                }
            }

        }

        pthread_mutex_lock(th->gen->mutex_queue);

        if (is_new_client){
            add_client_to_queue(&(th->gen->sock_q), cl_socket, client_addr);
            is_new_client = false;
        }

        if (th->gen->sock_q != NULL){
            must_be_signal_to_thread = true;
        } else {
            must_be_signal_to_thread = false;
        }

        if (close_other_threads){
            must_be_signal_to_thread = true;
            th->gen->cancel_work = true;
            
            if (cnt_closed_cmd >= NUM_OF_THREAD)
                th->in_work = false;
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

void set_all_client_on_close(inst_thread_t * th)            // check it
{
    for (client_list_t * cl_i = th->client; cl_i != NULL; cl_i =  cl_i->next){
        cl_i->cur_state = CLIENT_STATE_CLOSED;    // CLIENT_STATE_SENDING_ERROR;       //  
        sprintf(cl_i->buf, REPLY_TERMINATE);
        cl_i->busy_len_in_buf = sizeof(REPLY_TERMINATE);    // can not be
        cl_i->is_writing = true;

        send(cl_i->fd, cl_i->buf, strlen(cl_i->buf), 0);    // just without success hz // check it
    }
}

void work_cicle(inst_thread_t * th)
{
    // init data thread
    struct timeval timeout;
    struct timespec timecond;
    th->n_sockets = NUM_DEFAULT_FD_WORKER; // NUM_DEFAULT_FD;                          // only cmd without server_socket
    char logbuf[BUFSIZE];                                   // we will recieve already in client struct
    
    struct sockaddr client_addr;                            /* для адреса клиента */
    // socklen_t addrlen = sizeof(client_addr);          /* размер структуры с адресом */ // need only for accept
    int cl_socket = 0;


    int flag_send = 0;
    int flag_recv = 0;
    ssize_t len_recieved = 0; // now "cl_i->busy_len_in_buf"
    
    while (th->in_work) {
        timeout.tv_sec = TIME_FOR_WAITING_MESSAGE_SEC;          // x sec +
        timeout.tv_usec = TIME_FOR_WAITING_MESSAGE_NSEC;        // 800000;  // + 0.8 sec
        
        pthread_mutex_lock(th->gen->mutex_queue);

        /*Осторожно! 
        если очередь не пуста, но мы уже имеем работу, мы можем взять себе еще клиентов. 
        Но!! нет уверености, что клиенты будут поровну распределены, 
        особенно обидно, если какой-то поток будет спать, а тут будет крутится работа.
        Все зависит от планировщика системы и, в часности, его работы с мьютексами 
        и еще работы с мьютексами в pthread_cond_wait().

        Однако. Мне кажется работать с мьютексами правильнее, чем ловить пустые accept(), 
        после успешного select() (на одном и томже серверном сокете).
        хотя если мы работаем с процесами (fork()), то наверное это будет допустимым решением (или использование той же очереди mq).
        */
        if (th->gen->cancel_work){
            th->in_work = false;
        } else {
            if ((th->gen->sock_q != NULL) || (th->n_sockets == NUM_DEFAULT_FD_WORKER)){
                /*if we can get new client *//*or*//*if we don't have work*/
                while((th->gen->sock_q == NULL) && (th->gen->cancel_work == false)){
                    timecond.tv_sec     = timeout.tv_sec + 2;      // 2 sec
                    timecond.tv_nsec    = timeout.tv_usec * 1000; // nsec
                    pthread_cond_timedwait(th->gen->is_work, th->gen->mutex_queue, &timecond);            // block any work thread while we do't have more client
                }

                if (th->gen->cancel_work){
                    th->in_work = false;
                } else {
                    th_queue_t * new_client = pop_client_from_queue(&(th->gen->sock_q));
                    if (new_client->next == NULL){                      // WAS returned right vallue
                        client_addr = new_client->cl_addr;
                        cl_socket = new_client->fd_cl;
                        free(new_client);                               // check it!
                    } else
                        push_error(ERROR_QUEUE_POP);
                }
            }
            // else if (th->gen->sock_q->next == NULL && th->n_sockets != 0) 
            //      we already have work => do it;
        }

        pthread_mutex_unlock(th->gen->mutex_queue);


        if (th->in_work == false){
            sprintf(logbuf, "to Worker(%d): came command on close.", (int) th->tid);
            log_queue(th->fd.logger, logbuf);
            set_all_client_on_close(th);
        }

        FD_ZERO(th->socket_set_read);                      // do we should free last value?
        FD_ZERO(th->socket_set_write);
        // FD_SET(th->fd.cmd, th->socket_set_read);           // the same socket for all threads and proc
        
        // every new cicle we will clear all sets, because it is more easy then update sets, 
        // if was deleted we just clear by state - fd, close fd, free all data fd
        for (client_list_t * i = th->client; i != NULL; /*i = i->next*/){

            if (i->cur_state != CLIENT_STATE_CLOSED){
                /*C code notebene: as i know when i assign from pointer 
                    (any_type tmp = *(any_type *) p) the all struct data (from '*p') will be memcopy to new variable (to 'tmp') every time,
                    , it is not good. // when i use (*)p = (*)other => i copy only address for ppointer
                */
                if (i->is_writing)  FD_SET(i->fd, th->socket_set_write); 
                else                FD_SET(i->fd, th->socket_set_read);
                i = i->next;
            } else {
                if (i->next != NULL) {                      // if not last client::
                    i = i->next;
                    close_client_by_state(&(i->last));         // after it, "i" will be content last (prev == (i-1)) value, 
                } else                                      
                    close_client_by_state(&i);               // here i == NULL (was deleted last client) 
                
                th->n_sockets -= 1;
            }                                               
        }

        if (th->n_sockets == 0)
            continue;

        // if server ready and have new client not in set  - we need to add it to 
        if ((cl_socket != 0) && !FD_ISSET(cl_socket, th->socket_set_read)){
            // 2 - was new socket
            if (fcntl(cl_socket, F_SETFL, fcntl(cl_socket, F_GETFL, 0) | O_NONBLOCK) < 0) 
                push_error(ERROR_FCNTL_FLAG);
            
            init_new_client(&th->client, cl_socket, client_addr);    // check it (client is pointer already like in arg init() )
            th->n_sockets += 1;

            if (th->client->is_writing) FD_SET(th->client->fd, th->socket_set_write); 
            else                        FD_SET(th->client->fd, th->socket_set_read);

            sprintf(logbuf, "new client_sk accepted %s.", client_addr.sa_data);
            log_queue(th->fd.logger, logbuf);

            cl_socket = 0;
        }
        
        timeout.tv_sec = TIME_FOR_WAITING_MESSAGE_SEC;          // x sec +
        timeout.tv_usec = TIME_FOR_WAITING_MESSAGE_NSEC;        // 800000;  // + 0.8 sec
        
        int flag_sk = select(FD_SETSIZE, th->socket_set_read, th->socket_set_write, NULL, &timeout); // (FD_SETSIZE OR n_socket+1,
        if (flag_sk == -1){ 
            push_error(ERROR_SELECT);
        } else if ((flag_sk == 0) && (!FD_ISSET(th->fd.cmd, th->socket_set_read)) ){ // wait more then time
            
            // before code below we must be sure that nothing was disconnected, 
            // or if so - must delet connect without crash server!

            sprintf(logbuf, "Time over for thread %lu, close work with %d nums of client (anyone don't been ready).", th->tid, (th->n_sockets-NUM_DEFAULT_FD_WORKER));
            log_queue(th->fd.logger, logbuf);
            set_all_client_on_close(th);
        }else{ 
            /* Below part code work with clients sockets. We will try use the same cl->buffer for receive and (after prepare) for send*/
            
            bool continue_recv = true;

            for(client_list_t * cl_i = th->client; cl_i != NULL; cl_i = cl_i->next){ // i == 0 - server socket
                /* Read message to clientt buf */
                if (FD_ISSET(cl_i->fd, th->socket_set_read)){
                    
                    continue_recv = true;
                    // cl_i->busy_len_in_buf = 0; /* buffer will be clean inside parser, when catched whole message, or part of body*/
                    while (continue_recv){

                        len_recieved = recv(cl_i->fd, (cl_i->buf+cl_i->busy_len_in_buf), (sizeof(cl_i->buf) - cl_i->busy_len_in_buf), flag_recv);  // 1
                        if (len_recieved < 0) {
                            if (errno == EWOULDBLOCK){
                                // NEED TRY AGAIN - WAIT NEXT session
                            } else {
                                cl_i->cur_state = CLIENT_STATE_CLOSED;          // just error
                            }
                            continue_recv = false;
                            // continue;                                        // can not be
                        } else if (len_recieved == 0) {
                            cl_i->cur_state = CLIENT_STATE_CLOSED;
                            continue_recv = false;
                            // disconnect(sockets, &socket_set, i);
                            // continue;                                        // can not be
                        }else{

                            /* if we receive anything -> work more as can */

                            cl_i->busy_len_in_buf += len_recieved;                                                              // 2

                            flags_parser_t flag_parser = parse_message_client(cl_i);                                            // 3

                            if (flag_parser == ANSWER_READY_to_SEND  /*|| flag_parser == RECEIVED_PART_IN_DATA*/) {             // 4 
                                cl_i->busy_len_in_buf = 0;
                                continue_recv = false;
                                // break;
                            } else if (flag_parser == RECEIVED_PART_IN_DATA){
                                cl_i->busy_len_in_buf = 0;
                            }
                            
                            // .. cl_i->is_writing = true; // must be inside parser check it
                            sprintf(logbuf, "message %lu byte receive form socket %s.", cl_i->busy_len_in_buf, cl_i->addr.sa_data);
                            log_queue(th->fd.logger, logbuf);
                        }
                    }
                }
                /* do main work:: send ok, save message in file or send error to client socket */
                if (FD_ISSET(cl_i->fd, th->socket_set_write)){
                        
                        if(cl_i->cur_state == CLIENT_STATE_START){
                            /* just send OK message after accept() */
                            strcpy(cl_i->buf, REPLY_START); /*be sure, strcpy() added in the end '/0' simbol 1*/
                            if ( send(cl_i->fd, cl_i->buf, strlen(cl_i->buf), flag_send) < 0){
                                if (errno != EWOULDBLOCK)
                                    cl_i->cur_state = CLIENT_STATE_CLOSED;
                                continue;
                            }
                            cl_i->cur_state = CLIENT_STATE_INITIALIZED;
                            cl_i->is_writing = false;                               // THEN will be read from fd
                        } else {
                            /* main part */
                            if ( send(cl_i->fd, cl_i->buf, strlen(cl_i->buf), flag_send) < 0){
                                if (errno != EWOULDBLOCK)
                                    cl_i->cur_state = CLIENT_STATE_CLOSED;
                                continue;
                            }
                            if (cl_i->cur_state != CLIENT_STATE_SENDING_ERROR) 
                                cl_i->cur_state = CLIENT_STATE_WHATS_NEWS;
                            else
                                cl_i->cur_state = CLIENT_STATE_CLOSED;              // if error msg was send success
                            cl_i->is_writing = false;
                        }

                        // delet below !

                        
                    }
                    
                }


            } // select (flag from select)
    } // while

    // send_cmd_on_close logger
        
    gracefull_exit(NUM_CMD_PROC);
    gracefull_exit(NUM_CMD_THREAD);

    // free in main yeh?

}



void init_data_thread( threads_var_t * t_data, server_t server)
{
    
    pthread_mutex_t can_use_general_th_data;
    pthread_mutex_t can_see_condition;
    pthread_cond_t is_work;
    pthread_mutex_init(&can_use_general_th_data, NULL);
    pthread_mutex_init(&can_see_condition , NULL);
    pthread_cond_init(&is_work, NULL);

    t_data->gen.mutex_queue         = &can_use_general_th_data;
    t_data->gen.mutex_use_cond      = &can_see_condition;
    t_data->gen.is_work             = &is_work; 
    t_data->gen.sock_q              = NULL;                     // malloc(sizeof(*t_data->gen.sock_q)); //  malloc(sizeof(th_queue_t))
    // t_data->gen.sock_q->next        = NULL;
    t_data->gen.cancel_work         = false;

    // for i = 0; i < NUM_OF_WOREKER + 1; i++
    t_data->srv_sk = server.socket;                             //[i]
    t_data->log_id = server.log_id;                             //[i]
        
}

void destroy_data_thread(threads_var_t * t_data)
{
    pthread_mutex_destroy(t_data->gen.mutex_queue);
    pthread_mutex_destroy(t_data->gen.mutex_use_cond);
    pthread_cond_destroy(t_data->gen.is_work);

    free_thread_queue_sock(t_data->gen.sock_q);
    free(t_data);                                   // here will be free(t_data->gen) if "t_data->gen" is static 
                                                    // ! see in type declaration of threads_var_t
}

// here declared or in thread ?
void free_thread_queue_sock(th_queue_t * th_queue_sock)
{
    th_queue_t * prev_th_sock;
    while(th_queue_sock != NULL){
        prev_th_sock = th_queue_sock;
        th_queue_sock = th_queue_sock->next;
        free(prev_th_sock);
    }
}
