#include "mta_server.h"


#include <sys/stat.h>


// method for logger and text

/*description how work with ../maildir/ */

/*VAR 1 MY*/
// mta_client must delivery mail from folder
// "../maildir/<to_domain_mail>/new/<timestamp_name_message>"
// when send is success just delet it.

// mta_server must save not ready file to 
// "../maildir/tmp_local/new/<temp_name>"

// when file <temp_name> is ready for sending (was end DATA)
//  -> copy message to all recipients to "../maildir/<to_domain_mail>/new/<timestamp_name_message>"


/*var 1*/
// client must delivery mail from folder
// "../maildir/<domain_mail>/<user_name>/new/"
// swap it to (or already delet)
// "../maildir/<domain_mail>/<user_name>/cur/"
// and when deliver is success to ?
// "../maildir/<domain_mail>/<user_name>/dust/"
// delet from server after end of work or timeout?


/*var 2*/
// just save as file with timestamp
// "../maildir/new/"
// not ready mail message in
// "../maildir/tmp/"
// mail that is sended by mta_client in
// "../maildir/dust/"
//if need that sending in 
// "../maildir/cur/"


// new method for DATA state
void generate_filename(char *seq) {
	struct timeval tv;
    // struct timezone tz;
    __timezone_ptr_t tzp = NULL;                                           // already ptr *
    
    gettimeofday(&tv, tzp);
    srand(tv.tv_usec);
    sprintf(seq,"%lx.%lx.%x",tv.tv_sec, tv.tv_usec, rand());        // have static size
}

// must be free after using! return len + 2 (just use it in any case)
char* concat_strings(char *s1, char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 2);                         // +2 is just in case. enough be +1 !
    strcpy(result, s1);
    strcat(result, s2);
    // here can use memcpy <= mb will be more fast

        #ifdef __DEBUG_PRINT__
    printf("\ndebug out concat_strings: <%s>\n", result);
        #endif // __DEBUG_PRINT__

    return result;
}

int make_mail_dir(char* dir_path) {
    int status = 0;
    status |= mkdir(dir_path, GENERAL_DIR_MODE);
    if (status != 0){
        if (errno != EEXIST){
            push_error(errno);
        }
        return status;
    }
    // create under folder
    char* new_dir = concat_strings(dir_path, NEWDIR);
    status |= mkdir(new_dir, GENERAL_DIR_MODE);
    free(new_dir);

    new_dir = concat_strings(dir_path, TMPDIR);
    status |= mkdir(new_dir, GENERAL_DIR_MODE);
    free(new_dir);

    new_dir = concat_strings(dir_path, DUSTDIR);
    status |= mkdir(new_dir, GENERAL_DIR_MODE);
    free(new_dir);

    return status;
}

char * get_domain(char * mail_addres){
    char * dog_start = strchr(mail_addres, DOG_IN_MAIL);
    return dog_start+1;
}
//                                  path
// for example: make_user_dir("../maildir/<user_name>   /new/")
//                                                      /cur/"
//                                                      /dust/"
// IF TEMP
// : make_user_dir("../maildir/TEMP_DIR/new/")
char** make_user_dir_path(char *path, char *address_to) {
    char * domain_name = get_domain(address_to);                            // must not be free() becaose it is address (use after
    // checking for => is_good_domain_name(domain_name);    as i know it must be instde RCPT state!
	char *root_user_dir = concat_strings(path, domain_name);
	strcat(root_user_dir, "/");                             // it is "sign" for dir

    struct stat file_stat;
    
    // cheking if exist  mail_dir!
	if (stat(root_user_dir, &file_stat) == -1){
        make_mail_dir(root_user_dir);                                           // create user's folders
    } 

    char **dir_path_for_save_new_message = malloc(2*sizeof(char *));            // TMP_DIR_ID + READY_DIR_ID

    dir_path_for_save_new_message[READY_DIR_ID] = concat_strings(root_user_dir, NEWDIR);
    if (stat(dir_path_for_save_new_message[READY_DIR_ID], &file_stat) == -1)
        mkdir(dir_path_for_save_new_message[READY_DIR_ID], GENERAL_DIR_MODE);

    dir_path_for_save_new_message[TMP_DIR_ID] = concat_strings(root_user_dir, TMPDIR);
    if (stat(dir_path_for_save_new_message[TMP_DIR_ID], &file_stat) == -1)
        mkdir(dir_path_for_save_new_message[TMP_DIR_ID], GENERAL_DIR_MODE);


    free(root_user_dir);
    return dir_path_for_save_new_message;
}
//                                          dir_path
// for example: make_user_dir("../maildir/<user_name>/new/<file_name>")
char* make_FILE(char* dir_path, char* file_name) {
    // dir_path == "../maildir/<domain_name>/new/" <= must be already existed
    char* new_file = concat_strings(dir_path, file_name);

    return new_file;
}   

