#include "mta_server.h"

/* call it when receive by client */
/* if answer ready, we save it to buf in client */
flags_parser_t parse_message_client(client_list_t * cl){

    flags_parser_t flag = RECEIVED_ERROR_CMD;
    // bool was_received_RN = false; 

    if (cl->is_writing || cl->cur_state == CLIENT_STATE_START) {
        push_error(ERROR_SOMETHING_IN_LOGIC);
    }

    if (cl->cur_state != CLIENT_STATE_DATA) // we except that cur_state == CLIENT_STATE_WHATS_NEWS
    {
        /*start recognazing state in word*/
        /* i don't know must i receive only with "/r/n" (CRLF) */
        if (is_start_string(cl->buf, STR_HELO) || is_start_string(cl->buf, STR_EHLO)){
            flag = handle_HELO(cl);
        } else if (is_start_string(cl->buf, STR_MAIL)){
            flag = handle_MAIL(cl);
        } else if (is_start_string(cl->buf, STR_RCPT)){
            flag = handle_RCPT(cl);
        } else if (is_start_string(cl->buf, STR_DATA)){
            flag = handle_DATA(cl);
        } else if (is_start_string(cl->buf, STR_QUIT)){
            flag = handle_QUIT(cl);
        } else if (is_start_string(cl->buf, STR_NOOP)){
            flag = handle_NOOP(cl);
        } else if (is_start_string(cl->buf, STR_VRFY)){
            flag = handle_VRFY(cl);
        } else if (is_start_string(cl->buf, STR_RSET)){
            flag = handle_RSET(cl);
        } else { // AS default
            flag = handle_default(cl);
        }

    } 
    else 
    {
        /* == CLIENT_STATE_DATA */
        flag = handle_DATA(cl);
        /* => receive in buffer while can */
    }
    
    return flag;
} 

// cl->data->to must be clear previsually
void get_mem_recipients(client_list_t * cl){
    /*must be check on NULL (but i must set poiner on NULL wherever)*/
    cl->data->to = malloc(STEP_RECIPIENTS*sizeof(*cl->data->to));
    for (int i = 0; i < STEP_RECIPIENTS; i++) 
        *(cl->data->to+i) = NULL;

    /*todo reallocate memory for unlimits*/
}

flags_parser_t handle_RSET(client_list_t * cl){
    flags_parser_t flag = RECEIVED_ERROR_CMD;

    if (cl->data->from != NULL){
        free(cl->data->from);
        cl->data->from = NULL;
    }
    free_client_forward(cl->data->to);
    get_mem_recipients(cl);

    flag = ANSWER_READY_to_SEND;
    strcpy(cl->buf, REPLY_RSET_OK);
    cl->busy_len_in_buf = sizeof(REPLY_RSET_OK);  //is not nessesary
    cl->is_writing = true; 
    cl->cur_state = CLIENT_STATE_WHATS_NEWS;

    return flag;
}

/*we must check the MX?*/
flags_parser_t handle_VRFY(client_list_t * cl){
    flags_parser_t flag = RECEIVED_ERROR_CMD;

    int start_next_mail = strlen(STR_VRFY); // strlen(cl->buf) - 

    char * v_mail = NULL;
    v_mail = get_mail(cl->buf, &start_next_mail);
    if (v_mail != NULL){
        char * domain_name = get_domain(v_mail);             // must not be free() becaose it is address (use after
        if (is_good_domain_name(domain_name)){                       // as i know it must be instde RCPT state! too
            strcpy(cl->buf, REPLY_VRFY_OK);
        } else {
            strcpy(cl->buf, REPLY_UN_MAIL);
        }
    } else {
        strcpy(cl->buf, REPLY_BAD_ARGS); // can't find mail inside
        cl->cnt_wrong_cmd += 1;
    }

    flag = ANSWER_READY_to_SEND;
    cl->busy_len_in_buf = strlen(cl->buf) ;  //is not nessesary, check it?
    cl->cur_state = CLIENT_STATE_WHATS_NEWS;
    cl->is_writing = true; 


    return flag;
}
/*
void set_answer_on_cmd(client_list_t * cl, char * ans, int is_ok_cmd){
    
    // flag = ANSWER_READY_to_SEND;
    strcpy(cl->buf, ans);
    cl->busy_len_in_buf = strlen(cl->buf) ;  //is not nessesary, check it?
    cl->is_writing = true; 

    if (is_ok_cmd){
        
    } else {
        cl->cnt_wrong_cmd += 1;
    }

    return;
}
*/

