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

struct sockaddr_in init_sockaddr_in(string h, int p)
{
    struct sockaddr_in saddr;
    saddr.sin_family      = AF_INET;
    saddr.sin_port    = htons(p);
    saddr.sin_addr.s_addr = inet_addr(&h[0]);
    return saddr;
}

class ShadowsocksConnect
{
    public:
        ShadowsocksConnect(int fd): sockfd(fd) {};
        ~ShadowsocksConnect()
        {
            if(sockfd > 0)
            {
                cout << "close sockfd=" << sockfd << endl;
                close(sockfd);
            }
        }
        ssize_t read(char *, size_t);
        ssize_t write(char *, size_t);
        char    buffer[MAXSIZE];
        void    settimeout(int timeout);
        void    Shutdown() { shutdown(sockfd, SHUT_WR); }
    private:
        int     sockfd;
};

void ShadowsocksConnect::settimeout(int timeout)
{
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
}

ssize_t ShadowsocksConnect::read(char * buffer, size_t len)
{
    return recv(sockfd, buffer, len, 0);
}

ssize_t ShadowsocksConnect::write(char * buffer, size_t len)
{
    return send(sockfd, buffer, len, 0);
}

class ShadowsocksPipe
{
    public:
        ~ShadowsocksPipe();
        ShadowsocksConnect  *request  = NULL;
        ShadowsocksConnect  *response = NULL;
        void Run() { run(); }
        void operator() ();
        int  status = 1;
    private:
        void run(void);
        int  handshake(void);
        int  getrequest(void);
        static void forward(ShadowsocksConnect *, ShadowsocksConnect *, string);
};

ShadowsocksPipe::~ShadowsocksPipe()
{
    if(request)
        delete request;
    if(response)
        delete response;
}

int ShadowsocksPipe::handshake(void)
{
    const int idVer     = 0;
    const int idNmethod = 1;
    char      buffer[263];

    size_t n = request->read(buffer, 263);
    if(n <= 0)
    {
        throw "handshake: can't read data!";
    }
    if(SOCKSVER5 != buffer[idVer])
    {
        throw "handshake: get not socks5 data!";
    }

    char wbuf[2] = {0x05, 0x00};
    if(2 != request->write(wbuf, 2))
    {
        throw "handshake: write error!";
    }

    return 0;
}

int ShadowsocksPipe::getrequest(void)
{
    const int idVer  = 0;
    const int idType = 3;
    char      buffer[263];

    ssize_t n = request->read(buffer, 263);
    if(SOCKSVER5 != buffer[idVer])
    {
        throw "getrequest: get not socks5 data!";
    }

    char wbuf[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x08, 0x43};
    request->write(wbuf, 10);

    n -= idType;
    memcpy(request->buffer, &buffer[3], n);

    return n;
}

void ShadowsocksPipe::forward(ShadowsocksConnect *src, ShadowsocksConnect *dst, string m)
{
    int n;
    for(;;)
    {
        n = src->read(src->buffer, MAXSIZE);
        if(n > 0) 
        {
            dst->write(src->buffer, n);
        }
        else if(n == 0)
        {
            break;
        }
    }
    dst->Shutdown();
    cout << m << " exit..." << endl;
}

void ShadowsocksPipe::run(void)
{
    try
    {
        request->settimeout(60 * 1000);
        /*
         * 1. socks5 handshake
         * 2. get request
         * 3. connect server, forward package
         */
        handshake();
        int n = getrequest();

        struct sockaddr_in saddr = init_sockaddr_in("127.0.0.1", 8388);
        int connfd = socket(AF_INET, SOCK_STREAM, 0);
        if(connfd == -1)
        {
            throw "create client socket error";
        }
        if(connect(connfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
        {
            throw "connect Shadowsocks server error";
        }
        response = new ShadowsocksConnect(connfd);
        response->write(request->buffer, n);

        thread t(forward, response, request, "res -> req");
        forward(request, response, "req -> res");
        t.join();
    }
    catch (const char* msg)
    {
        cerr << msg << endl;
    }
    // set status to 0, mains can be deleted
    this->status = 0;
}

void ShadowsocksPipe::operator() (void)
{
    cout << "this=" << this << " run" << endl;
    run();
}

class SocketBase
{
    public:
        SocketBase(string h, int p): host(h), port(p) {}
    protected:
        string  host;
        int     port;
        int     sockfd;
};

class SocketService: public SocketBase
{
    public:
        SocketService(string, int);
        ~SocketService();
        void Run(void);
    private:
        list<ShadowsocksPipe *> pipelist;
};

#define MAXLISTENQ  32
SocketService::SocketService(string h, int p): SocketBase(h, p)
{
    struct sockaddr_in saddr = init_sockaddr_in(host, port);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        throw runtime_error("SocketService socket error");
    }

    if(bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        throw runtime_error("SocketService bind error");
    }

    if(listen(sockfd, MAXLISTENQ) == -1)
    {
        throw runtime_error("SocketService listen error");
    }
}

SocketService::~SocketService(void)
{
    close(sockfd);
}

void SocketService::Run()
{
    int conn = 0;
    ShadowsocksPipe *pipe = NULL;
    list<ShadowsocksPipe *>::iterator iter;
    for(;;)
    {
        cout << "num=" << pipelist.size() << endl;
        for(iter = pipelist.begin(); iter != pipelist.end();)
        {
            list<ShadowsocksPipe *>::iterator t = iter++;
            pipe = *t;
            if(0 == pipe->status)
            {
                pipelist.erase(t);
                delete pipe;
            }
        }

        if((conn = accept(sockfd, NULL, NULL)) < 0)
        {
            cerr << __func__ << ": accept error" << endl;
            break;
        }

        pipe = new ShadowsocksPipe();
        pipe->request = new ShadowsocksConnect(conn);
        pipelist.push_back(pipe);

        thread t(&ShadowsocksPipe::Run, pipe);
        t.detach();
    }
    cout << __func__ << ": end!" << endl;
}

int main(int argc, char* argv[])
{
    const char *configFile = NULL;

    int ch = 0;
    while((ch=getopt(argc, argv, "c:")) != -1)
    {
        switch(ch)
        {
            case 'c':
                configFile = optarg;
                break;
        }
    }
    
    if(NULL == configFile)
        configFile = "./config.json";
    /*
     * TODO: Parse config file
     */


    // Start a service
    string host = "0.0.0.0";
    int    port = 1080;
    SocketService localservice(host, port);
    localservice.Run();
}
