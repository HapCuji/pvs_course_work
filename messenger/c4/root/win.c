//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <libgen.h>
#include <unistd.h>

#include <time.h>

#include <netdb.h>


int usage(char *name)
{
    fprintf(stderr, "Usage: %s <port> <delay_sec> [max_packets]\n", basename(name));
    return 1;
}

int main(int ac, char *av[])
{
    int delay = 0, port = 0;
    int reuseaddr = 1;
    unsigned max_packets = -1;

    if (ac < 3) 
        exit(usage(av[0]));
    port  = atoi(av[1]);
    delay = atoi(av[2]);
    if (av[3]) 
        max_packets = atoi(av[3]);

    int MAX = max_packets;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = INADDR_ANY;

    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0)
        perror("reuseaddr");
    /* Give the socket FD the local address ADDR (which is LEN bytes long).  */
    if (bind(s, &sa, sizeof(sa)) < 0)
        perror("bind");

    printf("i am here 1 - and wait for sleep(10)\n");
    sleep(10);              // sleep 10 sec and connection to syn ack

    // listen no more 100 connection 
    if (listen(s, 100))
        perror("listen");
    // here will answer on syn and message while buffer not full and queue (for N) not full
    // nothing return 
    printf("i am here 2 - sleep(10) after it message\n");
    sleep(10);              // sleep 10 sec and connection to syn ack
    
    // add first connection to queue, but if will more => and set more (for example 100) will be add to queue all connection no more then set input for listen
    // below we take from fifo queue a number of socket and recv from it message (it will clean windows tcp and brack connection)
    int fd = accept(s, NULL, NULL);
    printf("i am here 3  - sleep (delay) after it message\n");
    if (delay > 0) 
        sleep(delay); // after first connectened sys wait a little time // here will 

    char buf[1024];
    int byte_n;
    while ( (byte_n = recv(fd, buf, sizeof(buf), 0)) > 0 && --max_packets){
        if (max_packets < MAX){
            printf("----\nOur counter: %d\n", max_packets);
            printf("buf: %d\n", byte_n);
        }
    }

    return 0;
}
