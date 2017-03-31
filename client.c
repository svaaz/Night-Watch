#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "keepalive.h"

int sock_id = -1;

#define INVALID_SOCKET 1
#define TRY_AGAIN 2

int sendKeepAlive()
{
    keepalive_t msg;

    msg.m_bit = 1;
    msg.frequency = 2;
    msg.action = 1;
    msg.syslog_ip = 0;
    msg.reserved = 0;

    char str[100];
    int t =0;

    memset(&str, 100, 0);
    memcpy(&str, &msg, sizeof(msg));

    if(sock_id == -1)
    {
        printf("\n Invalid Socket !");
        return INVALID_SOCKET;
    }

    {
        if (send(sock_id, str, sizeof(msg), 0) == -1) 
        {
            perror("send");
            return TRY_AGAIN;
        }

    }

}

void createSocket()
{

    int len;
    struct sockaddr_un remote;

    if ((sock_id = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        perror("socket");
        return;
    }

    printf("Trying to connect...\n");

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(sock_id, (struct sockaddr *)&remote, len) == -1) 
    {
        perror("connect");
        return;
    }

    printf("Connected.\n");

}


void DestroySocket()
{
    close(sock_id);
    sock_id =-1;
}

int main(void)
{

    for(;;)
    {
        createSocket();
        
        while(1)
        {
            if(sendKeepAlive() == TRY_AGAIN)
            {
                break;
            };
            sleep(5);
        }

        DestroySocket();
    }
    return 0;
}
