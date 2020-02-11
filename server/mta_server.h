#ifndef __MTA_SERVER_H__
#define __MTA_SERVER_H__

#define TIME_FOR_WAITING_MESSAGE_SEC    20         
#define TIME_FOR_WAITING_MESSAGE_NSEC   800000  // add it to top value
#define TIME_IDLE_WORK_SEC              600     // if no one connection => our server will or restart or shut down // 10 minut?                 
#define SELECT_TIMEOUT_LOGGER           240


#define MAX_SIZE_SMTP_DATA          1023   // byte if more in client msg 

#define BUFSIZE                     512

#define ERROR_CREATE_NEW_SOCKET         -1              // just general out if error

#define ERROR_CREATE_NEW_SOCKET_push    0xF0000001
#define ERROR_BIND_push                 0xF0000002
#define ERROR_LISTEN_SOCKET_push        0xF0000003
#define ERROR_FORK                      0xF0000004
#define ERROR_SELECT                    0xF0000005
#define ERROR_ACCEPT                    0xF0000006
#define ERROR_FCNTL_FLAG                0xF0000007

#define SERVER_ADDR                     "0.0.0.0"
#define SMTP_PORT                       1996  
#define NUM_OF_WORKER                   1

#define REPLY_HELLO     ""

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

#include <stdin.h>
#include <time.h>
#include <fcntl.h>

#include <ctype.h> // isdigit toupper

#include <signal.h> // for SIGTERM hhzz and kill()
#include <mqueue.h> // for queue

#include <pthread.h>

//----------------------
// smtp keys string
//----------------------
#define STR_HELO            "HELO"
#define STR_EHLO            "EHLO"
#define STR_MAIL            "MAIL"
#define STR_RCPT            "RCPT"
#define STR_DATA            "DATA"
#define STR_NOOP            "NOOP"
#define STR_RSET            "RSET"
#define STR_QUIT            "QUIT"
#define STR_VRFY            "VRFY"

// replies smtp server
// 2xx
#define REPLY_HELLO          "220 SMTP bmstu myserver.ru ready\r\n"
#define REPLY_EHLO           "250-VRFY\r\n250-NOOP\r\n250-RSET\r\n"
#define REPLY_QUIT           "221 close connection\r\n"
#define REPLY_OK             "250 OK\r\n"
#define REPLY_ADMIN          "252 Administrative prohibition\r\n"
// 3xx          
#define REPLY_DATA           "354 Enter message, ending with \".\" on a line by itself\r\n"
// 4xx          
#define REPLY_TERMINATE      "421 closing server\r\n"
#define REPLY_UN_MAIL        "450 mailbox unavailable\r\n"
#define REPLY_MUCH_REC       "451 too much recepients\r\n"
#define REPLY_MEM            "452 memory is filled\r\n"
// 5xx          
#define REPLY_UNREC          "500 Unrecognized command\r\n"
#define REPLY_ARGS           "501 invalid argument(s)\r\n"
#define REPLY_SEQ            "503 Wrong command sequence\r\n"

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

} ;
// A static global variable or a function is "seen" only in the file it's declared in
// a local var in function is declared in only first calling (and after we will have the same value) 


typedef struct
{
    int t_error;
    bool must_be_closed; // control by server, if true break all works
    int srv_sk;
    pid_t log_id;
} general_threads_data_t;

#define MAX_RECIPIENTS      15
#define MAIL_LEN            150
// info about connection
struct client_msg_t
{
    char from[MAIL_LEN];
    char to[MAX_RECIPIENTS][MAIL_LEN];
    char body[MAX_SIZE_SMTP_DATA];
    int body_len;       // <= MAX_SIZE_SMTP_DATA, if no => save in tmp file!

    char * file_to_save;        // for big message // or save already
    bool was_start_writing;     // if we already write but don't have "quit" message
    // bool can_be_write;       // for other thread if we use it for write only
} ;

typedef enum {
     CLIENT_STATE_RESET,            // 8
     CLIENT_STATE_CLOSED,            // -1
     CLIENT_STATE_START,            // 1
     CLIENT_STATE_INITIALIZED,      // 2 // ? really nessesary
     CLIENT_STATE_HELO,             // 3
     CLIENT_STATE_EHLO,             // 4
     CLIENT_STATE_MAIL,             // 5 // FROM
     CLIENT_STATE_TCPT,             // 6 // TO - RECIPIENTS
     CLIENT_STATE_DATA,             // 7
     CLIENT_STATE_DONE                  // 3
    //  CLIENT_STATE_WAIT,             // 9 what next?
    //  CLIENT_STATE_NOOP,             // 10 what next?
    //  CLIENT_STATE_INVALID,             // 10 what next?
} client_state;

// more easy save all sockets in list
// but if i want delet different one - it is noot easy choosed 
struct  client_list_t
{ 
    int fd;         // client socket
    client_state cur_state;      // ready, in sending, cancel (failed, too many error) 
    struct sockaddr addr;
    bool is_writing;    // read == 0; write == 1;
    char buf[BUFSIZE];  // what we recieve from "client" (other server)
    // need here len too like "offset"

    struct client_msg_t * data; // can be many of message // here sava data of message before save it on disk
    struct client_list_t * next; // last is - NULL
};





// info about all workers
struct inst_proc_t
{
    pid_t pid;
    pid_t log_id; // 

    struct fd_t{
        int server_sock;
        int logger;
        int cmd;        // fd for command to server? 
    } fd;
    
    fd_set * socket_set_read;  // one from it in - will be server socket for checking ready or time out! 
    fd_set * socket_set_write;
    // fd_set * ready_set_read;
    // fd_set * ready_set_write;

    int n_sockets;      // number of conected socket // ?
    struct client_list_t *client; // for logger will be NULL
    
    bool in_work; // working / closed
};

struct inst_thread_t
{
    // do here need use the mutex? or just use general data when calling threads
    pid_t tid;
    
    // below general socket for all thread and proc ( i thinck may be own logger for every proc )
    struct fd_t {
        int server_sock;
        int logger;
        int cmd;        // fd for command to server? 
    } fd;
    
    fd_set * socket_set_read;  // one from it in - will be server socket for checking ready or time out! 
    fd_set * socket_set_write;
    
    int n_sockets;      // number of conected socket + 1 (server) // ?
    struct client_list_t *client; // for logger will be NULL
    
    bool in_work; // working / closed
};

// process
int init_process(struct inst_proc_t * proc, int fd_server_socket, int logger_id);
pid_t create_logger(int fd_server_socket);
int log_queue(int fd_logger, char* msg);


// thread
void run_thread(void * t_data);
int init_thread(struct inst_thread_t * th, int fd_server_socket, pid_t pid_logger);
void work_cicle(struct inst_thread_t * th);

int init_new_client(struct client_list_t * list, int client_sock, struct sockaddr * new_addr);

// main
bool save_to_file(char* fname, char* txt, bool info);
void push_error(int num_error);
void init_socket(int port, char *str_addr);
void gracefull_exit();//?

#endif // __MTA_SERVER_H__