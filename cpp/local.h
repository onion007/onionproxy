#include <iostream>
#include <list>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

#define DEBUG
#define QUEUE       20
#define MAXSIZE     4096
#define SOCKSVER5   5
#define RSERVERIP   "127.0.0.1"
#define RSERVERPORT 8388

#ifndef _LOCAL_STRUCT_
#define _LOCAL_STRUCT_
struct sockaddr_in init_sockaddr_in(string h, int p)
{
    struct sockaddr_in saddr;
    saddr.sin_family      = AF_INET;
    saddr.sin_port    = htons(p);
    saddr.sin_addr.s_addr = inet_addr(&h[0]);
    return saddr;
}
#endif
