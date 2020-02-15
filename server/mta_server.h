#ifndef __MTA_SERVER_H__
#define __MTA_SERVER_H__

#define TIME_FOR_WAITING_ACCEPT_SEC     600                  
#define TIME_FOR_WAITING_ACCEPT_NSEC    0
#define TIME_FOR_WAITING_MESSAGE_SEC    20         
#define TIME_FOR_WAITING_MESSAGE_NSEC   800000  // add it to top value
#define TIME_IDLE_WORK_SEC              600     // if no one connection => our server will or restart or shut down // 10 minut?                 
#define SELECT_TIMEOUT_LOGGER           240


#define MAX_SIZE_SMTP_DATA          65536   // 64 Kb on client 
                                            // if will more (in client msg)  => need save to file
                                            // clear the buffer and continue receive here

#define BUFSIZE                     512

#define ERROR_CREATE_NEW_SOCKET         -1              // just general out if error

// create as enum type todo.?
#define ERROR_CREATE_NEW_SOCKET_push    0xF0000001
#define ERROR_BIND_push                 0xF0000002
#define ERROR_LISTEN_SOCKET_push        0xF0000003
#define ERROR_FORK                      0xF0000004
#define ERROR_SELECT                    0xF0000005
#define ERROR_ACCEPT                    0xF0000006
#define ERROR_FCNTL_FLAG                0xF0000007
// below not super error
#define ERROR_QUEUE_POP                 0xF0000008  // error delet new element 
#define ERROR_SOMETHING_IN_LOGIC        0xF0000009  // for example when we receive and have is_writing == true 

#define SERVER_ADDR                     "0.0.0.0"
#define SMTP_PORT                       1996  
#define NUM_OF_WORKER                   1
#define NUM_OF_THREAD                   (NUM_OF_WORKER + 1) // +1 - it is accept thread and call workers

// #include <ctype.h>           for header
// #include <stdbool.h>         for header

#include <stdlib.h>         // memset()
#include <string.h>         // str
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h> // FOR socket
#include <netinet/in.h> // socaddr_in
#include <arpa/inet.h>  // 

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <stdint.h>
#include <time.h>
#include <fcntl.h>

#include <ctype.h> // isdigit toupper

#include <signal.h> // for SIGTERM hhzz and kill()
#include <mqueue.h> // for queue
#include <netdb.h> // for gethostname()

#include <pthread.h>

#include "smpt_const.h"


// string
#define UPPERCASE_STR_N(str, n)             for(int k = 0; k<n; k++) { *(str+k) = toupper(*(str + k)); }
#define UPPERCASE_STR_SK_to_EK(str, sk, ek) for(int k = sk; k<ek; k++) { *(str+k) = toupper(*(str + k)); }

//------------------------------
// logger
//------------------------------

#define LOG(msg) save_to_file("server_log", msg, true)

typedef struct server_t
{
    int socket; // server socket (for listen)
    // int num_proc; //
    pid_t log_id;
    // pid_t* worker; // server thread or proc will be main (if exit => gracefull exit)

} server_t;
// A static global variable or a function is "seen" only in the file it's declared in
// a local var in function is declared in only first calling (and after we will have the same value) 

// #define MAX_RECIPIENTS      15 // why not any count ??? // only we must block from spammers 
                                    // by control addres sender in controller_thread !! TODO IT !
#define MAX_MAIL_LEN            150
// info about connection
typedef struct client_msg_t
{
    char *from; // from[MAX_MAIL_LEN];
    char * *to; // to[MAX_MAIL_LEN];    // to[MAX_RECIPIENTS][MAIL_LEN];
    char * body;            // body[MAX_SIZE_SMTP_DATA];
    int body_len;                       // body_len < MAX_SIZE_SMTP_DATA, if no => save in tmp file!

    char * file_to_save;                // for big message // or save already
    bool was_start_writing;             // if we already write but don't have "quit" message
    // bool can_be_write;               // for other thread if we use it for write only
} client_msg_t;

typedef enum client_state{
     CLIENT_STATE_RESET,                // 8
     CLIENT_STATE_CLOSED,               // -1
     CLIENT_STATE_START,                // 1
     CLIENT_STATE_INITIALIZED,          // 2 // ? really nessesary
     CLIENT_STATE_HELO,                 // 3
     CLIENT_STATE_EHLO,                 // 4
     CLIENT_STATE_MAIL,                 // 5 // FROM
     CLIENT_STATE_RCPT,                 // 6 // TO - RECIPIENTS
     CLIENT_STATE_DATA,                 // 7
     CLIENT_STATE_DONE,                  // 3
     CLIENT_STATE_WHATS_NEWS                 // 9  // when we send answer we don't know what new's
    //  CLIENT_STATE_NOOP,              // 10 what next?
    //  CLIENT_STATE_INVALID,           // 10 what next?
} client_state;

// more easy save all sockets in list
// but if i want delet different one - it is noot easy choosed 
typedef struct client_list_S { 
    int fd;                             // client socket
    client_state cur_state;             // ready, in sending, cancel (failed, too many error) 
    struct sockaddr addr;   
    char * hello_name;                  // domain name (send in helo/ehlo)

    bool is_writing;                    // read == 0; write == 1; 
                                        /*  если мы в состоянии чтения то команда еще не принята, либо мы принимаем текст, 
                                            если в записи, то у сервера уже готов ответ(он в это время хранится в буфере) 
                                        */
    char buf[BUFSIZE];                  // what we recieve from "client" (other server)
    ssize_t busy_len_in_buf;            // how much available in buf

    client_msg_t * data;                // can be many of message // here sava data of message before save it on disk
    struct client_list_S * next;        // followed node with client, last is - NULL // *HEAD
    struct client_list_S * last;        // *TAIL == prevision client
} client_list_t;





