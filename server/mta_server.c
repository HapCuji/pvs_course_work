#include "mta_server.h"

/*
create threads worker:
    create socet
    bind
    listen 
    answer / logging  / add to new sets

create proc logger:
    mqueue
    

set connection 
get post and save


+ все потоки или процессы работают с одним прослушевающем соктом (т.к должен быть один порт)

несколько процессов (потоков) могут слушать разных отправителей 
    в зависимости от полученного набора они иметь уникальный достут к входящим соединениям
        (т.е. если мы имеем в наборе одного из потоков ya.ru => у другого процесса его в наборе быть не должно иначе один процесс начнет обрабатывать соединение, 
        а другой продолжит (хз может и нет или жто будет нормально)
        => если набор будет один у всезх (для потоков это нормальн) в этом случае просто будем обновлять данные о клиенте (в общем наборе тоже пока они не заполнятся любым из рабочих потоков)
            после завешающего сообщения готовый майл будет сохранен в определенную папку (под уникальным именем)
            при этом указатель на это сообщение следует освубодить, оставив только текст
            ++ важно! важно определять, кто именно отправил (из общего набора было бы естественно) 
                => ищем сообщение именно от этого подключения и продолжаем его заполнять
                    если отключится - удалить текст и освубодиь память
                    если успех - сохранить на диск
            ++ для длинных сообщений - важно не держать в памяти весь объем 
                => следует сохранять сообщения впо чучуть (
                    при этом
                    а. держать файл в другой папке - до успешного приема или освубождения места
                    б. сохранять сразу - куда надо но добавить пометку long, что будет означать, что сообщение еще не готово (и перезаписывать каждые MAX_RECIEVE BYTE!)

        )

для сервера - 
доделать открытие и создание одного процесса, сделать состояния и отправить в папку готовое письмо

для клиента - 
сделать состояния для одного процесса и отправить на сервер письмо (готовое) из папки
уточнить что делать с локальным письмом

*/
// #include <stdlib.h>         // memset()
// #include <string.h>         // str
// #include <stdio.h>
// #include <stdbool.h>

// #include <sys/types.h>
// #include <sys/socket.h> // FOR socket
// #include <netinet/in.h> // socaddr_in
// #include <arpa/inet.h>  // 

// #include <sys/time.h>
// #include <time.h>
// #include <unistd.h>
// #include <errno.h>

// #include <stdin.h>
// #include <time.h>
// #include <fcntl.h>

// #include <ctype.h> // isdigit

// #include <signal.h> // for SIGTERM hhzz and kill()
// #include <mqueue.h> // for queue

// #include <pthread.h>

#include <dirent.h> // for file clear and find

void clean_up_maildir(char * clean_path){
    DIR *dir;
    struct dirent *ent;
    char file[256+strlen(clean_path)];
    char file_inside[256+strlen(clean_path)];
    if ((dir = opendir (clean_path)) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            if ( (strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0) ){
                strcpy(file, clean_path);
                strcat(file, ent->d_name);

                sprintf(file_inside, "%s/", file);
                clean_up_maildir(file_inside);

                remove(file);
                printf ("removed: %s\n", file);
            }
        }
        closedir (dir);
    } else {
        /* could not open directory */
        if (strcmp(clean_path, MAILDIR) == 0){
            perror ("could not open directory");
            exit(EXIT_FAILURE);
            return;
        } else {
            // inside is not nessesary (recursive)
            return;
        }
    }
}

void create_mail_dir_cache(void){
    char cur_folder[sizeof(MAILDIR)];
    char * path = malloc(sizeof(MAILDIR)*sizeof(char));
    strcpy(path, MAILDIR);
    char * next_folder = NULL; // strchr(path, '/');

    int step = 0, status = 0;
    while ( (next_folder = strchr(path+step, '/')) != NULL)
    {
        step = next_folder - path + 1;
        strncpy(cur_folder, path, (step));
        status = mkdir(cur_folder, GENERAL_DIR_MODE);
        if (status < 0){
            if (errno != EEXIST){
                perror("Can't work, in bad dir =(");
                exit(EXIT_FAILURE);
            }
        } else {
            printf("created dir %s \n", cur_folder);
        }
    }
    free(path);
}

