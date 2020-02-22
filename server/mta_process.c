#include "mta_server.h"


// save
bool save_msg_to_file(char* fname, char* txt, bool show_time)
{
    FILE* logf = fopen(fname, "a");
    char msg[BUFSIZE];
    if (!logf) {
        sprintf(msg, "error opening log file(%s)", fname);
        perror(msg);
        return false;
    }
    // variable add '\n' or not
    (txt[strlen(txt) - 1] == '\n') ? sprintf(msg, "%s", txt) 
                                    : sprintf(msg, "%s\n", txt); 
    // write with timetag
    if (show_time) {
        time_t ct = time(NULL);
        char* t = ctime(&ct);
        t[strlen(t) - 1] = '\0'; 

        fprintf(logf, "[%s]: %s", t, msg);
    } else  // without timetag
        fprintf(logf, "%s", msg);
        
    fflush(logf);
    fclose(logf);
    return true;
}

void run_logger(inst_proc_t *pr)
{
    char msg[BUFSIZE];

    FD_ZERO(pr->socket_set_read);
    FD_SET(pr->fd.logger, pr->socket_set_read);
    FD_SET(pr->fd.cmd, pr->socket_set_read);

    while(pr->in_work){
        struct timeval timeout;
        timeout.tv_sec = SELECT_TIMEOUT_LOGGER;
        timeout.tv_usec = 0;

        switch(select(FD_SETSIZE, pr->socket_set_read, NULL, NULL, &timeout)) {
            case 0:
                sprintf(msg, "Logger(%d): Timeout\n", getpid());
                printf("%s", msg);
                LOG(msg);
                break;
            default: {
                memset(msg, 0x0, sizeof(msg));
                
                printf("Logger(%d): i am in case(after select)\n", getpid());

                // one question what i must doing when receive cmd for thread? mb return it to back by mq_send()
                if (FD_ISSET(pr->fd.cmd, pr->socket_set_read)) {
                    if (mq_receive(pr->fd.cmd, msg, BUFSIZE, NULL) >= 0) {
                        if (strcmp(msg, "$") == 0) {    // msg start from $
                            sprintf(msg, "Logger(%d): accept command on close\n", getpid());
                            printf("%s", msg);
                            LOG(msg);
                            pr->in_work = false;
                        }
                    }
                }
                if (FD_ISSET(pr->fd.logger, pr->socket_set_read)) {
                    if (mq_receive(pr->fd.logger, msg, BUFSIZE, NULL) >= 0) {
                            printf("Logger(%d): received message : %s\n", getpid(), msg);
                            LOG(msg);
                    }
                }
                break;
            }
        }
    }
}

// return 0 or error code
// now proc only listen
int init_process(inst_proc_t * proc, int fd_server_socket, int logger_id) // 
{
    int status = 0;
    proc->pid = getpid();
    proc->log_id = logger_id;                       // process_is == LOGGER_PROC ? proc->pid : logger_id;

    proc->fd.server_sock = fd_server_socket;
    printf("fd_server_socket == %d; fd.server_sock == %d", fd_server_socket, proc->fd.server_sock); // debug // check it
    
    proc->in_work = true;
    proc->n_sockets = 0;
    proc->client = NULL;  // not have sockets
    proc->socket_set_write = malloc(sizeof(*proc->socket_set_write));
    proc->socket_set_read = malloc(sizeof(*proc->socket_set_read)); // not need for logger

    char lgname[20];
    sprintf(lgname, "/process%d", proc->log_id);

    // logger message queue init
    if (proc->pid == proc->log_id) {   // process_is == LOGGER_PROC 
        struct mq_attr attr;
        attr.mq_flags = attr.mq_curmsgs = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = BUFSIZE;
        
        // 0644: write, read, read  
        proc->fd.logger = mq_open(lgname, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);    
        // memset(lgname, 0x0, sizeof(lgname)); // for what?
        
        sprintf(lgname, "/exit%d", proc->log_id);
        proc->fd.cmd = mq_open(lgname, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);

    } else {    // if (process_is == WORKER_PROC) 
        // sleep(1);
        // proc->fd.logger = mq_open(lgname, O_WRONLY);  // for send message to logger
        // memset(lgname, 0x0, sizeof(lgname));
        // sprintf(lgname, "/exit%d", proc->log_id);
        // proc->fd.cmd = mq_open(lgname, O_RDONLY);   // for receive command
    }

    if (proc->fd.logger == -1 || proc->fd.cmd == -1) {
        perror("mq_open() failed in proc");
        free(proc->socket_set_read);
        free(proc->socket_set_write);
        free(proc);
        proc = NULL;
    } 
    // else if (proc->fd.logger > proc->fd.max || proc->fd.cmd > proc->fd.max) { // check fd message queue
    //     proc->fd.max = (proc->fd.logger > proc->fd.cmd) ? proc->fd.logger 
    //                                                     : proc->fd.cmd;
    //     (proc->pid == proc->log_id) ? printf("Logger is success started(%d): queue created \n", proc->log_id) 
    //                             : printf("Another proc (%d): linked log queue\n", getpid());
    // }
    printf("Logger is success started(%d): queue created \n", proc->log_id) ;

    return status;
}

