#ifndef __BUFF__
#define __BUFF__

// #include <stdio.h>
// #include <string.h>
#include <stddef.h>
#include <mqueue.h> // for queue

#define BUFSIZE         512
#define MAX_SOCKETS     10 // FD_SETSIZE

#define TIME_FOR_WAITING_MESSAGE_SEC    20         
#define TIME_FOR_WAITING_MESSAGE_NSEC   800000  // add it to top value
#define TIME_IDLE_WORK_SEC              600     // if no one connection => our server will or restart or shut down // 10 minut?                 
#define SELECT_TIMEOUT_LOGGER           240

#define SERVER_ADDR         "0.0.0.0"

// socket state
#define SOCKET_NOSTATE      -1

#define LOG(msg) save_msg_to_file("server_log", msg, true)

// struct of process data
struct process_t {
    pid_t pid;
    pid_t lgpid;
    bool worked;

    struct fd_t {
        int listen;
        int logger;
        int cmd;    // command message queue
        int max;
    } fd;

    // sets
    fd_set* writefds;
    fd_set* readfds;

    struct cs_node_t* s_list; // lists of clients sockets
};


struct buffer
{
    char * buf;
    size_t size;
    size_t pos;
};

struct client_info {
    /* data */
    char *name;
};

struct client_info clients[MAX_SOCKETS];
int sockets[MAX_SOCKETS];
unsigned int n_socket;


void data_init(void);
void disconnect(int *socket_list, fd_set * socket_set, int deleted_i); //, int * n_socket)


struct process_t* init_process(int* fd, pid_t pid_logger) ;
void free_process(struct process_t* proc);

bool save_msg_to_file(char* fname, char* txt, bool info);
pid_t create_logger();
void body_logger(int* fd, pid_t* pid);
void run_logger(struct process_t* pr);

#endif //__BUFF__