// share == CAN_SHARE_FILE, SAVE_TO_TEMP
int save_message(client_msg_t *message, bool share_name) {
	if ((strcmp(message->from, "") == 0) || (strcmp(message->to[0], "") == 0) || (strcmp(message->body, "") == 0))
		push_error( PARSE_FAILED);
	int i;

    int status = 0;
    FILE *fp = NULL;
    char filename[SIZE_FILENAME+strlen(TMP_NAME_TAG)];                          // name is static size not nedd to free()
    // char *tmp_path = NULL;
    char **user_path = NULL;                                                    // TMP AND READY dir
    char *user_file[2];                                                         // tmp and ready full filename
    user_file[0] = NULL;
    user_file[1] = NULL;

    /*if was start writing 1=>*/
    if (message->file_to_save != NULL){                                         // && message->was_start_writing == true;
            /* just added */
		fp = fopen(message->file_to_save, "a");

        // fprintf(fp,"%s",message->body);                                    // i thinck here must be memcpy
        fwrite(message->body, sizeof(char), message->body_len, fp);

        fclose(fp); ///

    } else {
        /* create new tmp */
            // tmp_path = make_user_dir_path(MAILDIR, TMP_DIR_FOR_ALL_USER);    // in first only one file
		generate_filename(filename);
        strcat(filename, TMP_NAME_TAG);
        message->file_to_save = make_FILE(TMP_DIR_FOR_ALL_USER, filename);

        fp = fopen(message->file_to_save, "w");
        fwrite(message->body, sizeof(char), message->body_len, fp);
       
        fclose(fp); 
            // free(tmp_path);                         
		
    }
    
    /*if data ready to send 3=> rename file and copy for all recipients \
    (with different MX) if you can't  => just rewrite below code for cicle i !) !!*/

    if (share_name){
        FILE *fp_i = NULL;
        // resulting rename
        for (i = 0; (i < STEP_RECIPIENTS) && (message->to[i] != NULL); i++) {           // while to != NULL    
            user_path = make_user_dir_path(MAILDIR, message->to[i]);                // in first only one file
		    generate_filename(filename);
            user_file[TMP_DIR_ID] = make_FILE(user_path[TMP_DIR_ID], filename);
            user_file[READY_DIR_ID] = make_FILE(user_path[READY_DIR_ID], filename);
            free(user_path[TMP_DIR_ID]);                                            // temp path
            free(user_path[READY_DIR_ID]);       
            
		    fp_i = fopen(user_file[TMP_DIR_ID], "w");
            fprintf(fp_i,"From: %s\r\n",message->from);
            fprintf(fp_i,"To: %s\r\n",message->to[i]);
            fclose(fp_i); 

            status |= copy_file(message->file_to_save, user_file[TMP_DIR_ID], "a");     // add to file general "body"
            status |= rename(user_file[TMP_DIR_ID], user_file[READY_DIR_ID]);           // ready for MTA_client
            
            free(user_file[TMP_DIR_ID]);        
            free(user_file[READY_DIR_ID]);                                              // resulting path // user_path MUST BE inside user_file (if no use free())
        }
        if (i == 0)
            status = NOT_HAVE_RECIPIENTS;
        
        remove(message->file_to_save);                                       // clear and delet temp file for save data
        free(message->file_to_save);
        message->file_to_save = NULL;
        message->was_start_writing = false;


    } else {

        message->was_start_writing = true;

    }

    /*Where free body to and from is better? ... here? check it*/
    message->body_len = 0;

	return status;
}

// use flag to open == 'w' or 'a'
int copy_file(char * src_file, char * target_file, char * flag_to_open ){
    int status = 0;

    FILE * dist, * src;
    char ch;

    if ( (strcmp(flag_to_open, "w") != 0) && (strcmp(flag_to_open, "a") != 0) )
        sprintf(flag_to_open, "a");      // as default
        
    dist = fopen(target_file, flag_to_open);
    if( dist == NULL ){
        fclose(dist);
        push_error(ERROR_WORK_WITH_FILE);
        return ERROR_WORK_WITH_FILE;
    }

    src = fopen(src_file, "r");
    if( src == NULL ){
        fclose(src);
        push_error(ERROR_WORK_WITH_FILE);
        return ERROR_WORK_WITH_FILE;
    }

    while( ( ch = fgetc(src) ) != EOF )  fputc(ch, dist);
    
    fclose(dist);
    fclose(src);

    return status;
}