#define LOGGER_PROC         0x01 // 
#define WORKER_PROC         0x02 // 

// can be use 
// 1 : if use process for worker then for every process have thread with logging
// 2: 1 process logger for all, and threads as workers
// now second var - only one on all server
pid_t create_logger(int fd_server_socket)
{   
    inst_proc_t * pr_logger = (inst_proc_t *) malloc(sizeof(inst_proc_t));
    // int fd_listen = 0;
    // pid_t pid_logger = -1;

    pid_t pid = fork(); // '0' for child / 'number' for parent
    switch (pid) {
        // process not created
        case -1: 
            push_error(ERROR_FORK);
            break;
        // process-child
        case 0:
            printf("Logger pid (%d) is starting (getpid == %d)\n", pid, getpid());

            pid = getpid();
                // INITIALIZATION process (log_id == getpid)
            if(init_process(pr_logger, fd_server_socket, LOGGER_PROC) && pr_logger != NULL)
            {
                char msg[BUFSIZE];
                sprintf(msg, "new log session(%d)", pid);
                printf("Logger(%d): started work\n", pid);
                LOG(msg);
                // run
                run_logger(pr_logger);                         // run logger
                free_process(pr_logger);                       // free proc
            }
            else
            {
                printf("Logger(%d): failed init\n", pid);
            }
            printf("Logger(%d): process killed\n", pid);
            kill(getpid(), SIGTERM);
            LOG("end work logger (after kill)");

            break;
        // process-parent
        default:
            printf("Server(%d): create proccess(%d)\n", getpid(), pid);
            break;
    }
    return pid;

}

void free_process(inst_proc_t * proc){
    if (proc != NULL) {
        printf("free_process (%d)\n", proc->pid);
        free(proc->socket_set_read);
        free(proc->socket_set_write);

            /* Need clear queue every proc or no (now i have only logger*/
        // free message queue if (getpid() == proc->pid == proc->log_id){}
        if (proc->fd.logger != -1)
            mq_close(proc->fd.logger);
        if (proc->fd.cmd != -1)
            mq_close(proc->fd.cmd);

        if (proc->log_id == proc->pid) {
            char* lgname = malloc(sizeof(*lgname) * 20); 

            sprintf(lgname, "/process%d", proc->log_id);
            mq_unlink(lgname);

            memset(lgname, 0x00, strlen(lgname));
            sprintf(lgname, "/exit%d", proc->log_id);
            mq_unlink(lgname);
            
            free(lgname);
        }
        // end free message queue

        free(proc);
        proc = NULL;
    } 
}


// use for send message to queue for logger
int log_queue(int fd_logger, char* msg)
{
    char buf[BUFSIZE];
    sprintf(buf, "%d <%s>", getpid(), msg);
    int res = mq_send(fd_logger, buf, strlen(buf), 0);
    if (res == -1)  perror(msg);
    return res;
}
