#include "mta_server.h"
#include <sys/stat.h>
#include <stdio.h> // fileno

#define TEST_WRITE              "I was here! n\ntest write.rn\r\nn\nololon : t\tpassed."
#define TEST_MAIL_file          "olol@ail.rii"
#define TEST_MAIL_data_t2       "koko@olo.rur"
#define TEST_MAIL_data_t1       "koko2@easy.rur"
#define TEST_MAIL_data_f        "vova@olo.rur"

int global_counter_test;
void assert_str (char * string, const char * must_be, char * test_name);
void assert_bool(bool one, bool is, char * test_name);
char * get_mail(char * string, int * start_next_mail);
void assers_file_is_exist(char * file_name);
void assers_is_string_in_file(char * file_name, char * string);
void assert_true(bool is, char * msg);

int main(){
    global_counter_test = 0;
    printf("Test started\n");

    char buf[100];

    sprintf(buf, "oLolo Small data, need to Upper\n");
    UPPERCASE_STR_N(buf, 6);
    assert_str(buf, "OLOLO ", "to uppercase first 6");

    sprintf(buf, "OLOLO Small data, need to Upper\n");
    UPPERCASE_STR_SK_to_EK(buf, 7, 12);
    UPPERCASE_STR_SK_to_EK(buf, 26, 31);
    assert_str(buf, "OLOLO SMALL data, need to UPPER\n", "to uppercase selected");

    printf("\n");

    sprintf(buf, "MAil from: Small data, need to Upper\n");
    assert_bool(is_start_string(buf, STR_MAIL), true, "check STR_MAIL (in)");
    assert_bool(is_start_string(buf, STR_HELO), false, "check STR_HELO (not in)");
    sprintf(buf, "helofrom: Small data, need to Upper\n");
    assert_bool(is_start_string(buf, STR_HELO), false, "check STR_HELO (wrong in)");
    sprintf(buf, "helo from: Small data, need to Upper\n");
    assert_bool(is_start_string(buf, STR_HELO), true, "check STR_HELO (in)");
    sprintf(buf, "data\n\r");
    assert_bool(is_start_string(buf, STR_DATA), true, "check STR_DATA ()");

    printf("\n");

    sprintf(buf, "oLolo <Small@mail.dsdf> data,\n");
    int start_len = 5;
    assert_str(get_mail(buf, &start_len), "Small@mail.dsdf", "test mail parser");
    sprintf(buf, "oLolo <Smallmail.dsdf> data,\n");
    start_len = 5;
    assert_str(get_mail(buf, &start_len), NULL, "test mail dog");
    sprintf(buf, "oLolo <Small@ma il.dsdf> data,\n");
    start_len = 5;
    assert_str(get_mail(buf, &start_len), NULL, "test mail space");
    sprintf(buf, "oLolo <Small@maildsdf> data,\n");
    start_len = 5;
    assert_str(get_mail(buf, &start_len), NULL, "test mail dot");
    
    printf("\n");

    /*below checking for file create function*/
    FILE * fp;
    char filename[SIZE_FILENAME+strlen(TMP_NAME_TAG)];                      // name is static size not nedd to free()
    char **user_path = NULL;                                                // TMP AND READY
    char *user_file[2];
    user_file[0] = NULL;
    user_file[1] = NULL;

        client_msg_t * message = malloc(sizeof(client_msg_t));

    generate_filename(filename);
    printf("filename %s\n%d\n", filename, make_mail_dir(TMP_DIR_FOR_ALL_USER));
    strcat(filename, TMP_NAME_TAG);
    message->file_to_save = make_FILE(TMP_DIR_FOR_ALL_USER, filename);

	struct stat file_stat;
    if (stat(TMP_DIR_FOR_ALL_USER, &file_stat) == -1){
        make_mail_dir(TMP_DIR_FOR_ALL_USER);                    // create user's folders
        // mkdir(root_user_dir, GENERAL_DIR_MODE);
    } 
    assers_file_is_exist(TMP_DIR_FOR_ALL_USER);

    fp = fopen(message->file_to_save, "w");
    fprintf(fp,"Body: %s\n", TEST_WRITE);
    fclose(fp); 
    assers_file_is_exist(message->file_to_save);

    user_path = make_user_dir_path(MAILDIR, TEST_MAIL_file);                // in first only one file
    generate_filename(filename);
    user_file[TMP_DIR_ID] = make_FILE(user_path[TMP_DIR_ID], filename);
    user_file[READY_DIR_ID] = make_FILE(user_path[READY_DIR_ID], filename);
    
    assers_file_is_exist(user_path[TMP_DIR_ID]);
    assers_file_is_exist(user_path[READY_DIR_ID]);
    free(user_path[TMP_DIR_ID]);        
    free(user_path[READY_DIR_ID]); 

    message->from = malloc((1+strlen(TEST_MAIL_file))*sizeof(char));
    strcpy(message->from, TEST_MAIL_file);
    fp = fopen(user_file[TMP_DIR_ID], "w");
    fprintf(fp,"From: %s\r\n", TEST_MAIL_file);
    fclose(fp); 
    assers_file_is_exist(user_file[TMP_DIR_ID]);

    int status = 0;
    status |= copy_file(message->file_to_save, user_file[TMP_DIR_ID], "a");           // add to file general "body"
    status |= rename(user_file[TMP_DIR_ID], user_file[READY_DIR_ID]);       // ready for MTA_client
    assers_is_string_in_file(user_file[READY_DIR_ID], TEST_WRITE);

    free(user_file[TMP_DIR_ID]);                                // temp path
    remove(user_file[READY_DIR_ID]);
    free(user_file[READY_DIR_ID]); 

    free(message->from);   
    remove(message->file_to_save);
    free(message->file_to_save);    

    printf("\nCopy and rename is tested!\n");
    
    client_list_t * cl = NULL;
    int fd = 0;
    struct sockaddr addr;
    init_new_client(&cl, fd, addr);
    assert_true(cl->cur_state == CLIENT_STATE_START,     "CLIENT_STATE_START");

    cl->cur_state = CLIENT_STATE_WHATS_NEWS;
    flags_parser_t flag = handle_DATA(cl);
    assert_true(cl->cur_state == CLIENT_STATE_WHATS_NEWS,     "REPLY_DATA_ERR_START");

    /*set forward*/
    cl->data->from = malloc(strlen(TEST_MAIL_data_f)*sizeof(char));
    strcpy(cl->data->from, TEST_MAIL_data_f);
    cl->data->to = malloc(STEP_RECIPIENTS*sizeof(*cl->data->to));
    cl->data->to[0] = malloc(strlen(TEST_MAIL_data_t1)*sizeof(char));
    strcpy(cl->data->to[0], TEST_MAIL_data_t1);
    cl->data->to[1] = malloc(strlen(TEST_MAIL_data_t2)*sizeof(char));
    strcpy(cl->data->to[1], TEST_MAIL_data_t2);

    flag = handle_DATA(cl);
    assert_true(cl->cur_state == CLIENT_STATE_DATA,     "CLIENT_STATE_DATA");
    assert_true(flag == ANSWER_READY_to_SEND,           "ANSWER_READY_to_SEND");
    assert_true(cl->is_writing == true,                 "cl->is_writing true");

    sprintf(cl->buf, "olol\r\ni am here\n\r");
    cl->busy_len_in_buf = strlen(cl->buf);
    flag = handle_DATA(cl);
    assert_true(cl->cur_state == CLIENT_STATE_DATA,     "CLIENT_STATE_DATA");
    assert_true(flag == RECEIVED_PART_IN_DATA,          "RECEIVED_PART_IN_DATA");
    assert_true(cl->is_writing == false,                "cl->is_writing false");
    assert_str(cl->data->body, cl->buf,                 "check buf in body");
    assert_true(cl->data->body_len == strlen(cl->buf),  "len buf == len body");

    sprintf(cl->buf, "olol2\r\ni am here2\n\r2");
    strcat(cl->buf, END_DATA);
    cl->busy_len_in_buf = strlen(cl->buf);

    
    flag = handle_DATA(cl);
    assert_true(cl->cur_state == CLIENT_STATE_DATA,     "CLIENT_STATE_DATA");
    assert_true(flag == ANSWER_READY_to_SEND,           "ANSWER_READY_to_SEND");
    assert_true(cl->is_writing == true,                 "cl->is_writing true");
    assert_true(cl->data->body_len == 0,                "clear len body");

    char * last_file_name = malloc((SIZE_FILENAME+strlen(TMP_NAME_TAG))*sizeof(char)); // static size
    /*test big message*/
    char bigbuf[500000+1];
    memset(bigbuf, 'i', 500000*sizeof(char));
    for (int i = 0; i < 500000; ){
        int j = 0;
        while((i < 500000) && (j < BUFSIZE)){
            cl->buf[j++] = bigbuf[i++];
        }
        cl->busy_len_in_buf = strlen(cl->buf);
        flag = handle_DATA(cl);
        if (flag != RECEIVED_PART_IN_DATA)
            break;
        if (i > MAX_SIZE_SMTP_DATA){
            if ((cl->data->file_to_save != NULL) && (strcmp(last_file_name, cl->data->file_to_save) != 0)){
                strcpy(last_file_name, cl->data->file_to_save);
                printf("\nbeginig new file %s\n", last_file_name);
            }
        }
    }

    // WHILE IT NOT DELETED BY STATE
    assert_str(cl->data->file_to_save, last_file_name, "file for saved data not changed in time!");
    free(last_file_name);


    sprintf(cl->buf, "%s", END_DATA);
    cl->busy_len_in_buf = strlen(cl->buf);
    flag = handle_DATA(cl);

    printf("\ndata handler tested!\n");

    printf("\n");

    printf("\n");

    printf("\n");


    // int len = strlen(STR_HELO);
    // char * tmpbuf = malloc( (len+1)*sizeof(char));
    // memset(tmpbuf, 0x71, (len+1)*sizeof(char));
    // printf("%s\n", tmpbuf);
    // printf("len(p+4) = %lu \n size = %lu\n ", strlen(tmpbuf+4), sizeof(tmpbuf));
    // strcpy(tmpbuf, STR_HELO);
    // for (int i = 0; i < len+1; i++){
    //     printf(" <%d> ", tmpbuf[i]);
    // }

    printf("\n");

    return 0;
}

