#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>

#define ERROR(x) printf("%s\n",strerror( x ) )

int main()
{
    struct sockaddr_in serv_adr;
//    serv_adr.sin_addr.s_addr = inet_addr("127.0.0.1");
    inet_pton( AF_INET, "127.0.0.1", &serv_adr.sin_addr );
    serv_adr.sin_port = htons(6379);
    serv_adr.sin_family = AF_INET;

    int sock = socket( PF_INET,SOCK_STREAM,0 );
    if( connect(sock,(struct sockaddr*)&serv_adr,sizeof(serv_adr)) == -1 )
        ERROR(errno);

    char buf2[1024] = "cl is not handsome\n";
    send( sock,buf2,strlen(buf2),0);

    sleep(3);

    char buf1[1024] = "cl is handsome\n";
    send( sock,buf1,strlen(buf1),0);
}
