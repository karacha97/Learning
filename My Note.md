# 实现一个多用户并发的IO复用/分离的服务器
仓促的读完了《TCP/IP网络编程》这本书，大致对于Linux下的网络编程有了一点点的了解，虽然之前大四时候学过计算机网络，做小项目时也用过socket的编程，但是时间太久远了，（而且当时纯粹是为例实现简单的传输功能
并没有详细的去追究原理性的东西）

现在准备做一些总结，从最开始用socket实现的简单echoserver开始，再到为了让多用户接入而引入的多进程、以及为了方便编程而做的IO分离、乃至最后使用的epoll和多线程。
之前的技术为何有缺陷，之后的又是如何弥补的，还存在的缺陷在哪里。我觉得需要再理一下。

## 一些概念的辨析
1. 并发和并行

2. IO分离和复用

3. 进程和线程


## 1. 一个简单的尝试：基于TCP/IP的服务器端
 构建网络的基础：套接字

套接字用来链接两台计算机，是由操作系统来提供的，利用套接字来通信需要以下几个部件：
- socket
- bind
- listen
- accept
- connect
- close
***
### 1.1 socket()
想要用套接字的话第一步肯定要创建相应的套接字啦，创建套接字的函数原型如下：
```
#include<sys/socket.h>
int socket(int domain,int type, int protocol);
```
仔细一看好怪哦，为何会返回一个`int`型变量呢？在Linux中，所有东西都会被看作是文件，因此套接字也不例外，创建好的套接字也会被看作一个文件，用文件描述符来表示，在Linux中，文件描述符用整型来表示。

^文件描述符是系统分配给文件或者是套接字的整数，相当于对文件做了一个编号，通过此编号可以访问文件。在Linux中是用文件描述符来操作，在Windows中是用句柄^

```
domain:套接字使用的协议簇 e.g. PF_INET:IPV4协议簇
type: 套接字数据传输类型
- SOCK_STREAM：面向链接的套接字（TCP)
- SOCK_DGRAM:面向消息的套接字（UDP）
protocol:计算机间通信使用的协议信息（同一协议族中国存在这多个数据传输方式相同的协议，
虽然IPV4中不是这样，但是其他协议族可能有，因此需要第三个参数）
- IPPROTO_TCP:TCP协议
- IPPROTO_UDP:UDP协议
```
使用方式示例：
```
int serv_sock;
int clnt_sock;
serv_sock=socket(PF_INET,SOCK_STREAM,0)//参数3可省略
```
***
### 1.2 bind()
bind函数的作用是给套接字分配地址，相当于把套接字绑定在一个地址上，如同其名字一样。
```
int bind( int sockfd, struct sockaddr *myaddr,socklen_t addrlen);
```
- sockfd:要分配地址信息的套接字的文件描述符
- myaddr 存有地址信息的结构体遍量的地址
- addrlen 结构体变量的长度

#### 1.2.1 `sockaddr_in`结构体
明明bind里面需要的地址信息是`sockaddr`结构体欸，为什么要那么费劲用`sockaddr_in`结构体呢

这里需要看一下`sockaddr`结构体和`sockaddr_in`结构体的内容！
```
struct sockaddr
{
	sa_family_t sin_family;//地址族
	char sa_data[14];
}
```
可以发现结构体sockaddr并非专为IPV4设计，从保存地址和端口信息的sa_data长度可以看出来（14字节，IPV4地址只要4字节，加上端口号（16位）也才6字节）。

如果直接初始化sockaddr的话，后面的位需要补零很麻烦。因此使用`sockaddr_in`结构体，其组成如下：

```
struct sockaddr_in{
	sa_family_t sin_family;
	uint15_t sin_port;//端口号
	struct in_addr;//32位（4字节）IPV4地址
	char sin_zero[8]；//不用的
}
struct in_addr{
	in_addr_t s_addr;
}//in_addr_t 的类型是uint32_t，在POSOIX中定义
```
接下来可以给一个地址信息赋值了

1.首先赋地址族：

|地址族|含义|
|---|---|
|AF_INET|IPV4中使用的地址族|
|AF_INET5|IPV6中使用的地址族|

