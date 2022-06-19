### dup函数和dup2函数

```c
#include <unistd.h>
int dup(int file_descriptor);
int dup2(int file_descriptor_one,int file_descriptor_two)
```

```dup```函数创建一个新的文件描述符, 该新文件描述符与```file_descriptor```指向相同的文件/管道/网络连接(```dup```返回的文件描述符总是取当前可用的最小整数值)

```dup2```函数作用类似, 不过它返回第一个不小于```file_descriptor_two```的整数值.

两个函数失败时都是返回```-1```并设置```errno```

---

**利用```dup```实现一个基本的```CGI```服务器**

```6.1-CGI.c```

```c
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
```

可以用以下命令连接上溯服务器

```
telnet 127.0.0.1 9090
```

可以发现如下输出

```
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
abcd
Connection closed by foreign host.
```

可以发现服务端使用```printf```函数输出的```abcd```在客户端显示了.

那是因为我们关闭了服务端的标准输出流```close(1)```, 导致当前最小可用的整数是```1```

再调用```dup```函数, 使得复制出的新文件描述符为```1```, 而```printf```则是往```1```中输出数据, 所以相当于完成了一个重定向.



### readv函数和writev函数

```c
#include <sys/uio.h>
ssize_t readv(int fd,const struct iovec* vector,int count);
ssize_t writev(int fd,const struct iovec* vector,int count);
```

##### iovec结构体定义

其中```iovec```定义如下所示

```struct iovec``` 结构体定义了一个向量元素，通常这个 ```iovec``` 结构体用于一个多元素的数组.

对于每一个元素，```iovec``` 结构体的字段 ```iov_base``` 指向一个缓冲区，这个缓冲区存放的是网络接收的数据（```read```），或者网络将要发送的数据（```write```）。```iovec``` 结构体的字段 ```iov_len``` 存放的是接收数据的最大长度（```read```），或者实际写入的数据长度（```write```）。

```c
struct iovec {
    /* Starting address (内存起始地址）*/
    void  *iov_base;   

    /* Number of bytes to transfer（这块内存长度） */
    size_t iov_len;  
};
```

##### stat结构体定义

```c
#include <sys/stat.h>
struct stat {
  dev_t     st_dev;    //文件的设备编号
  ino_t     st_ino;    //节点
  mode_t    st_mode;   //文件的类型和存取的权限
  nlink_t    st_nlink;   //连到该文件的硬连接数目，刚建立的文件值为1
  uid_t     st_uid;    //用户ID
  gid_t     st_gid;    //组ID
  dev_t     st_rdev;   //(设备类型)若此文件为设备文件，则为其设备编号
  off_t     st_size;   //文件字节数(文件大小)
  unsigned long st_blksize;  //块大小(文件系统的I/O 缓冲区大小)
  unsigned long st_blocks;  //块数
  time_t    st_atime;   //最后一次访问时间
  time_t    st_mtime;   //最后一次修改时间
  time_t    st_ctime;   //最后一次改变时间(指属性)
};
```



```readv```和```writev```在成功返回读入/写出```fd```的字节数, 失败则返回```-1```并设置```errno```

```web```服务器解析完一个```http```请求时, 如果目标文档存在且用户有读权限, 就需要返回一个```http```应答传输该文档

这个```http```应答包含一个状态行, 多个头部字段, 一个空行和文档的实际内容

其中前三部分可能在同一块内存, 文档被放置在另一块内存, 使用```writev```可以把他们同时写出(不需要拼接)

下面应用该函数实现一个简易的```http```服务器(为了方便,省去解析请求操作,收到请求直接返回响应)

收到请求时, 该服务器总是返回用户输入的参数文件

```6.2-writev.c```

