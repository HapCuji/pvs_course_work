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
        
    fflush(logf);                   // check it!!
    fclose(logf);
    return true;
}

void run_logger(inst_proc_t *pr)
{
    char msg[BUFSIZE];
    int cnt_exit_msg = 0;

    
    while(pr->in_work){
        FD_ZERO(pr->socket_set_read);
        FD_SET(pr->fd.logger, pr->socket_set_read);
        FD_SET(pr->fd.cmd, pr->socket_set_read);

        struct timeval timeout;
        timeout.tv_sec = SELECT_TIMEOUT_LOGGER;
        timeout.tv_usec = 0;

        switch(select(FD_SETSIZE, pr->socket_set_read, NULL, NULL, &timeout)) {
            case 0:
                sprintf(msg, "Logger(%d): Timeout\n", getpid());
                printf("%s", msg);
                LOG(msg);
                gracefull_exit(NUM_CMD_PROC);
                break;
            default: {
                memset(msg, 0x0, sizeof(msg));
                
                // printf("Logger(%d): i am in case(after select)\n", getpid());

                unsigned int cmd_logger_priority = LOGGER_PRIORITY;
                // one question what i must doing when receive cmd for thread? mb return it to back by mq_send()
                if (FD_ISSET(pr->fd.cmd, pr->socket_set_read)) {
                    if (mq_receive(pr->fd.cmd, msg, BUFSIZE, &cmd_logger_priority) >= 0) {
                        if (strcmp(msg, EXIT_MSG_CMD_LOGGER) == 0) {                                    // msg start from $
                            sprintf(msg, "Logger(%d): accept command on close\n", getpid());
                            if( (++cnt_exit_msg) == NUM_OF_WORKER){                                     // when logger received exit/ from all threads WORKERS (and proc if exist)
                                pr->in_work = false;
                                // printf("last cmd exit came : %s", msg);
                                strcat(msg, " Last cmd exit came. ");
                            }
                        } else
                            printf("logger in cmd %s\n", msg);
                        LOG(msg);
                    }
                }
                if (FD_ISSET(pr->fd.logger, pr->socket_set_read)) {
                    if (mq_receive(pr->fd.logger, msg, BUFSIZE, NULL) >= 0) {
                            // printf("Logger(%d): received message : %s\n", getpid(), msg);
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
    
    proc->in_work           = true;
    proc->n_sockets         = 0;
    proc->client            = NULL;  // not have sockets
    proc->socket_set_write  = malloc(sizeof(*proc->socket_set_write));
    proc->socket_set_read   = malloc(sizeof(*proc->socket_set_read)); // not need for logger

    char fname_log_cmd[20];

    // logger message queue init
    if (proc->pid == proc->log_id) {   // process_is == LOGGER_PROC 
        
        // 0644: write, read, read  
        sprintf(fname_log_cmd, "/process%d", NUM_LOGGER_NAME);  // proc->log_id);
        proc->fd.logger = open_queue_fd(fname_log_cmd, MODE_READ_QUEUE_NOBLOCK);
        
        sprintf(fname_log_cmd, "/exit%d", NUM_CMD_PROC);        //  proc->log_id);
        proc->fd.cmd = open_queue_fd(fname_log_cmd, MODE_READ_QUEUE_NOBLOCK);

    } else {    // if (process_is == WORKER_PROC) 
        // sleep(1);
        // sprintf(fname_log_cmd, "/process%d", NUM_LOGGER_NAME); // proc->log_id);
        // proc->fd.logger = mq_open(fname_log_cmd, O_WRONLY);  // for send message to logger
        // sprintf(fname_log_cmd, "/exit%d", proc->log_id);
        // proc->fd.cmd = mq_open(fname_log_cmd, O_RDONLY);   // for receive command
    }

    if (proc->fd.logger == -1 || proc->fd.cmd == -1) {
        perror("mq_open() failed in proc");
        free(proc->socket_set_read);
        free(proc->socket_set_write);
        free(proc);
        proc = NULL;                                    // be carefull it does not have an effect (local var)
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


// #define LOGGER_PROC         0x01 // 
// #define WORKER_PROC         0x02 // 

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
            init_process(pr_logger, fd_server_socket, pid); // LOGGER_PROC
            if (pr_logger != NULL)
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
        
            /* Need clear queue every proc or no (now i have only logger*/
        // free message queue if (getpid() == proc->pid == proc->log_id){}
        if (proc->fd.logger != -1)
            mq_close(proc->fd.logger);
        if (proc->fd.cmd != -1)
            mq_close(proc->fd.cmd);

        if (proc->log_id == proc->pid) {
            char* fname_log_cmd = malloc(sizeof(*fname_log_cmd) * 20); 

            sprintf(fname_log_cmd, "/process%d", NUM_LOGGER_NAME);
            mq_unlink(fname_log_cmd);

            sprintf(fname_log_cmd, "/exit%d", NUM_CMD_PROC);
            mq_unlink(fname_log_cmd);
            
            free(fname_log_cmd);
        }
        // end free message queue

        free(proc->socket_set_read);
        free(proc->socket_set_write);
        while(proc->client != NULL)
            free_one_client_in_list(&(proc->client));           // already is pointer on pointer for list
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


mqd_t open_queue_fd(char * fname_log_cmd, int mode){
    struct mq_attr attr;                // need for create file (eight?)
    attr.mq_flags = attr.mq_curmsgs = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = BUFSIZE;
        
    mqd_t fd = -1;
    if (mode == MODE_READ_QUEUE_NOBLOCK){
        fd = mq_open(fname_log_cmd, O_RDONLY | O_NONBLOCK); // , 0644, &attr);     // can be exist

        if (fd == -1){
            mq_unlink(fname_log_cmd);
            fd = mq_open(fname_log_cmd, O_EXCL | O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
        }

        if (fd > 0){
            char msg [BUFSIZE];
            while(mq_receive(fd, msg, BUFSIZE, NULL) > 0);
        }

    } else if (mode == MODE_WRITE_QUEUE_BLOCK) {  // CHECK BELOW
        
        fd = mq_open(fname_log_cmd, O_WRONLY); // ? , 0644, &attr);     //  will be blocked when write // | O_NONBLOCK

        if (fd == -1){
            mq_unlink(fname_log_cmd);
            fd = mq_open(fname_log_cmd, O_EXCL | O_CREAT | O_WRONLY, 0644, &attr); // | O_NONBLOCK
        }
    }
    
    return fd;
}