#ifndef __MTA_SERVER_H__
#define __MTA_SERVER_H__

// #define __DEBUG_PRINT__                 /// comment it after test!!
#define __EVOLUTION_OUTPUT__

#define NUM_OF_WORKER                       3                                   
#define NUM_OF_THREAD                       (NUM_OF_WORKER + 1)             // +1 - it is accept thread and call workers
// КАК сделать число с количеством занятых бит по константе..
#define bm_WORKERS_WAIT                     0x00000007          // if anyone is true (can be 32 workers)
#define IS_WAITING_THREAD(cnt)              ((cnt & bm_WORKERS_WAIT))     

#define TIME_FOR_WAITING_ACCEPT_SEC     40 // 600                  
#define TIME_FOR_WAITING_ACCEPT_NSEC    0
#define TIME_FOR_WAITING_MESSAGE_SEC    30 // 20         
#define TIME_FOR_WAITING_MESSAGE_NSEC   800000  // add it to top value
#define TIME_IDLE_WORK_SEC              600     // if no one connection => our server will or restart or shut down // 10 minut?                 
#define SELECT_TIMEOUT_LOGGER           160 //240

#define MAX_USER_IN                     200
#define CMD_EXIT_USER                   "/q"

#define STEP_RECIPIENTS                 15
#define MAX_SIZE_SMTP_DATA              65536   // 64 Kb on client 
#define STEP_ALLOCATE_BODY              (MAX_SIZE_SMTP_DATA/10)    
                                            // if will more (in client msg)  => need save to file
                                            // clear the buffer and continue receive here

#define BUFSIZE                         512

#define ERROR_CREATE_NEW_SOCKET         -1                          // just general out if error

#define     MODE_READ_QUEUE_NOBLOCK         0x01            // BECAUSE READ IS BY select()
#define     MODE_WRITE_QUEUE_BLOCK          0x02            // if block is as default mode (write to queue is without select() now)
/*TODO: WRITE TO LOGGER WITH SELECT()*/


// create as enum type todo.?
#define ERROR_CREATE_NEW_SOCKET_push    0xF0000001
#define ERROR_BIND_push                 0xF0000002
#define ERROR_LISTEN_SOCKET_push        0xF0000003
#define ERROR_FORK                      0xF0000004
#define ERROR_SELECT                    0xF0000005
#define ERROR_ACCEPT                    0xF0000006
#define ERROR_FCNTL_FLAG                0xF0000007
#define ERROR_QUEUE_POP                 0xF0000008                  // error delet new element 
#define ERROR_SOMETHING_IN_LOGIC        0xF0000009                  // for example when we receive and have is_writing == true 
#define ERROR_WORK_WITH_FILE            0xF000000A                  // open write read
#define ERROR_MKDIR                     0xF000000B
#define ERROR_LINKED_CMD_LOG            0xF000000C
#define ERROR_FAILED_INIT               0xF000000D

// below not super error
#define PARSE_FAILED                    0xFF0000

#define MAILDIR                         "/home/hapcuji/tcp-ip/__server_dat/"         // ""/media/sf_kde_neon/_PVS/_server_dat/"
#define TMP_DIR_FOR_ALL_USER            "/home/hapcuji/tcp-ip/__server_dat/tmp/"         //"/media/sf_kde_neon/_PVS/_server_dat/tmp/" // "\\maildir\\tmp_prepare\\"       // just ? "tmp_prepare/"
#define SERVER_LOG_FILE_ABS             "/home/hapcuji/tcp-ip/__server_dat/server_log.txt"
#define SERVER_LOG_FILE_REF             "server_log"

#define NEWDIR                          "new/"                          // for save ready message for send
#define TMPDIR                          "cur/"                          // now in work with copy ro write (by server)
#define DUSTDIR                         "dust/"                         // mb for user? save deleted or for server save crashed message 
#define GENERAL_DIR_MODE                0700

#define TMP_NAME_TAG                    "_tmp"                          // as defende for file
#define SIZE_FILENAME                   25                              // not changed, always is unical

#define SERVER_ADDR                     "0.0.0.0"
#define SMTP_PORT                       1996  

#define TMP_DIR_ID                      1
#define READY_DIR_ID                    0

#define SAVE_TO_TEMP_FILE               0
#define SAVE_TO_SHARE_FILE              1       // file ready for client_mta
#define NOT_HAVE_RECIPIENTS             -500    // save to file failed

#define MSG_ERR_SIZE                    40

// #include <ctype.h>           for header
// #include <stdbool.h>         for header

#include <stdlib.h>         // memset()
#include <string.h>         // str
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

// #include <sys/stat.h> // for common
#include <sys/types.h>
#include <sys/socket.h> // FOR socket
#include <sys/time.h>
#include <netinet/in.h> // socaddr_in
#include <arpa/inet.h>  // 

#include <time.h>
#include <errno.h>

#include <fcntl.h>

#include <ctype.h> // isdigit toupper

#include <signal.h> // for SIGTERM hhzz and kill()
#include <mqueue.h> // for queue
#include <netdb.h> // for gethostname()

#include <pthread.h>

#include <sys/stat.h> // FOR COMMUNITI MKDIR status dir

#include "smpt_const.h"

// PARSE MAIL

#define START_MAIL                          '<'
#define END_MAIL                            '>'
#define DOG_IN_MAIL                         '@'
#define DOT_IN_MAIL                         '.'
#define SPACE_IN_MAIL                       ' ' // MUST be false
#define END_DATA_INSIDE                     "\r\n.\r\n"         // in windows too?
#define END_DATA_FIRST                      ".\r\n"         // in windows too?