初始化过程比较固定；
```
struct sockaddr_in addr;
char* serv_ip="211.217.168.13";
char* serv_port="9190";
memset(&addr,0,sizeof(addr));//addr所有成员初始化位0
addr.sin_addr.saddr=inet_addr(serv_ip);
addr.sin_port=htons(atoi(serv_port));
```
### 1.3 TCP的原理

在分析`listen()`和`accept（）`以及`connect`之前，还需要了解一点TCP建立的方式；
简单来说就是通信之前双方需要建立链接，通信结束后再断开链接；

建立链接之前，服务器一方需要先调用`listen()`函数来监听是否有用户端想要和自己链接；

```
int listen(int sock,int backlog)
```
- sock：之前绑定好地址的套接字的文件描述符
- backlog:请求队列的长度，也就是说服务器对于想要连入的用户套接字有个缓存，虽然一次只能进入一个，但是可以让剩下的先等待。
- 成功时返回0否则返回-1；

调用完`listen`后，可以调用`accept()`函数来接入了，但是这时需要注意的是，此时需要新建一个套接字来和客户端链接；
这样做的原因是因为之前的套接字只负责监听；
```
int accept(int sock, struct sockaddr* addr,socklen_t* addrlen);
```
accept函数会返回链接着客户端的套接字

#### 1.3.1 客户端操作
客户端的实现顺序会比服务器端要简单的多；
1. 建立socket
2. connect()
3. close()

```
int connect(int sock,struct sockaddr* servaddr,socklen_t addrlen);
```
`servaddr`:目标服务器地址信息

这时还有一个小问题，服务器端会将地址通过`bind()`绑定给`socket`，但是客户端并未有绑定操作，何时给客户端分配地址呢？

答案是操作系统会在`connect`时自动分配，不需要`bind()`

#### 1.3.2 TCP套接字的IO缓冲

套接字实际上也是依靠缓冲区来存数据的
- 用`write（）`时，会直接向输出缓冲区放数据
- 调用`read（）`会从输入缓冲区读数据

#### 1.3.3 Time-wait状态_
何为Time-wait状态？
- 先断开的套接字在四次握手后不会立刻消除，而是需要再额外等一段时间；

为何会有这种设计？
- 为了保证后断开的主机能收到从先断开的主机传回的ACK（如果中间丢包了，可以重发，这样保证后断开的主机也能关掉链接）

这样做的缺陷是什么？
- 若是想要断开后立刻使用该端口，则会报错，因为端口还在被占用！

如何避免呢？
- 再套接字可选项中选择SO_REUSEADDR状态
## 2.多进程服务器端

能同时向所有客户端提供服务的并发服务器端有以下几种模型：

- 多进程服务器：通过创建多个进程来提供服务
- 多路复用服务器：通过捆绑并统一管理IO对象来提供服务
- 多线程服务器：类似多进程服务器

### 2.1 进程简介

进程是什么呢？如果了解冯诺依曼的计算机结构可以知道，程序只是存储再计算机中的指令按步骤执行，因此正在运行的进程是一块内存空间中的代码段以及数据；
在Linux中，进程可以依靠ID来区分。

创建进程的方法有很多，最简单的是`fork()`指令：
```
pid_t fork(void)
```
`fork（）`指令成功时返回进程的ID，失败时返回-1；
- 对于父进程而言，会返回子进程的id
- 而子进程则会得到0；
```
值得注意的是：
- fork()会创建调用他的进程的副本，也就是说可能会发生递归调用，要小心。
- 父进程和子进程只是拥有同样的代码和和数据，但是其内存空间是完全独立的，因此不共享数据
- 进程间共享数据需要管道。
```
#### 2.1.1 如何结束进程
进程结束后应该被销毁，但是由于种种原因，进程可能会未被销毁，变成所谓的僵尸进程。

首先来看一下子进程的终止方式（注意是终止，而非销毁！）
- 传递参数并且调用exit
- 从main函数中执行return

向exit函数传递的参数和return返回的值都会传递给操作系统，但是OS并不会销毁子进程，而是直到这些值传递给产生该子进程的父进程！
```
所谓的僵尸进程就是处在这两个状态下的进程：子进程以及终止，但是父进程还会收取返回值
```

销毁子进程的方法：
- wait函数
```
pid_t wait(int* statloc);
```
成功时返回子进程的id