void assert_true(bool is, char * msg){
    global_counter_test++;
    int status = 0;

    if (!is)
        status = -1;

    if (status == 0){
        printf("#%.2d test - passed - %s\n", global_counter_test, msg);
    } else {
        printf("#%.2d test - ERROR - %s\n", global_counter_test, msg);
    }
}


void assert_bool(bool one, bool is, char * test_name){
    global_counter_test++;

    if (one == is){
        printf("#%.2d test - passed - <%s> \n", global_counter_test, test_name);
    } else {
        printf("#%.2d test - ERROR - <%s>\n", global_counter_test, test_name);
    }   
}

void assers_file_is_exist(char * file_name){
    global_counter_test++;
    int status = 0;

	struct stat file_stat;
    if (stat(file_name, &file_stat) == -1)
        status = -1;

    if (status == 0){
        printf("#%.2d test - passed - file exist for %s\n", global_counter_test, file_name);
    } else {
        printf("#%.2d test - ERROR - file exist for  %s\n", global_counter_test, file_name);
    }
}

#define STEP_LOC            15
void assers_is_string_in_file(char * file_name, char * string){
    global_counter_test++;
    int status = 0;

	struct stat file_stat;
    if (stat(file_name, &file_stat) == -1)
        status = -1;
    else{
        // read(file)
        FILE * fp = fopen(file_name, "r");
        char * contented = malloc(STEP_LOC*sizeof(char));
        int ch = 0, i = 0, size_f = STEP_LOC;
        while ( (ch = fgetc(fp)) != EOF){
            if (i >= size_f){
                size_f += STEP_LOC;
                contented = realloc(contented, size_f*sizeof(char));
                // printf("realloc len = %lu <%s>\n", strlen(contented), contented);
            }
            contented[i++] = ch;
        }
        fclose(fp);
        // compare
        if (strstr(contented, string) == NULL)
            status = -1;
    }

    if (status == 0){
        printf("#%.2d test - passed - file %s contented\n", global_counter_test, file_name);
    } else {
        printf("#%.2d test - ERROR - file %s contented\n", global_counter_test, file_name);
    }
}

