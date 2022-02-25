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






## 用多进程来实现IO分离

### 进程间通信

## 用select来实现IO复用


## 用标准IO实现IO分离

## 用epoll实现IO分离

## 多线程来替代多进程

### 互斥锁和信号量的概念