子进程返回的值保存至 statloc指向的内存空间

但是wait会阻塞住父进程。

- waitpid函数

与wait类似，但是不会阻塞，不过也是需要一直等待

- 通过信号处理

解决上述问题的方式是利用信号，（这里信号是特定事件发生时，操作系统向进程发送的消息）实际上是交给操作系统来处理这些事情了。

我们想要的：

进程希望操作系统在发现子进程调用结束时，调用特定的函数来接收子进程的返回值。 
可以通过两种函数处理，一般多用后者`sigaction()`；
```
void (*signal(int signo,void(*func)(int)))(int);
int sigaction(int signo,const struct sigaction* act,struct sigaction* oldact);
```

### 2.2基于多任务的并发服务器
好啦！现在我们可以用多进程来实现多用户的并发服务器了！

基本思想是这样的：
- 父进程创建serv_sock用来监听有无用户接入
- 一旦接入一个就fork一个子进程，复制了文件描述符
- 子进程通过复制的文件描述符对于客户端提供服务

值得注意的时，fork复制并不会复制一个一样的套接字，因为套接字是属于操作系统的，fork仅仅会复制文件描述符，因此实际上会有多个文件描述符指向同一个套接字；
我们需要适当的断开部分的文件描述符和套接字的关系（close）

^销毁文件描述符不会导致套接字也关闭吗？不会！只有将套接字对应的所有的文件描述符都销毁时，才会关闭套接字^

```
while(1){
	clnt_sock=accept(serv_sock,(struct sockaddr*)&clnt_adr,&adr_sz);
	···
	pid=fork();
	···
	if(pid==0){
		close(serv_sock);
			·····//读写
		close(clnt_sock);
	}else{
		close(clnt_sock);
	}
}
```


### 2.3 用多进程来实现IO分离
我们想要在客户端中实现IO分离。

为何要实现IO分离呢？
- 可以简化数据收发逻辑需要的细节，在父进程只考虑接受数据，在子进程只考虑发送数据
- 可以提高频繁交换数据的程序性能

部分代码如下(注意是客户端，服务器端还有要考虑的问题)
```
if(connect(sock,(struct sockaddr*) &serv_adr,sizeof(serv_adr))==-1)
	error_handling("connection error");
pid=fork();
if(pid==0){
	write_routine(sock,buf)；//子进程
}else{
	read_routine(sock,buf);
}

//--------------------------------------
void read_routine(int sock, char* buf){
	while(1){
		int str_len=read(sock,buf,BUF_SIZE);
		if(str_len==0)
			return;
		buf[str_len]=0;
		print("message from serv: %s",buf);
	}
}

void write_routine(int sock, char* buf){
	while(1){
		fget(buf,BUF_SIZE,stdin);
		if(!strcmp(buf,"q\n")||!strcmp("Q\n")){
			shutdown(sock,SHUT_WR);
			return;
		}
		write(sock,buf,strlen(buf));
	}
}
```
为何需要半关闭？fork复制的文件描述符，需要关闭2次？

为何不直接关闭呢？我暂时没想明白

### 2.4 进程间通信

^可以看出客户端完全可以利用IO分离，因为我们的发送端对于接收端的数据没有依赖，但是服务器端就不是这样了，我们的发送端需要接收端的数据啊！
因此需要进程间通信！^

为了完成进程间的通信，需要利用管道`pipe()`；管道和socket一样，不是属于进程而是操作系统的资源。因此也不是fork复制的对象！

```
int pipe(int filedes[2]);
```
- filedes[0]:管道入口，即接收数据的文件描述符
- filedes[1]:管道出口，传输数据时的文件描述符

父进程调用`pipe()`产生收发文件的管道的入口出口；这之后再调用fork，子进程也会复制文件描述符。
```
int fds[2];
...
pipe(fdes);
int pid=fork;
if(pid==0){
	write(fds[1],str,sizeof(str));
}else{
	read(fds[0],buf,BUF_SIZE);
	puts(buf);
}
```
上述通信方法中，父进程只用了输出路径，子进程只用了输入路径；那是否可以实现双向通信呢？

答案是可以，但是需要用两个管道，因为用一个管道的话，先读的进程会把管道中的数据读走；

