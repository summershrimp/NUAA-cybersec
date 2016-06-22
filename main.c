//
//  main.c
//  seclab-scanner
//
//  Created by Summer on 6/9/16.
//  Copyright © 2016 summer. All rights reserved.
//

#include <unistd.h>
#include <sys/types.h>       // socket
#include <sys/socket.h>      // socket
//#include <sys/ioctl.h>       // ioctl
#include <sys/fcntl.h>
#include <net/if.h>          // ifreq
#include <netinet/tcp.h>          // tcp
#include <netinet/in.h>          // tcp
#include <arpa/inet.h>
#include <string.h>          // strcpy
#include <stdio.h>           // printf
#include <errno.h>
#include <pthread.h>

int do_connect_scan(in_addr_t addr, int port);
void set_timeout(int sec);
void *connect_scan(void *args);
struct timeval tout;

typedef struct {
    in_addr_t addr;
    int tgt_port;
    int result;
} scan_args;


int main(int argc, const char * argv[]) {
    int i, j;
    in_addr_t addr = inet_addr("127.0.0.1");
    set_timeout(2);

    scan_args args_list[100];
    pthread_t pth_list[100];
    for(i=0; i<600; ++i)
    {
        for(j = 0; j<100; ++j)
        {
            args_list[j].addr = addr;
            args_list[j].tgt_port = i * 100 + (j + 1);
            //printf("sanning %d\n",args_list[j].tgt_port);
            pthread_create(pth_list + j, NULL, connect_scan, args_list + j);
        }
        for(j = 0; j<100; ++j)
        {
            pthread_join(pth_list[j], NULL);
            if(args_list[j].result)
                printf("Port%7d: OPEN\n", args_list[j].tgt_port);
        }
    }
    return 0;
}



void *connect_scan(void *args)
{
    scan_args *p = (scan_args *) args;
    p->result = do_connect_scan(p->addr, p->tgt_port);
    //printf("%d ", p->tgt_port);
    return NULL;
}

void set_timeout(int sec)
{
    tout.tv_sec = sec;
    tout.tv_usec = 0;
}

int do_connect_scan(in_addr_t addr, int port)
{
    int fd, res, valopt, retflag = 1;
    long arg;
    unsigned int temp;
    fd_set set;
    struct sockaddr_in tgt_addr;
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    tgt_addr.sin_addr.s_addr = addr;
    tgt_addr.sin_family = AF_INET;
    tgt_addr.sin_port = htons(port);
    
    arg = fcntl(fd, F_GETFL, NULL);
    arg |= O_NONBLOCK;
    fcntl(fd, F_SETFL, arg);
 
    // Trying to connect with timeout
    res = connect(fd, (struct sockaddr *)&tgt_addr, sizeof(tgt_addr));
    if (res < 0) {
        if (errno == EINPROGRESS) {
            //fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
            while (1)
            {
                FD_ZERO(&set);
                FD_SET(fd, &set);
                res = select(fd + 1, NULL, &set, NULL, &tout);
                if (res < 0 && errno != EINTR)
                {
                    //fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
                    retflag = 0;
                    goto ret;
                }
                else if (res > 0)
                {
                    // Socket selected for write
                    temp = sizeof(int);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &temp) < 0)
                    {
                        //fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno));
                        retflag = 0;
                        goto ret;
                    }
                    // Check the value returned...
                    if (valopt)
                    {
                        //fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt));
                        retflag = 0;
                        goto ret;
                    }
                    break;
                }
                else
                {
                    //fprintf(stderr, "Timeout in select() - Cancelling!\n");
                    retflag = 0;
                    goto ret;
                }
            }
        }
        else
        {
            //fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
            retflag = 0;
            goto ret;
        }
    }
ret:
    close(fd);
    return retflag;
}