// info about all workers
typedef struct inst_proc_t
{
    pid_t pid;
    pid_t log_id;                       // can get just logger_fd, but when init proc(thread) i call mq_open(name_with_pid_logger, W_ONLY)

    struct fd_t{
        int server_sock;
        int logger;
        int cmd;                        // fd for command to server? 
    } fd;
    
    fd_set * socket_set_read;           // one from it in - will be server socket for checking ready or time out! 
    fd_set * socket_set_write;
    // fd_set * ready_set_read;
    // fd_set * ready_set_write;

    int n_sockets;                      // number of conected socket // ?
    client_list_t *client;              // for logger will be NULL
    
    bool in_work;                       // working(1) / closed(0)
} inst_proc_t;


typedef struct th_queue_S{
    struct th_queue_S * next;           // can't use just th_queue_t here (because it is defined below)
    int fd_cl;
    struct sockaddr cl_addr;
}   th_queue_t;

typedef struct general_threads_data_t{
    pthread_mutex_t * mutex_queue;      // thread with mutex can exchange with queue
    pthread_mutex_t * mutex_use_cond;   // not sure?
    pthread_cond_t * is_work;           // for thread
    th_queue_t * sock_q;
    // bool see_queue;
} general_threads_data_t;

typedef struct threads_var_t
{
    // int t_error;                     // return value
    // bool must_be_closed;             // control by server, if true break all works
    // type_thread_t type_th;
    int srv_sk;
    pid_t log_id;

    general_threads_data_t gen;         // now static (one for all thread not need have malloc) 
    /* except it (general_threads_data_t), other vars - can be unical for every thread */

} threads_var_t;

/*
typedef enum type_thread_enum {
    WORKER_THREAD,
    CONTROLLER_THREAD // must be only one
} type_thread_t;
*/

typedef struct inst_thread_t
{
    // do here need use the mutex? or just use general data when calling threads
    pthread_t tid;                          // must be pthread_t
    pid_t log_id;                       // can get just logger_fd, but when init proc(thread) i call mq_open(name_with_pid_logger, W_ONLY)
    
    general_threads_data_t * gen;       // it is will be pointed on static data (we can free it but it don;t have an effenct see in main())
    
    // below general socket for all thread and proc ( i thinck may be own logger for every proc )
    struct th_fd_t {
        int server_sock;
        int logger;
        int cmd;                        // fd for command to server? 
    } fd;
    
    fd_set * socket_set_read;           // one from it in - will be server socket for checking ready or time out! 
    fd_set * socket_set_write;
    
    int n_sockets;                      // number of conected socket + 1 (server) // ?
    client_list_t *client;              // for logger will be NULL
    
    bool in_work;                       // working(1) / closed(0)
} inst_thread_t;

// process
int init_process(inst_proc_t * proc, int fd_server_socket, int logger_id);
pid_t create_logger(int fd_server_socket);
int log_queue(int fd_logger, char* msg);
void free_process(inst_proc_t * proc);


// thread
int init_thread(inst_thread_t * th, threads_var_t * t_data);
void free_thread(inst_thread_t * th);

void run_thread_worker(void * t_data);
void work_cicle(inst_thread_t * th);

void run_thread_controller(void * t_data);
void thread_analitic_cicle(inst_thread_t * th);

// sockets
int init_new_client(client_list_t * list, int client_sock, struct sockaddr new_addr);
int close_client_by_state(client_list_t * rest_client);
void free_client_message(client_msg_t * client_data);
void free_one_client_in_list(client_list_t * last_client_list);

void add_client_to_queue(th_queue_t * cl_queue, int sock, struct sockaddr cl_addr);
th_queue_t * pop_client_from_queue(th_queue_t * cl_queue);

// parser
typedef enum flags_parser_E {
    RECEIVED_ERROR_CMD,                // wrong command, send answer from buffer (and crear it)
    RECEIVED_WITHOUT_END,                // (WE not find /r/n) save received, wait without answer
    RECEIVED_PART_IN_DATA,              // receive more wile don't have '.'
    RECEIVED_WHOLE_MESSAGE,             // -> we must send answer (if was error in cmd too)
    ANSWER_READY_to_SEND
} flags_parser_t;
flags_parser_t parse_message_client(client_list_t * client); /* call it when receive by client */
bool is_start_string(char * buf, char * must_be);

// handle / in parser
flags_parser_t handle_HELO(client_list_t * cl);
flags_parser_t handle_MAIL(client_list_t * cl);
flags_parser_t handle_RCPT(client_list_t * cl);
flags_parser_t handle_DATA(client_list_t * cl);
flags_parser_t handle_QUIT(client_list_t * cl);
flags_parser_t handle_NOOP(client_list_t * cl);
flags_parser_t handle_VRFY(client_list_t * cl);
bool is_good_domain_name(char * name);
flags_parser_t handle_default(client_list_t * cl);

// main
bool save_to_file(char* fname, char* txt, bool info);
void push_error(int num_error);
int init_socket(int port, char *str_addr);
void gracefull_exit();                              // ? 

#endif // __MTA_SERVER_H__