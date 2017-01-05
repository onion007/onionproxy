#include "local.h"
#include "ShadowsocksConnect.cpp"

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
