#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

void ERROR(char* message);

int main(){
    char* adr = "127.0.0.1";
    int port = 9090;
    struct sockaddr_in serv_adr;
    serv_adr.sin_port = htons( port );
    serv_adr.sin_addr.s_addr = inet_addr( adr );
    serv_adr.sin_family = AF_INET;

    int sock = socket( PF_INET,SOCK_STREAM, 0 );
    if( bind(sock,(struct sockaddr*)&serv_adr,sizeof(serv_adr) ) == -1 )
        ERROR(" bind() error." );
    if( listen(sock,5) == -1 )
        ERROR("listen() error." );
    struct sockaddr_in clnt_adr;
    int clnt_adr_siz;
    int fd = accept( sock,(struct sockaddr*)&clnt_adr,&clnt_adr_siz );
    if( fd < 0 ){
        ERROR("accept() error.");
    }else{
        close( 1 );
        dup( fd );
        printf( "abcd\n" );
        close( fd );
    }
    close( sock );
    return 0;
}

void ERROR(char* message){
    printf("%s\n", message );
    exit( 1 );
}