void assert_str (char * string, const char * must_be, char * test_name){
    global_counter_test++;
    if (string == NULL || must_be == NULL){
        if (string == must_be) {
            printf("#%.2d test - passed - <%s>\n", global_counter_test, test_name);
        } else {
            printf("#%.2d test - ERROR - <%s>\n", global_counter_test, test_name);
            printf("\tin strlen=%4lu: |%s|\n", sizeof(string), string);
            printf("\tmust be strlen=%4lu: |%s|\n", sizeof(must_be), must_be);
        }
    } else {

        if (strncmp(string, must_be, strlen(must_be)) == 0) {
            printf("#%.2d test - passed - <%s>\n", global_counter_test, test_name);
        } else {
            printf("#%.2d test - ERROR - <%s>\n", global_counter_test, test_name);
            printf("\tin strlen=%4lu: |%s|\n", strlen(string), string);
            printf("\tmust be strlen=%4lu: |%s|\n", strlen(must_be), must_be);
        }
    }
}

/*delet below*/
void push_error(int num_error)
{
    bool need_exit = false;
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

    if (need_exit) gracefull_exit();
    // if (must_be_close_proc) close_proc(getpid());
    // if (must_be_close_thread) close_thread(pthread_self());

    //todo: logging
    // if (was_error in error)  perror("error error");

}


void gracefull_exit()
{
    return;
}