flags_parser_t handle_NOOP(client_list_t * cl){
    flags_parser_t flag = RECEIVED_ERROR_CMD;

    flag = ANSWER_READY_to_SEND;
    strcpy(cl->buf, REPLY_NOOP_OK);
    cl->busy_len_in_buf = sizeof(REPLY_NOOP_OK);  //is not nessesary
    cl->is_writing = true; 
    cl->cur_state = CLIENT_STATE_WHATS_NEWS;

    return flag;
}

flags_parser_t handle_QUIT(client_list_t * cl){
    flags_parser_t flag = RECEIVED_ERROR_CMD;

    flag = ANSWER_READY_to_SEND;
    strcpy(cl->buf, REPLY_QUIT);
    cl->busy_len_in_buf = sizeof(REPLY_QUIT);  //is not nessesary
    cl->is_writing = true; 
    cl->cur_state = CLIENT_STATE_DONE;

    /*clean up must be after close by state closed (after send answer!)*/

    return flag;
}


flags_parser_t handle_DATA(client_list_t * cl){
    flags_parser_t flag = RECEIVED_ERROR_CMD;
    if (cl->cur_state != CLIENT_STATE_DATA) {

        cl->data->body_len = 0;               /*for new message must be clean last value */

        if (cl->data->to[0] == NULL){
            strcpy(cl->buf, REPLY_DATA_ERR_START_TO);
            cl->cur_state = CLIENT_STATE_WHATS_NEWS;
            cl->cnt_wrong_cmd += 1;
        } else if (cl->data->from == NULL) {
            strcpy(cl->buf, REPLY_DATA_ERR_START_FROM);
            cl->cur_state = CLIENT_STATE_WHATS_NEWS;
            cl->cnt_wrong_cmd += 1;
        } else {
            strcpy(cl->buf, REPLY_DATA_START);
            cl->cur_state = CLIENT_STATE_DATA;
        }

        flag = ANSWER_READY_to_SEND; 
        cl->is_writing = true;

    } else {                                    // already inside
        char * end = strstr(cl->buf, END_DATA_INSIDE);
        if (end == NULL){
            if (strncmp(cl->buf, END_DATA_FIRST, strlen(END_DATA_FIRST)) == 0){
                end = cl->buf;              
            }
        }

        if (end != NULL){
            add_buf_to_body(cl, end);
            int status = save_message(cl->data, SAVE_TO_SHARE_FILE); 

            if (status == NOT_HAVE_RECIPIENTS){
                strcpy(cl->buf, REPLY_DATA_ERR_START_FROM);         // is very very seldom case (impossible)
                cl->cnt_wrong_cmd += 1;
            } else {
                strcpy(cl->buf, REPLY_DATA_END_OK);
            }
            
            cl->cur_state = CLIENT_STATE_WHATS_NEWS;
            flag = ANSWER_READY_to_SEND;
            cl->is_writing = true;

        } else {                                // need more data

            add_buf_to_body(cl, NULL);
            
            flag = RECEIVED_PART_IN_DATA;
            cl->is_writing = false;
        }
    }

    return flag;
}

void add_buf_to_body(client_list_t * cl, char * end){                       // check it!
    // int buf_len = strlen(cl->buf);
    int buf_len = cl->busy_len_in_buf;
    if (end != NULL)
        buf_len = end - cl->buf;                                            // the end not saved as data

    if ( cl->data->body_len + buf_len > cl->data->body_size ) {

        if( (cl->data->body_size + buf_len) >= MAX_SIZE_SMTP_DATA) {
            save_message(cl->data, SAVE_TO_TEMP_FILE);                      // here will be clean cl->body 
        } else {                                                            // else => can allocate more
            get_mem_to_body(cl->data);                                      /*first allocate for new body (be only here)*/
        }
    } 

    // strcat(cl->data->body, cl->buf);                                     // it is can be wrong we can send not string data!!!
    memcpy( (cl->data->body + cl->data->body_len), cl->buf, buf_len);       // cl->busy_len_in_buf = 0;must be AFTER IT (see in mta_thread.c recv cicle)
    cl->data->body_len += buf_len;
}