```
int fds1[2];
int fds2[2];
...
pipe(fdes);
int pid=fork;
if(pid==0){
	write(fds1[1],str,sizeof(str));
	read(fds2[0],buf,BUF_SIZE);
}else{
	read(fds1[0],buf,BUF_SIZE);
	write(fds2[1],str,sizeof(str));
	puts(buf);
}
```

利用进程间通信实现文件存储，多用户文件存档

```
int main(int argc, char *argv[])
{
	......
	
	pipe(fds);
	pid=fork();//创建一个子进程来存文件
	if(pid==0)
	{
		FILE * fp=fopen("echomsg.txt", "wt");
		char msgbuf[BUF_SIZE];
		int i, len;

		for(i=0; i<10; i++)
		{
			len=read(fds[0], msgbuf, BUF_SIZE);
			fwrite((void*)msgbuf, 1, len, fp);
		}
		fclose(fp);
		return 0;//子进程结束，会调用函数收回空间
	}

	while(1)
	{
		adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
		if(clnt_sock==-1)
			continue;//没人接入的话只在上面循环，不用创建新的进程
		else
			puts("new client connected...");

		pid=fork();//创建子进程来实现多用户
		if(pid==0)
		{
			close(serv_sock);
			while((str_len=read(clnt_sock, buf, BUF_SIZE))!=0)
			{
				write(clnt_sock, buf, str_len);
				write(fds[1], buf, str_len);//进程间通信。
			}
			
			close(clnt_sock);
			puts("client disconnected...");
			return 0;
		}
		else
			close(clnt_sock);
	}
	close(serv_sock);
	return 0;
}
```
如果我想要像之前可客户端一样做IO分离呢
```
int fds[2];
...
pipe(fdes);
int pid=fork();
if(pid==0){
	read(fds[0],buf,BUF_SIZE);
	write_routine(sock,buf);//子进程
}else{
	
	read_routine(sock,buf);
	wite(fds[1],buf,sizeof(buf));
}

//--------------------------------------
void read_routine(int sock, char* buf){
	while(1){
		int str_len=read(sock,buf,BUF_SIZE);
		if(str_len==0)
			return;
		buf[str_len]=0;
		print("message from serv: %s",buf);
	}
}

void write_routine(int sock, char* buf){
	while(1){
	
		if(!strcmp(buf,"q\n")||!strcmp("Q\n")){
			shutdown(sock,SHUT_WR);
			return;
		}
		write(sock,buf,strlen(buf));
	}
}
```
这样一想问题来了，我要是想要多用户并发，且IO分离呢？需要在子进程内嵌套子进程来做IO分离啊；这样看上去会占用很多资源。

## 3.IO复用的服务端
如何不用进程来实现多用户并发呢？
IO复用是个解决方案！

复用这个思想在通信领域中很常见了，最常见的时分复用，频分复用……

比如我们一个进程可以操作多个套接字
^实际上服务器端的复用是根据套接字是否收到数据来选择套接字，如果有收到就操作该套接字^

### 3.1 select函数
select函数的功能：将多个文件描述符集中到一起监视；

- 是否存在套接字接收数据
- 无需阻塞传输数据的套接字有哪些？
- 哪些出现了异常？

调用顺序：
1. 设置文件描述符

监视的结构是`fd_set`
```
//可以通过宏来设置fdset
FD_ZERO(fd_set* fdset)
FD_SET(int fd, fd_set* fdset);
FD_CLR(int fd, fd_set* fdset);
...
```
2. 设置检查范围和超时
```
int select(int maxfd, fd_set* readset,fd_set* writeset,fd_set* exceptset, const struct timeval* timeout);
```
- maxfd:监视对象文件描述符的数量
- readfd:需要读数据的文件描述符
- writefd:需要写数据的文件描述符
- exceptfd:需要检测是否发现异常的文件描述符
- timeout:超时检测

maxfd希望得到最大有多少个文件描述符，但实际上一般文件描述符会递加，只要把最大的文件描述符加一即可。

关于超时时间，因为select会在监视的文件描述符发生变化后返回，若是不发生变化则会一直阻塞在那里，所以需要设定超时时间，如果超过该时间，也会返回；
要是不想的话可以传递nullptr;