```
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
static const char* status_line[2] = {"200 OK","500 Internal server error"};

void ERROR(char* message);

int main(int argc,char* argv[] ){
    if( argc <= 3 ){
        printf("usage: %s ip_address port_number filename\n",basename(argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );
    const char* file_name = argv[3];
    struct sockaddr_in serv_adr;
    bzero( &serv_adr,sizeof( serv_adr ) );
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr( ip );
    serv_adr.sin_port = htons( port );

    int sock = socket( PF_INET,SOCK_STREAM, 0 );
    if( bind(sock,(struct sockaddr*)&serv_adr,sizeof(serv_adr) ) == -1 )
        ERROR("bind() error." );
    if( listen(sock,5) == -1 )
        ERROR("listen() error," );
    struct sockaddr_in clnt_adr;
    int clnt_adr_siz;
    int fd = accept( sock,(struct sockaddr*)&clnt_adr,&clnt_adr_siz );
    if( fd < 0 )
        ERROR("accept() error." );
    char* file_buf;
    struct stat file_stat;
    bool valid = true;
    if( stat(file_name,&file_stat) < 0 ) {
        valid = false;
    }
    else{
        if( S_ISDIR(file_stat.st_mode ) ) { // 判断一个路径是不是目录
            valid = false;
        }
        else if( file_stat.st_mode & S_IROTH ){ // 拥有读权限
            int fd = open( file_name,O_RDONLY );
            file_buf = (char*)malloc( file_stat.st_size+1 );
            memset( file_buf,'\0', file_stat.st_size+1 );
            if( read(fd,file_buf,file_stat.st_size) < 0 )
                valid = false;
        }else{
            valid = false;
        }

        int len = 0, ret;
        char header_buf[ BUFFER_SIZE];
        if( valid ){
            memset( header_buf,'\0',BUFFER_SIZE );
            ret = snprintf( header_buf,BUFFER_SIZE-1,"%s %s\r\n","HTTP/1.1",status_line[0] );
            len += ret;
            ret = snprintf( header_buf+len,BUFFER_SIZE-1-len,"Content-Length: %d\r\n",file_stat.st_size );
            len += ret;
            ret = snprintf( header_buf+len,BUFFER_SIZE-1-len,"%s","\r\n" );

            struct iovec iv[2];
            iv[0].iov_base = header_buf;  // 头部内容
            iv[0].iov_len = strlen( header_buf );
            iv[1].iov_base = file_buf;  // 实际文件内容
            iv[1].iov_len = file_stat.st_size;
            ret = writev( fd,iv,2 );
        }else{
            ret = snprintf( header_buf,BUFFER_SIZE-1,"%s %s\r\n","HTTP/1.1",status_line[1] );
            len += ret;
            ret = snprintf( header_buf+len,BUFFER_SIZE-1-len,"%s","\r\n" );
            send( fd,header_buf,strlen( header_buf ),0 );
        }
        close( fd );
        free( file_buf );
    }
    close( sock );
    return 0;
}

void ERROR(char* message){
    printf("%s\n",message );
    exit( 1 );
}
```

使用```telnet```或浏览器都可以访问该服务器.



### sendfile函数

```sendfile```函数在两个文件描述符间直接传输数据(完全在内核中操作),从而避免内核缓冲区和用户缓冲区间的数据拷贝,效率很高,称为零拷贝

```sendfile```定义如下

```c
#include <sys/sendfile.h>
ssize_t sendfile(int out_fd,int in_fd,off_t* offset,size_t count);
```

```in_fd```是待读出内容的文件描述符, ```out_fd```是待写入内容的文件描述符

```offset```指定从读入文件流的哪个位置开始读, 如果为空, 则使用读入文件流默认的起始位置

```count```参数指定在两个文件描述符传递的字节数

值得注意的是```in_fd```必须是一个支持类似```mmap```函数的文件描述符, 即它必须指向真实的文件而不是```socket```和管道

而```out_fd```必须是一个```socket```

由此可见, ```sendfile```几乎是专门为在网络上传输文件设计的

下面利用```sendfile```函数将服务器上的一个文件传送给客户端

```6.3-sendfile.c```

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