void get_mem_to_body (client_msg_t * msg){
    if (msg->body == NULL){
        msg->body = malloc(STEP_ALLOCATE_BODY*sizeof(char));
        msg->body_size = STEP_ALLOCATE_BODY;
        msg->body_len = 0;        
    } else if (msg->body_size < MAX_SIZE_SMTP_DATA) {
        msg->body_size += STEP_ALLOCATE_BODY;
        msg->body = realloc(msg->body, (msg->body_size)*sizeof(char)); // check as it work
    }
}

bool exist_in_list_to(client_list_t * cl, char * to){
    for (int i = 0; cl->data->to[i] != NULL; i++){
        if (strcmp(cl->data->to[i], to) == 0){
            return true;
        }
    }
    return false;
}

/*mail addres "->to" will be clear every time as call it function for every message session (mb it is not right?)*/
flags_parser_t handle_RCPT(client_list_t * cl){
    flags_parser_t flag;

    int start_next_mail = strlen(STR_RCPT); // strlen(cl->buf) - 
    char * to = NULL;

    to = get_mail(cl->buf, &start_next_mail);
    if (to != NULL){

        /*TODO right step (now STEP_RECIPIENTS is max) <= need counter (global in "struct to"*/
        free_client_forward(cl->data->to);                  // if not empty
        get_mem_recipients(cl);

        int cnt_i = 0;
        while (to != NULL){
            *(cl->data->to + cnt_i) = to;
            cnt_i++;
            if (cnt_i < STEP_RECIPIENTS ){
                to = get_mail(cl->buf, &start_next_mail);
                while((to != NULL) && exist_in_list_to(cl, to)){
                    to = get_mail(cl->buf, &start_next_mail);
                }
            } else
                break;
        }

        /*TODO more checking party: 1 - for error; 2 - for save in buffer not whole data in don't have /r/n !? */
        strcpy(cl->buf, REPLY_RCPT_OK);
    } else {
        strcpy(cl->buf, REPLY_BAD_ARGS); // can't find mail inside
        cl->cnt_wrong_cmd += 1;
    }

    flag = ANSWER_READY_to_SEND;
    cl->is_writing = true;
    cl->cur_state = CLIENT_STATE_RCPT; // check it

    return flag;
}

flags_parser_t handle_MAIL(client_list_t * cl){
    flags_parser_t flag;

    int start_next_mail = strlen(STR_MAIL); // strlen(cl->buf) - 

    if (cl->data->from != NULL){
        free(cl->data->from);
    }

    char * from = NULL;
    from = get_mail(cl->buf, &start_next_mail);
    if (from != NULL){
        cl->data->from = from;
        strcpy(cl->buf, REPLY_MAIL_OK);
    } else {
        strcpy(cl->buf, REPLY_BAD_ARGS); // can't find mail inside
        cl->cnt_wrong_cmd += 1;
    }

    flag = ANSWER_READY_to_SEND;
    cl->is_writing = true;
    cl->cur_state = CLIENT_STATE_MAIL;

    return flag;
}

// start len will be changed to real start (after compare !!)
// return - end_len after first "good" compare
char * get_mail(char * string, int * start_next_mail){
    char * begin_mail = NULL;
    char * end_mail = NULL;
    char * tmp = NULL;
    char * mail = NULL;

    /*right type mail name*/
    begin_mail = strchr(string+*start_next_mail, START_MAIL);            // pointed on '<'
    if (begin_mail != NULL){
        end_mail = strchr(begin_mail, END_MAIL);        // pointed on '>'
        if (end_mail != NULL){
            tmp = strchr(begin_mail, DOG_IN_MAIL);
            if(tmp == NULL || end_mail <= tmp)   return NULL;
            tmp = strchr(begin_mail, DOT_IN_MAIL);
            if(tmp == NULL || end_mail <= tmp)   return NULL;
            tmp = strchr(begin_mail, SPACE_IN_MAIL);
            if(tmp != NULL && end_mail >= tmp)   return NULL;        // if was ' ' inside mail name
            tmp = NULL;



        }
    }
    /*write mail without <> type mail name*/

    // ..todo it..

    /*prepare return*/
    if (begin_mail != NULL && end_mail != NULL){
        int len_mail = end_mail - begin_mail; 
        // if (len_mail > MAX_MAIL_LEN)    return NULL; // need?
        mail = malloc(len_mail*sizeof(char));               // free() only when close, or already write !! check it!
        strncpy(mail, begin_mail+1, len_mail-1);
        *(mail+len_mail-1) = '\0';                         // must be for defende malloc
            
        *start_next_mail += (end_mail - (string+*start_next_mail) );
    }
    
    return mail;
}