### 3.2 基于select的IO复用服务器
```
FD_ZERO(&reads);
FD_SET(serv_sock, &reads);
fd_max=serv_sock;

while(1)
{
	cpy_reads=reads;
	timeout.tv_sec=5;
	timeout.tv_usec=5000;

	if((fd_num=select(fd_max+1, &cpy_reads, 0, 0, &timeout))==-1)
		break;
		
	if(fd_num==0)
		continue;

	for(i=0; i<fd_max+1; i++)
	{
		if(FD_ISSET(i, &cpy_reads))
		{
			if(i==serv_sock)     // connection request!
			{
				adr_sz=sizeof(clnt_adr);
				clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
				FD_SET(clnt_sock, &reads);
				if(fd_max<clnt_sock)
					fd_max=clnt_sock;
				printf("connected client: %d \n", clnt_sock);
			}
			else    // read message!
			{
				str_len=read(i, buf, BUF_SIZE);
				if(str_len==0)    // close request!
				{
					FD_CLR(i, &reads);
					close(i);
					printf("closed client: %d \n", i);
				}
				else
				{
					write(i, buf, str_len);    // echo!
				}
			}
		}
	}
}
	close(serv_sock);
	return 0;
}
```



## 4. 用标准IO实现IO分离
### 4.1 标准IO

标准IO的优点：
- 具有良好的移植性
- 可以利用缓存提高性能（标准IO都自带有缓存，算上套接字的缓存，会有两级缓存哦）

标准IO的缺点
- 不易进行双向通信。why？
- 需要频繁调用fflush
- 需要以FILE结构体指针返回文件描述符

#### 4.1.1 FILE结构体指针和文件描述符的相互转换
文件描述符转换为FILE结构体指针
```
FILE* fdopen(int fildes,const char* mode);

//e.g.
FILE *fp;
int fd=open("data.txt",O_WRONLY|O_CREAT);
fp=fdopen(fd,"w");
```
反之：
```
int fileno(FILE* stream);
```

#### 4.1.2 用标准IO实现分离IO流的问题，怎么半关闭
之前分离IO流的方法：
- 用fork函数创建一个进程专门来分离
- 用标准输入输出函数

流分离的方法不同，带来的有点也不同。
但是分离流会带来问题，比如用标准IO来分离流的情况，半关闭是个问题。

之前半关闭是通过`shutdown(sock,SHUT_WR)`来实现，但是对于标准IO实现的流，还不知道如何半关闭。
如果单纯调用fclose的话，会直接销毁套接字；这是因为何原因呢？

很简单的一点就是，我们将一个套接字对应的文件描述符分别以读的模式和写的模式转化成立FILE结构体指针，这两个指针实际上是一个文件描述符，也是对应一个套接字，
销毁其中一个会关闭对应的文件描述符，因为套接字对应的文件描述符只有一个，所以套接字也会关闭。

这样就明白了，只需要将文件描述符再复制以下就好了啊！

下面考虑的就是如何复制文件描述符了，第一个想到的肯定是之前的`fork()`，fork再复制进程时肯定会复制文件描述符的，但是很可惜，fork复制的内容是分别在两个进程下的
如果要想在同一个进程下复制文件描述符，那么需要用到`dup`&`dup2`
```
int dup(int fildes);
int dup2(int fildes, int fildes2);
```
二者都是成功后返回复制的文件描述符
- fildes即是需要复制的文件描述符
- dup2还会引入一个参数fildes2，这个是用户可以指定复制出来的文件描述符的值；
### 4.2 流的分离和半关闭
```
	readfp=fdopen(clnt_sock, "r");
	writefp=fdopen(dup(clnt_sock), "w");
	//可以看出此处复制了文件描述符
	fputs("FROM SERVER: Hi~ client? \n", writefp);
	fputs("I love all of the world \n", writefp);
	fputs("You are awesome! \n", writefp);
	fflush(writefp);
	
	shutdown(fileno(writefp), SHUT_WR);
	fclose(writefp);
	
	fgets(buf, sizeof(buf), readfp); fputs(buf, stdout); 
	fclose(readfp);
```
## 5. 优于`select`的`epoll`


## 6. 多线程来替代多进程

### 互斥锁和信号量的概念