/* now here will be only one process */
static server_t server; // do really need static 
#define COMMAND_SERVER_CLEAR_CACHE                  "-remove cache"

int main(int argc, char**argv)
{
    if (argc != 3) {
        if ((argc == 2) && (strcmp(argv[1], COMMAND_SERVER_CLEAR_CACHE) == 0)){
            create_mail_dir_cache();
            clean_up_maildir(MAILDIR);
        } else {
            printf("Usage: \nmta_server <addr> <port>\nExample: mta_server 0.0.0.0  1996\n");
            printf("Usage: \nmta_server -<cmd>\nExample: mta_server %s\n", COMMAND_SERVER_CLEAR_CACHE);
        }
        exit(0);
    }
    int i = 0;
    int port = 0;
    for (i=0; i < strlen(argv[2]); i++)
        port = ((int)argv[2][i] - (int)'0') + port*10;
    if (port <= 0 || port >= 0xFFFF) perror("range"); // need set server range?

    create_mail_dir_cache();
    clean_up_maildir(MAILDIR);

    server.socket = init_socket(port, argv[1]);
    server.log_id = create_logger(server.socket);
    

    // struct inst_proc_t  * inst_proc = malloc(NUM_OF_WORKER*sizeof(inst_proc_t)); // for thread and logger?
    // init_data(server.socket, &inst_proc);

    // ...
    //  i not sure that know how right use the mutex <= 
    // is the right way to use all the same soket_sets in all threads?
    // or is the right way to choosed mutex and change set for only one thread (not accepted for other)
    // or есть 2 варианта с мьютексами 
        // 1 - использовать mutex_can_use_queue (для доступа)  cond_var(для указания рабочим потокам что появилась работа) 
                //один поток на прием и записть поступивших (sockets) в очередь (другие потоки будут читать ) 
        // 2 - как я хотел мьютекс на работу с поступившим соединением 
                //(пологаю граммотнее использовать accept только одному потоку)
    // ...


    // pthread_t * save_on_disk_thread      = malloc(sizeof(pthread_t));                // <..? do we need it ?..>
    pthread_t * controller_thread           = malloc(sizeof(pthread_t));
    pthread_t * work_threads                = malloc(NUM_OF_WORKER*sizeof(pthread_t));
    threads_var_t * t_data                  = malloc(sizeof(threads_var_t));
    init_data_thread(t_data, server);

    pthread_create(controller_thread, NULL, run_thread_controller, t_data);
    for (i = 0; i < NUM_OF_WORKER; i++){
        pthread_create(&work_threads[i], NULL, run_thread_worker, t_data);
    }

    // here can be user controll
    //  pthread_cancel(&work_threads[i]);

    for (i = 0; i < NUM_OF_WORKER; i++){
        pthread_join(work_threads[i], NULL); // all logic will be inside
        // if (t_data[i].t_error != 0) printf("\n|error thread %d| code error = %d\n", i, t_error);
    }
    pthread_join(*controller_thread, NULL);   // controller_thread[0]

    printf("Server will be closed\n");

    destroy_data_thread(t_data);
    close(server.socket);
    free(work_threads);
    free(controller_thread);

    return 0;
}