// strchr(message+n, anySTART); /*it function - try find first input for simbol in string*/ 

flags_parser_t handle_HELO(client_list_t * cl){
    flags_parser_t status = RECEIVED_WHOLE_MESSAGE;

    if (strlen(cl->buf)-strlen(STR_HELO) < MAX_MAIL_LEN){
        cl->hello_name = malloc(MAX_MAIL_LEN*sizeof(char));
        char * from = cl->buf+strlen(STR_HELO);
        char * name = cl->hello_name; 
        while( *from == '.' || *from == '@' ||  (*from >= '0' && *from <= 'z') )  // \r must be first !   
            *name++ = *from++;
        *name++ = '\0';
        from = NULL;                                        //cl->buf not changed
        name = NULL;

        strcpy(cl->buf, REPLY_HELO);
    } else {
        // strcpy(cl->buf, REPLY_BAD_ARGS); // too long name
        // cl->cnt_wrong_cmd += 1;
        strcpy(cl->buf, REPLY_HELO);
    }
    
    status = ANSWER_READY_to_SEND;
    cl->is_writing = true;
    cl->cur_state = CLIENT_STATE_HELO;

    return status;
}

flags_parser_t handle_default(client_list_t * cl){
    flags_parser_t flag;

    flag = RECEIVED_ERROR_CMD;
    strcpy(cl->buf, REPLY_UNREALIZED);
    cl->cnt_wrong_cmd += 1;
    cl->is_writing = true;
    return flag;
}

/* IF PASSED then add mail else send wrong*/
bool is_good_domain_name(char * name){
    struct hostent *hosten; 
    hosten = gethostbyname( name ); //name
    // sa.sin_addr.s_addr = ((in_addr*)hosten->h_addr_list[0])->s_addr;
    if (hosten != NULL)
        return true;
    else 
        return false;
    /*HZ IS NEED FREE FOR hostent (that was returned)*/
}
/* uppercase and compare. Not changing buf*/
bool is_start_string(char * buf, char * must_be){
    bool status = false;
    size_t len_must = strlen(must_be);
    if (len_must <= strlen(buf)){
        char * tmp_str = malloc(len_must*sizeof(char));
        strncpy(tmp_str, buf, len_must);
        UPPERCASE_STR_N(tmp_str, len_must);
        if (strncmp(tmp_str, must_be, len_must) == 0) 
            status = true;
        free(tmp_str);
    }
    return status;
}

/*----------------------------*/
/*below organ*/
/*----------------------------*/

void add_client_to_queue(th_queue_t ** cl_sock_queue, int sock, struct sockaddr cl_addr)
{
    th_queue_t *new_client_sock = (th_queue_t *)malloc(sizeof(th_queue_t));

    new_client_sock->next       = *cl_sock_queue; // addres last in addres next
    new_client_sock->fd_cl      = sock;
    new_client_sock->cl_addr    = cl_addr;
    *cl_sock_queue              = new_client_sock;
}

    /* free( returned_client );*/
    /*<! must be outside !>*/
th_queue_t * pop_client_from_queue(th_queue_t ** cl_queue)
{

    th_queue_t * client; 

    client = *cl_queue;
    *cl_queue = client->next;
    client->next = NULL;                // AS rest return only "addres" and "fd" for select socket (in var client)
    
    return client;
}

