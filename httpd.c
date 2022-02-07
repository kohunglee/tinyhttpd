/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

void accept_request(void *); // 处理链接，子线程
void bad_request(int);  // 400 错误
void cat(int, FILE *);  // 处理文件，读取文件内容，并发送到客户端
void cannot_execute(int);  // 500 错误处理函数
void error_die(const char *);  // 错误处理函数处理
void execute_cgi(int, const char *, const char *, const char *);  // 调用 CGI
int get_line(int, char *, int);  // 从缓存区读取一行
void headers(int, const char *);  // 服务器成功响应，返回200
void not_found(int);  // 请求的内容不存在 404
void serve_file(int, const char *);  // 处理文件请求
int startup(u_short *);  // 初始化服务器
void unimplemented(int);  // 501 仅实现了 get post 方法，其他方法处理函数

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
// 处理链接，子线程
void accept_request(void *arg)
{
    int client = (intptr_t)arg;  // 建立 socket 描述符
    char buf[1024];  // 缓冲区
    size_t numchars;
    char method[255];
    char url[255];  // url
    char path[512];  // 路径的字符数组
    size_t i, j;
    struct stat st;  // 文件状态信息，下面检查文件是否存在时会用到
    int cgi = 0; // 是否调用 cgi 程序
    /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

    /* 添加 */
    pthread_detach(pthread_self());  // 子线程分离，在这个线程结束后，
                                     // 不需要其他的线程对他进行收尸

    // 开始对服务器进行读 第一行
    // get_line 就是解析 http 协议
    // http 协议第一行，请求方法、空格符、url、空格符、协议版本,这是第一行
    numchars = get_line(client, buf, sizeof(buf));
    i = 0; j = 0;
    // 这个循环就是在找空格符，判断第 i 个字符是不是空格
    // 并且，没有超过 method 缓冲的大小
    // 至于减去一，是因为要在最后面加一个 ’\0‘ ,作为标识符
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i]; // 不是空格，就复制到 method 里面
        i++;
    }
    j=i;
    method[i] = '\0';

    // 测试，打印方法
    printf("test:print the method-----%s\n", method);

    // 仅实现了 GET 和 PUT 方法，别的方法还没有实现
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        // 如果不是那两个方法，则调用 501 的错误处理函数
        unimplemented(client);
        return;
    }

    // 如果是 post 方法，将 cgi 设为1（下面会调用 cgi 来处理这些） 
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    // 下面该处理 url 了
    i = 0;
    while (ISspace(buf[j]) && (j < numchars))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        // 如果不是空格的话，继续向 url 里进行复制，跟上面那个 method 方法一样
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';  // 读完后依然向最后加一个这个，以标识这是一个字符串

    // 如果是 GET 方法
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        // GET 方法，往往在 url 后面有 ？ 
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        // 逐个字符寻找 ’？‘  ,如果找到问号了，说明就是 get 提交的数据
        // 那么就需要 cgi 来处理数据，将 cgi 设置成 1
        // 并将 query_string 指向 ’？‘ 后的内容
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    // 如果 url 是一个目录
    // 那么就和 ’htdocs‘ 拼接，（也就是根目录）
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')  // 如果最后一个字符是 ’/‘
        strcat(path, "index.html");  // 返回这个目录下的 html 文件，保证是个文件，而不是目录

    if (stat(path, &st) == -1) {  // 检查拼接后的文件是否存在， -1 就是代表不存在
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */ // 读取，并丢弃 headers 
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);  // 不存在，就返回 404_not_found
    }
    else  // 如果文件存在
    {
        if ((st.st_mode & S_IFMT) == S_IFDIR)  // 如果这个文件是一个目录的话
            strcat(path, "/index.html");  // 向下拼接 index.html （其实这两行不需要也行，上面已经拼接过了） 
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )  // 检查权限，如果可执行的话，则 cgi = 1
            cgi = 1;

        if (!cgi) // 做最终判断
            serve_file(client, path);  // cgi 等于 0 ，不需要调用 cgi ，相当于请求了个页面
        else
            execute_cgi(client, path, method, query_string);  // 调用 cgi ,执行 cgi 程序
            // client:描述符、path:路径、method:请求的方法、query_string：判断是否有问号，以便使用 get 请求发送数据
    }

    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