/* mb good way will be if we storage status inside thread and proc instance*/
void gracefull_exit(int num_cmd)
{
    /*here will be close proc and thread with they sockets*/
    char cmd_name[BUFSIZE];                         // and logger name
    int status = 0;
    mqd_t fd_for_send;

    switch(num_cmd){

        case NUM_CMD_THREAD:
            sprintf(cmd_name, "/exit%d", NUM_CMD_THREAD);
            fd_for_send = mq_open(cmd_name, O_WRONLY);
            if (fd_for_send < 0){
                perror("grace error /exit thread\n");
                exit(EXIT_FAILURE);
            }
            // close thread () (threads close all himself) 
            // must be before closing logger (there and be sended anything)
            status = mq_send(fd_for_send, EXIT_MSG_CMD_THREAD, sizeof(EXIT_MSG_CMD_THREAD), THREAD_PRIORITY);
            if (status < 0){
                perror("grace error EXIT_MSG_CMD_THREAD\n");
                exit(EXIT_FAILURE);
            }
            break;

        case NUM_CMD_PROC:
            /*when thread will be closed they will be send cmd on close logger*/
            sprintf(cmd_name, "/exit%d", NUM_CMD_PROC);
            fd_for_send = mq_open(cmd_name, O_WRONLY);
            if (fd_for_send < 0){
                perror("grace error /exit logger (proc)\n");
                exit(EXIT_FAILURE);
            }
            // close logger (logger unlink mq and instances himself)
            status = mq_send(fd_for_send, EXIT_MSG_CMD_LOGGER, sizeof(EXIT_MSG_CMD_LOGGER), LOGGER_PRIORITY);
            if (status < 0){
                perror("grace error EXIT_MSG_CMD_LOGGER\n");
                exit(EXIT_FAILURE);
            }
            break;

        default:
            break;
    }
    
    /*the main will close the main socket*/ 

    return;
}



// can try like in example create mq_queue and 
int init_socket(int port, char *str_addr)
{   

    // socket
    int server_sk = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sk == ERROR_CREATE_NEW_SOCKET) push_error(ERROR_CREATE_NEW_SOCKET_push);

    // addr and port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(str_addr); // inet_addr(SERVER_ADDR)
    addr.sin_port = htons(port);

    // connection socket and addres and port
    if( bind(server_sk, (struct sockaddr*) &addr, sizeof(addr)) < 0) push_error(ERROR_BIND_push);

    if (fcntl(server_sk, F_SETFL, fcntl(server_sk, F_GETFL, 0) | O_NONBLOCK) ) // it can help server not blocked when recieved data or send? but is is noy nesessary
        push_error(ERROR_FCNTL_FLAG); 
        
    if (listen(server_sk, SOMAXCONN) < 0) push_error(ERROR_LISTEN_SOCKET_push);
    printf("Start server listening --- press Ctrl-C to stop\n");

    // todo: create logger for current socket? or general after return
    return server_sk;
}


/* my own engine for recieve server error */
/* in future (todo):: input can be - pointer and i can here something assignment:: 
mb - function for calling in ""main"" func (if pointer not null) or more*/

void push_error(int num_error)
{
    bool need_exit = true;      // check it
    // bool must_be_close_thread = false;
    // bool must_be_close_proc = false;
    // bool must_be_perror = false;  // the same need_exit?

    char msg[MSG_ERR_SIZE];
    switch (num_error)
    {
        case ERROR_CREATE_NEW_SOCKET_push:
            sprintf(msg, "Error create new socket\n");
            break;
        case ERROR_BIND_push:
            sprintf(msg, "Error bind\n");
            break;
        case ERROR_LISTEN_SOCKET_push:
            sprintf(msg, "Error listen\n");
            break;
        case ERROR_FORK:
            sprintf(msg, "Server(%d): fork() failed\n", getpid()); 
            break;
        case ERROR_SELECT:
            sprintf(msg, "Error select in thread = %lu, proc = %d\n", (unsigned long int)pthread_self(), getpid());
            // must_be_close_thread = true;
            break;
        case ERROR_ACCEPT:
            sprintf(msg, "Error accept in thread = %lu, proc = %d\n", (unsigned long int)pthread_self(), getpid());
            break;
        default:
            sprintf(msg, "Somthing wrong #%d\n", num_error);
            break;
    }
    LOG(msg);
    // log_queue(server.fd.logger, msg); // if without exiting

    if (need_exit) {
        gracefull_exit(NUM_CMD_THREAD);     
        // gracefull_exit(NUM_CMD_PROC);    // it must be closed in thread
    }
    // if (must_be_close_proc) close_proc(getpid());
    // if (must_be_close_thread) close_thread(pthread_self());

    //todo: logging
    // if (was_error in error)  perror("error error");

}