int init_new_client(client_list_t **client_list_p, int client_sock, struct sockaddr new_addr)
{
    int status = 0;
    client_list_t * new_client = (client_list_t *) malloc(sizeof(client_list_t));
    
    // init parameter of client
    new_client->addr                    = new_addr;
    new_client->cur_state               = CLIENT_STATE_START;       // must be sended greeting (as first message)
    new_client->fd                      = client_sock;
    new_client->cnt_wrong_cmd           = 0;
    new_client->is_writing              = true;                 // we recieve first message and must send answer 
                                                                // (but not sure, it if we can't send already)
    // init data of message
    new_client->data                    = (client_msg_t *) malloc(1*sizeof(client_msg_t));    
                                                                // i will try save message on disk and use only one pointer !  
    new_client->data->file_to_save      = NULL;                 // malloc(MAX_FILE_SIZE*sizeof(char)); // i thinck it not here
    new_client->data->from              = NULL;
    new_client->data->to                = malloc(STEP_RECIPIENTS*sizeof(char*));
    for(int i = 0; i < STEP_RECIPIENTS; i++) new_client->data->to[i] = NULL;
    new_client->data->body              = NULL;
    new_client->data->body_len          = 0;
    new_client->data->body_size         = 0;
    new_client->data->was_start_writing = false;                // true only for long message
    
    new_client->busy_len_in_buf         = 0;
    new_client->hello_name              = NULL;

    // ADD item to begining of list threads socket (clients)
    new_client->last                    = NULL;
    new_client->next                    = *client_list_p;
    if ((*client_list_p) != NULL)                                  // if not first value
        (*client_list_p)->last          = new_client;
    
    *client_list_p                      = new_client;           // main pointer always will be content the last value
    // client_list_p->next = not changing == last value

    return status;
}

/*delet from list the last element, and switch on ->next !!!*/
void free_one_client_in_list( client_list_t ** __restrict__ last_client_list){
    client_list_t * closed_client;  // equivalent i

    if(*last_client_list != NULL){
        closed_client          = *last_client_list;

        if (closed_client->next != NULL){
            closed_client->next->last = closed_client->last;
        }
        if (closed_client->last != NULL){
            closed_client->last->next = closed_client->next;
        }

        *last_client_list    = closed_client->next;             // free we can from just pointer (that contented a address in mem map)
                                                                // but we can't assign like here (it assign will be for local pointer, not global list!)
        // if ((closed_client->next == NULL) && (closed_client->last == NULL))
        //     last_client_list = NULL;                            // now &i = NULL; (see outside)

        closed_client->last = NULL;
        closed_client->next = NULL;
        free_client_message(closed_client->data);

        if (closed_client->hello_name != NULL)  
            free(closed_client->hello_name);
        free(closed_client);                                // free(*last_client_list);
        
        closed_client       = NULL;
    }
}

void free_client_message(client_msg_t * client_data){
    
    if (client_data != NULL){
        if (client_data->file_to_save != NULL)  free(client_data->file_to_save);
        if (client_data->from != NULL)          free(client_data->from);
        free_client_forward(client_data->to);
        if (client_data->body != NULL)          free(client_data->body);
        
        free(client_data);                      // check is it free (not local)?
    }
}

void free_client_forward(char ** to){
    int i = 0;
    if (to != NULL)            {
        while (*(to+i) != NULL) free(*(to+i++)); /* it is if we use not static size of mail*/
        
        free(to);           // check it is it address (not local) // must be first addres on [0] mail
        to = NULL;          // do not have an effect here
    }
}

// i see we must have double linked list because we can want disconnecting not last client from list
// void disconnect_client(client_list_t * client){
//     client_list_t * closed_client = client;
//     /*....*/
//     free_one_client_in_list(closed_client);
//     free(closed_client);
// } // we will use close by state only !

// now it func in not using // 
int close_client_by_state(client_list_t ** closed_client_i){
    int state = 0;

    client_list_t * prev_client = (*closed_client_i)->last;

    if (prev_client != NULL){
        prev_client->next = (*closed_client_i)->next;
    } /* else if (closed_client_i->next != NULL) { 

    }  else is prev == NULL && next == NULL */

    (*closed_client_i)->next = NULL;                    // NOT nesesarry
    free_one_client_in_list(closed_client_i);           // already is pointer on pointer for list

    *closed_client_i = prev_client;

    return state;                                       // success is 0
}

client_state next_state(client_state cur_state)
{
    return (cur_state == CLIENT_STATE_DONE)? CLIENT_STATE_DONE : cur_state + 1; // check it !
}