// 400 错误
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
// 处理文件，读取文件内容，并发送到客户端
void cat(int client, FILE *resource)
{
    char buf[1024];  // 首先一个缓存区
    // 逐行读取，遇到换行符 eof 就停止
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))  // 是否已经读到了文件结尾，以确保读完整个文件
    {
        send(client, buf, strlen(buf), 0);  // 到结尾后，send 发送到客户端
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
// 500 错误处理函数
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
// 错误处理函数处理
void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
// 调用 CGI
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)  // 判断是不是 get 方法 ，如果是，则丢弃头部信息
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));  // 比较前 15 个字符，如果等于 Content-Length: ，则转化为 int
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {  // 如果不等于
            bad_request(client);  // 错误的处理
            return;
        }
    }
    else/*HEAD or other*/
    {
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 上面成功执行，则向服务器发送成功的响应头部
    send(client, buf, strlen(buf), 0);

    // 初始化管道 
    /*
        管道是为了在子线程里面的 cgi 和服务器调用 cgi 程序进程间通信用

        要创建两个管道
        1. 子线程向服务器端写的一个管道
        2. 子线程向服务器端读的一个管道
    */
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ( (pid = fork()) < 0 ) {  // 管道创建成功后，创建子线程
        cannot_execute(client);  // 错误就进行错误处理 
        return;
    }

    if (pid == 0)  /* child: CGI script */ // 判断是否是子进程，进而进行处理
    {   // 子进程
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], STDOUT);  // 0 文件（ STDOUT ）描述符重定向到管道读端
        dup2(cgi_input[0], STDIN);  // 1 文件 （ STDIN ）描述符重定向到管道写端
        close(cgi_output[0]);  // 关闭不必要的读写端
        close(cgi_input[1]);  // 子进程只需要从某一端读或某一端写，另一端是不需要的
        // 之后通过 cgi 写内容的话，是直接写到那个管道里而不是写到终端（或显示器）上
        sprintf(meth_env, "REQUEST_METHOD=%s", method);  // 把 REQUEST_METHOD 写到环境变量里面（ meth_env 这个变量可以写环境变量），是一种进行进程间通信的方式
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            // 如果是 get 方式，则向环境变量中写 QUERY_STRING ，以便让 cgi 程序知道 query_string
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            // 如果是 POST 方式。告诉 cgi 程序需要读多长的数据
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, NULL);  // 处理完后，系统调用
        exit(0);
    } else {    /* parent */
        // 父进程
        close(cgi_output[1]);  // 先关闭不需要的管道（ 读写端 ）
        close(cgi_input[0]);  // 父进程只需要管道的一端
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                // 逐个字符读，然后写到管道里面
                recv(client, &c, 1, 0);
                fprintf(stderr,"%c\n",c); // 测试
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);  // 读一个字符，发送一个字符

        close(cgi_output[0]);  // 两个管道关闭
        close(cgi_input[1]);
        waitpid(pid, &status, 0);  // 等待子进程结束
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
// 从缓存区读取一行
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");  // 首http 的协议、状态码、ok
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");  // 发送类型，html
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");  // 结束后，空行
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
// client 是建立连接的 socket 标识符
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */ // 读取 http 头部信息
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");  // 打开发送到客户端的文件，以 只读 的方式打开
    if (resource == NULL)  // 错误处理函数
        not_found(client);
    else
    {
        headers(client, filename);  // 如果成功，向客户端发送 200 的请求正确的头
        cat(client, resource);  // 将文件内容，逐行的发送到客户端
    }
    fclose(resource);  // 关闭文件
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;  // 定义服务器的 Socket 描述符
    int on = 1;  // ?
    struct sockaddr_in name;  // 用那个结构体,绑定服务器端的 ip 地址

    httpd = socket(PF_INET, SOCK_STREAM, 0); // 创建服务器端的 socket
    // ip V4 、 SOCK_STREAM 建立安全 TCP 流的类型、0 是这个流默认的协议

    if (httpd == -1) // 做判断，如果是 -1 就是出错了
        error_die("socket");  // 出错就打印错误信息，并退出整个程序

    // 接下来就是绑定服务器端的地址和端口  
    memset(&name, 0, sizeof(name)); // 把结构体初始化为 0
    // 下面是结构体的三个成员
    name.sin_family = AF_INET;  // 指定地址类型 ipv4
    name.sin_port = htons(*port);  // 传进来的端口，转化为网络字序节（就是大端储存的字节序）
    name.sin_addr.s_addr = htonl(INADDR_ANY);  // INADDR_ANY 本机任意可用的 ip 地址
    // ?
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
    // 绑定到地址上
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");  // 如果小于 0 ，就返回 绑定失败

    // 如果绑定的端口小于 0 ,则自动随机生成可用端口
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        // 获取已经绑定后的套字节信息，主要是获取随机生成的端口号是多少
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);  // 转化成 ntohs 类型，就是把网络字节序转化为本地的字节序
    }
    if (listen(httpd, 5) < 0)  // 这时开始监听
        error_die("listen");
    return(httpd);  // 把生成的 socket 描述符传递回 main 函数,也就是 main 函数中的server_sock
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;  // 定义服务器 socket 的描述符
    u_short port = 4000;  // 定义服务端监听端口
    int client_sock = -1;  // 定义客户端 socket 的描述符
    struct sockaddr_in client_name;  // 定义一个结构体，sockaddr_in 型
    socklen_t  client_name_len = sizeof(client_name);  // 获取客户端地址长度
    pthread_t newthread;  // 定义线程的 id

    server_sock = startup(&port);  // 初始化服务器
    printf("httpd running on port %d\n", port);  // 在控制台打印出端口号

    // 循环创建链接和子线程（就是提供服务，等待与客户端建立链接）
    while (1)
    {
        // 如果有客户过来，就从 listen() 里建立的队列里取一个建立连接的
        // 的链接，然后生成新的 socket 描述符。&client_name 就是客户端
        // 的地址信息
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);  // 阻塞等待客户端建立链接
        if (client_sock == -1)  // 如果函数出错的话，还是得错误处理
            error_die("accept");
        printf("%d\n",ntohs(client_name.sin_port)); // 测试打印客户端端口
        /* accept_request(&client_sock); */
        // 创建子线程处理链接
        // 如果建立连接成功的话，还是创建一个子线程，处理服务器端与客户端的通信
        // &newthread 就是线程 ID 、NULL 是默认属性、accept_request就是子线程要执行的函数
        // 然后把 client_sock 强制转化成 void*
         if (pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
            perror("pthread_create");  // 如果线程创建失败，执行错误处理
    }

    close(server_sock);

    return(0);
}
