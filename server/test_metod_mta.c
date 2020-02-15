#include "mta_server.h"

int global_counter_test;
void assert_str (char * string, const char * must_be, char * test_name);
void assert_bool(bool one, bool is, char * test_name);
char * get_mail(char * string, int * start_next_mail);

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

    int len = strlen(STR_HELO);
    char * tmpbuf = malloc( (len+1)*sizeof(char));
    memset(tmpbuf, 0x71, (len+1)*sizeof(char));
    printf("%s\n", tmpbuf);
    printf("len(p+4) = %lu \n size = %lu\n ", strlen(tmpbuf+4), sizeof(tmpbuf));
    strcpy(tmpbuf, STR_HELO);
    for (int i = 0; i < len+1; i++){
        printf(" <%d> ", tmpbuf[i]);
    }

    printf("\n");

    return 0;
}

void assert_bool(bool one, bool is, char * test_name){
    global_counter_test++;

    if (one == is){
        printf("#%.2d test - passed - <%s> \n", global_counter_test, test_name);
    } else {
        printf("#%.2d test - ERROR - <%s>\n", global_counter_test, test_name);
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

//-----------
// delet below
//-----------

#define START_MAIL      '<'
#define END_MAIL        '>'
#define DOG_IN_MAIL     '@'
#define DOT_IN_MAIL     '.'
#define SPACE_IN_MAIL   ' ' // MUST be false

// start len will be changed to real start
// return - end_len after first "good" compare
char * get_mail(char * string, int * start_next_mail){
    char * begin_mail = NULL;
    char * end_mail = NULL;
    char * tmp = NULL;

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
            if(tmp == NULL || end_mail >= tmp)   return NULL;        // if was ' ' inside mail name
            tmp = NULL;
        }
    } else {
        /*write mail without <> type mail name*/
        // ..todo it..
        return NULL;
    }

    /*prepare return*/
    int len_mail = end_mail - begin_mail; 
    // if (len_mail > MAX_MAIL_LEN)    return NULL; // need?
    char * mail = malloc(len_mail*sizeof(char));    // free() only when close, or already write !! check it!
    strncpy(mail, begin_mail+1, len_mail-1);
    *start_next_mail += (end_mail - (string+*start_next_mail) );
    return mail;
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