int main( int argc, char* argv[] )
{
    if( argc <= 3 )
    {
        printf( "usage: %s ip_address port_number filename\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );
    const char* file_name = argv[3];

    int filefd = open( file_name, O_RDONLY );
    assert( filefd > 0 );
    struct stat stat_buf;
    fstat( filefd, &stat_buf );

    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );

    int ret = bind( sock, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( sock, 5 );
    assert( ret != -1 );

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof( client );
    int connfd = accept( sock, ( struct sockaddr* )&client, &client_addrlength );
    if ( connfd < 0 )
    {
        printf( "errno is: %d\n", errno );
    }
    else
    {
        sendfile( connfd, filefd, NULL, stat_buf.st_size );
        close( connfd );
    }

    close( sock );
    return 0;
}
```



### mmap函数和munmap函数

```mmap```函数用于申请一段内存空间, 可以作为进程间通信的共享内存, 

```c
#include <sys/mman.h>
void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);
int munmap(void *start,size_t length);
```

```start```参数允许用户使用某个特定的地址作为这段内存的起始地址.如果被设置为```NULL```, 由系统分配一个地址

```length```参数指定内存段的长度

```prot```参数用于设置内存段的访问权限, 它可以是以下几个值的按位或

- ```PROT_READ```: 内存段可读
- ```PROT_WRITE```: 内存段可写
- ```PROT_EXEC```: 内存段可执行
- ```PROT_NONE```: 内存段不能被访问

```flags```参数控制内存段内容被修改后程序的行为(具体取值不在此列举)

```fd```参数是被映射文件对应的文件描述符, 一般通过```open```获得

```offset```参数设置从文件的何处开始映射

```mmap```成功时返回目标内存区域的指针



### splice函数

```splice```函数用于在两个文件描述符间移动数据, 也是零拷贝操作

和```sendfile```最大的不同是```sendfile```是为发送文件而生的, ```splice```的运用更为广泛

它的定义如下

```c
#include <fcntl.h>  
ssize_t splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);  
```

```fd_in```是待输入数据的文件描述符. 如果```fd_in```是一个管道文件描述符, 那么```off_in```必须被设置为```NULL```

如果```fd_in```不是管道文件描述符, 那么```off_in```表示从输入数据流的何处开始读取数据。此时如果```off_in```设置为```NULL```,表示从输入数据流的当前偏移位置读入

```fd_out/off_out```和```fd_in/off_in```意义相同

```len```参数指定移动数据的长度, ```flags```用于控制数据如何移动(设置为某些值的按位和,不在此列举)

使用```splice```函数时, ```fd_in```和```fd_out```必须至少有一个是管道文件描述符, 成功时返回移动的字节数量.返回```0```表示没有数据需要移动

这发生在从管道中读取数据而该管道没有被写入任何数据时.

```splice```函数失败返回```-1```并设置```errno```

下面使用```splice```函数实现一个零拷贝的回声服务器(把客户端的数据原样返回)

```6.4-splice.c```

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );

    int ret = bind( sock, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( sock, 5 );
    assert( ret != -1 );

    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof( client );
    int connfd = accept( sock, ( struct sockaddr* )&client, &client_addrlength );
    if ( connfd < 0 )
    {
        printf( "errno is: %d\n", errno );
    }
    else
    {
        int pipefd[2];
        assert( ret != -1 );
        ret = pipe( pipefd );
        ret = splice( connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE ); 
        assert( ret != -1 );
        ret = splice( pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
        assert( ret != -1 );
        close( connfd );
    }

    close( sock );
    return 0;
}
```



### tee函数

```tee```函数在两个管道文件描述符间复制数据, 也是零拷贝操作, 但它**不消耗**数据, 源文件描述符上的数据仍然可用于后续读操作

定义如下

```c
#include <fcntl.h>
ssize_t tee(int fd_in,int fd_out,size_t len,unsigned int flags);
```

参数意义与```splice```一致(但```fd_in```和```fd_out```必须都是管道文件描述符)

```tee```在成功时返回在两个文件描述符间复制的数据量(字节数)

```6.5-tee.cpp```

下列程序读取用户输入的文件, 同时输出数据到终端和文件

```cpp
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

int main( int argc, char* argv[] )
{
    if ( argc != 2 )
    {
        printf( "usage: %s <file>\n", argv[0] );
        return 1;
    }
    int filefd = open( argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666 );
    assert( filefd > 0 );

    int pipefd_stdout[2];
    int ret = pipe( pipefd_stdout );
    assert( ret != -1 );

    int pipefd_file[2];
    ret = pipe( pipefd_file );
    assert( ret != -1 );

    //close( STDIN_FILENO );
    // dup2( pipefd_stdout[1], STDIN_FILENO );
    //write( pipefd_stdout[1], "abc\n", 4 );
    ret = splice( STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
    assert( ret != -1 );
    ret = tee( pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK );
    assert( ret != -1 );
    ret = splice( pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
    assert( ret != -1 );
    ret = splice( pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
    assert( ret != -1 );

    close( filefd );
    close( pipefd_stdout[0] );
    close( pipefd_stdout[1] );
    close( pipefd_file[0] );
    close( pipefd_file[1] );
    return 0;
}
```

流程大概分为四步

- 使用```splice```把标准输入的输入值转移到```pipefd_stdout```管道中
- 使用```tee```把```pipefd_stdout```管道的值复制一份到```pipefd_file```管道中
- 使用```splice```把```pipefd_stdout```转移到标准输出
- 使用```splice```把```pipefd_file```转移到文件中



### fcntl函数

```fcntl```(```file control```)提供对文件描述符的各种控制操作

定义如下

```cpp
#include <fcntl.h>
int fcntl(int fd,int cmd,...)
```

```fd```是被操作的文件描述符, ```cmd```参数指定执行何种类型的操作

在网络编程中, ```fcntl```通常用于将一个文件描述符设置为非阻塞的,如下所示

```cpp
int setnonblocking(int fd)
{
    int old_option = fcntl( fd,F_GETFL );  // 获取文件描述符旧的状态
    int new_option = old_option | O_NONBLOCKL; // 设置为非阻塞
    fcntl( fd,F_SETFL,new_option ); // F_SETFL表示设置fd的状态标志
    return old_option;
}
```