#define MAX_NUM_WRONG_CMD                   6

// string
#define UPPERCASE_STR_N(str, n)             for(int k = 0; k<n; k++) { *(str+k) = toupper(*(str + k)); }
#define UPPERCASE_STR_SK_to_EK(str, sk, ek) for(int k = sk; k<ek; k++) { *(str+k) = toupper(*(str + k)); }

//------------------------------
// logger
//------------------------------

#define LOG(msg) save_msg_to_file(SERVER_LOG_FILE_ABS, msg, true)

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
    char * body;                        // body[MAX_SIZE_SMTP_DATA];
    int body_size;                       // body_len < MAX_SIZE_SMTP_DATA, if no => save in tmp file!
    int body_len;                       // body_len < MAX_SIZE_SMTP_DATA, if no => save in tmp file!
                                        // allocated memory

    char * file_to_save;                // for big message // or save already
    
    bool was_start_writing;             // if we already write but don't have "quit" message (but we have already not NULL file_to_save
    //above -> rename to:: can_be_save
    // bool can_be_write;               // for other thread ? if we use it for write only
} client_msg_t;

typedef enum client_state{
     CLIENT_STATE_RESET,                // 8
     CLIENT_STATE_SENDING_ERROR,        // next closed! -1
     CLIENT_STATE_CLOSED,               // -2
     CLIENT_STATE_START,                // 1
     CLIENT_STATE_INITIALIZED,          // 2 // ? really nessesary
     CLIENT_STATE_HELO,                 // 3
     CLIENT_STATE_EHLO,                 // 4
     CLIENT_STATE_MAIL,                 // 5 // FROM
     CLIENT_STATE_RCPT,                 // 6 // TO - RECIPIENTS
     CLIENT_STATE_DATA,                 // 7
     CLIENT_STATE_DONE,                 // 3
     CLIENT_STATE_WHATS_NEWS                 // 9  // when we send answer we don't know what new's
    //  CLIENT_STATE_NOOP,              // 10 what next?
} client_state;

// more easy save all sockets in list
// but if i want delet different one - it is noot easy choosed 
typedef struct client_list_S { 
    int fd;                             // client socket
    client_state cur_state;             // ready, in sending, cancel (failed, too many error) 
    struct sockaddr addr;   
    short cnt_wrong_cmd;                // to REPLY_TOO_MANY_ERROR
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
    pthread_cond_t * work_out;
    bool cancel_work;                   // was get command on stoping threads
    int status;

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

void * run_thread_worker(void * t_data);
void work_cicle(inst_thread_t * th);

void * run_thread_controller(void * t_data);
void thread_analitic_cicle(inst_thread_t * th);

void destroy_data_thread(threads_var_t * t_data);
void init_data_thread( threads_var_t * t_data, server_t server);

// sockets
int init_new_client(client_list_t ** list, int client_sock, struct sockaddr new_addr);
int close_client_by_state(client_list_t ** rest_client);
void set_all_client_on_close(inst_thread_t * th);

void free_client_message(client_msg_t * client_data);
void free_one_client_in_list(client_list_t ** __restrict__ last_client_list);
void free_client_forward(char ** to);
void get_mem_recipients(client_list_t * cl);

void add_client_to_queue(th_queue_t ** cl_queue, int sock, struct sockaddr cl_addr);
th_queue_t * pop_client_from_queue(th_queue_t ** cl_queue);
void free_thread_queue_sock(th_queue_t * th_queue_sock);

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
bool exist_in_list_to(client_list_t * cl, char * to);
flags_parser_t handle_DATA(client_list_t * cl);
flags_parser_t handle_QUIT(client_list_t * cl);
flags_parser_t handle_NOOP(client_list_t * cl);
flags_parser_t handle_RSET(client_list_t * cl);
flags_parser_t handle_VRFY(client_list_t * cl);
bool is_good_domain_name(char * name);
flags_parser_t handle_default(client_list_t * cl);


void set_answer_on_cmd(client_list_t * cl, char * ans, int is_ok_cmd);  // must be improve

// here file function
char ** make_user_dir_path(char *path, char *address_to);
char* make_FILE(char* dir_path, char* name);
int make_mail_dir(char* dir_path);
int copy_file(char * src_file, char * target_file, char * flag_to_open );
void generate_filename(char *seq);

void create_mail_dir_cache(void);
void clean_up_maildir(char * clean_path);

char * get_domain(char * addres);
char * get_mail(char * string, int * start_next_mail);

int save_message(client_msg_t *message, bool share_name);

void add_buf_to_body(client_list_t * cl, char * end);                     // check it!
void get_mem_to_body (client_msg_t * msg);

// main
// typedef struct error_S {
//     int num_error;
//     int fd_logger;          // need for calling other proc and thread to exit
// } error_t;

bool save_msg_to_file(char* fname, char* txt, bool info);

#define NUM_LOGGER_NAME         11
#define NUM_CMD_PROC            22  
#define NUM_CMD_THREAD          23
#define EXIT_MSG_CMD_THREAD     "#"
#define EXIT_MSG_CMD_LOGGER     "$"
#define LOGGER_PRIORITY         0 // hz for what, can delet
#define THREAD_PRIORITY         0 // hz for what, can delet

void push_error(int num_error);
int init_socket(int port, char *str_addr);
void gracefull_exit(int num_to_close);                              // ? 

mqd_t open_queue_fd(char * fname_log_cmd, int mode);


// global t_data
// threads_var_t * t_data;

#endif // __MTA_SERVER_H__