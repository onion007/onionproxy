#include <iostream>
#include <list>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//#include "json/reader.h"

#define DEBUG
#define QUEUE       20
#define BUFFERSIZE  2048
#define SOCKSVER5   5

using namespace std;

#define RSERVERIP     "127.0.0.1"
#define RSERVERPORT   8388
class ssServer
{
	public:
		ssServer(const char *, const int);
		ssize_t write(char *, size_t);
		ssize_t read(char *, size_t);
		int sockfd;
	protected:
		char host[64];
		int port;
		struct sockaddr_in server;
};

ssServer::ssServer(const char * h, const int p)
{
	strcpy(host, h);
	port = p;
	server.sin_family = AF_INET;
	server.sin_port   = htons(port);
	server.sin_addr.s_addr   = inet_addr(host);

	if((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0 )
	{
		throw runtime_error("ssServer() socket error");
	}

	if(0 != connect(sockfd, (struct sockaddr *)&server, sizeof(server)))
	{
		throw runtime_error("ssServer() connect error");
	}
}

ssize_t ssServer::write(char * buffer, size_t size)
{
	return send(sockfd, buffer, size, 0);
}

ssize_t ssServer::read(char * buffer, size_t size)
{
	return recv(sockfd, buffer, size, 0);
}

class Connect
{
	public:
		Connect(int fd)
		{
			sockfd = fd;
		}
		~Connect()
		{
			close(sockfd);
			if(rServer)
			{
				delete rServer;
			}
		}
		int     handshake();
		int     getrequest();
		ssize_t read(char *, size_t);
		ssize_t write(const char *, const int);
		void    setreadtimeout(int);
		void    displayclientinfo();
		ssServer *rServer = NULL;
		char               buffer[BUFFERSIZE];
		int                sockfd;
};

void Connect::displayclientinfo()
{
	struct sockaddr addr;
	socklen_t    addr_size = sizeof(addr);
	getpeername(sockfd, &addr, &addr_size);
	cout << "remote: " << inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr);
	cout << ":" << ((struct sockaddr_in *)&addr)->sin_port << endl;

	return;
}

void Connect::setreadtimeout(int timeout)
{
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
}

ssize_t Connect::read(char * buffer, size_t size)
{
	ssize_t n;
	while((n = recv(sockfd, buffer, size, 0)) > 0)
	{
		// Ignore that 1-byte package, it's unuseful. it should be ACK package.
		if(n > 1)
		{
			break;
		}
		cout << "read n=" << n << endl;
	}
	return n;
}

ssize_t Connect::write(const char * buffer, const int size)
{
	return send(sockfd, buffer, size, 0);
}

int Connect::handshake()
{
	const int idVer     = 0;
	const int idNmethod = 1;
	char      buffer[260];

	size_t n = read(buffer, idNmethod + 1);
	cout << "handshake read n=" << n << endl;
	if(n <= 0)
	{
		cerr << "read error" << endl;
	}
	if(SOCKSVER5 != buffer[idVer])
	{
		cerr << "not socks5 version!" << endl;
		return 1;
	}

	char wbuf[2];
	wbuf[0] = 0x5;
	wbuf[1] = 0x0;
	if(2 != write(wbuf, 2))
	{
		cerr << "write error" << endl;
		return 1;
	}

	return 0;
}

struct socks5request {
	uint8_t ver;
	uint8_t cmd;
	uint8_t rsv;
	uint8_t atyp;
};
#define IPV4    1
#define DOMAIN  3
#define IPV6    4

int Connect::getrequest()
{
	char    buffer[263];
	
	ssize_t  n;
	n = read(buffer, 263);

	struct socks5request * s5 = (struct socks5request *)&buffer;
	if(SOCKSVER5 != s5->ver)
		return -1;

	int idType = 3;
	int len = n - idType;
	switch(s5->atyp)
	{
		case IPV4:
			break;
		case IPV6:
			break;
		case DOMAIN:
			break;
		default:
			cerr << "unkown type" << endl;
			return -1;
	}

	char wbuf[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x08, 0x43};
	if(10 != write(wbuf, 10))
	{
		cerr << "write error" << endl;
		return -1;
	}
#ifdef DEBUG
	memcpy(this->buffer, &buffer[idType], len);
	cout << "len=" << len << endl;
	cout << "S:";
	cout << hex;
	for(int i=0; i<len; i++)
	{
		cout << (int)(buffer[idType+i]);
		cout << " ";
	}
	cout << endl;
	cout << "D:";
	for(int i=0; i<len; i++)
	{
		cout << (int)(this->buffer[i]);
		cout << " ";
	}
	cout << dec << endl;
#endif
	displayclientinfo();
 
	return len;
}

class ssLocal
{
	public:
		ssLocal(const char *, const int);
		~ssLocal();
		int Run();
	protected:
		Connect * connect[QUEUE];
		char host[64];
		int port;
		struct sockaddr_in server;
		int sockfd;
};

ssLocal::ssLocal(const char * h, const int p)
{
	strcpy(host, h);
	port = p;
	server.sin_family = AF_INET;
	server.sin_port   = htons(port);
	server.sin_addr.s_addr   = inet_addr(host);
}

ssLocal::~ssLocal()
{
}

void handle2(Connect * conn)
{
	while(conn->sockfd > 0 && conn->rServer->sockfd > 0)
	{
		// client <- 1080 <- 8388
		char    buffer[2048];
		ssize_t n;
		n = conn->rServer->read(buffer, 2048);
		if( n > 0)
		{
			conn->write(buffer, n);
		}
		else
		{
			break;
		}
	}
	pthread_exit((void *)0);
}

void * handle(void * t)
{
	Connect * conn = (Connect *)t;

	// set timeout
	conn->setreadtimeout(60 * 1000);

	int err, len;
	// socks5 handshake
	if(0 != conn->handshake())
	{
		cerr << "handshake error!" << endl;
		goto EXIT;
	}

	// getrequest
	if((len = conn->getrequest()) <= 0)
	{
		cerr << "getrequest error!" << endl;
		goto EXIT;
	}

	/*
	 * handle...
	 * 1. connect shadowsocks server
	 */
    {
	conn->rServer = new ssServer(RSERVERIP, RSERVERPORT);
	conn->rServer->write(conn->buffer, len);
    thread t(handle2, conn);
	while(conn->sockfd > 0 && conn->rServer->sockfd > 0)
	{
		// client -> 1080 -> 8388
		char    buffer[2048];
		ssize_t n;
		n = conn->read(buffer, 2048);
		if( n > 0 )
		{
			conn->rServer->write(buffer, n);
		}
		else
		{
			break;
		}
	}

#ifdef DEBUG
	cout << "waiting thread... " << endl;
#endif
    t.join();
#ifdef DEBUG
	cout << "thread done!!!!" << endl;
#endif
    }

EXIT:
	delete conn;
	pthread_exit((void *)0);
}

int ssLocal::Run()
{
	cout << host << " " << port << endl;
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		cerr << "socket errror" << endl;
		//return 1;
		exit(1);
	}
	
	if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		cerr << "bind error" << endl;
		exit(1);
	}

	if(listen(sockfd, QUEUE) == -1)
	{
		cerr << "listen error" << endl;
		exit(1);
	}

	for(;;)
	{
		struct sockaddr_in client;
		socklen_t length = sizeof(client);
		int connfd = accept(sockfd, (struct sockaddr *)&client, &length);
		if(connfd < 0)
		{
			cerr << "accept error" << endl;
			break;
		}
		Connect * conn = new Connect(connfd);

		cout << "connecting..." << endl;
		pthread_t tids;
		int err = pthread_create(&tids, NULL, handle, (void *)conn);
		if(0 != err)
		{
			cerr << "Create thread Failed!" << endl;
		}

	}
	close(sockfd);
}

struct sockaddr_in init_sockaddr_in(string h, int p)
{
    struct sockaddr_in saddr;
    saddr.sin_family      = AF_INET;
    saddr.sin_port        = htons(p);
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
                close(sockfd);
            }
        }
        ssize_t read(char *, size_t);
        ssize_t write(char *, size_t);
        char    buffer[4096];
        void    settimeout(int timeout);
        int     getsockfd() { return sockfd; }
    private:
        int     sockfd;
};

void ShadowsocksConnect::settimeout(int timeout)
{
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
}

ssize_t ShadowsocksConnect::read(char * buffer, size_t len)
{
    /*
    if(sockfd <= 0)
    {
        throw "sockfd is close";
    }
    */
    size_t n;
	while((n = recv(sockfd, buffer, len, 0)) > 0)
	{
		// Ignore that 1-byte package, it's unuseful. it should be ACK package.
		if(n != 1)
		{
			break;
		}
	}
	return n;
}

ssize_t ShadowsocksConnect::write(char * buffer, size_t len)
{
    /*
    if(sockfd <= 0)
    {
        throw "sockfd is close";
    }
    */
	return send(sockfd, buffer, len, 0);
}

class ShadowsocksPipe
{
    public:
        ShadowsocksConnect  *request  = NULL;
        ShadowsocksConnect  *response = NULL;
        void operator() ();
    private:
        void run(void);
        int  handshake(void);
        int  getrequest(void);
        static void forward(ShadowsocksConnect *, ShadowsocksConnect *, const char *);
};

int ShadowsocksPipe::handshake(void)
{
	const int idVer     = 0;
	const int idNmethod = 1;
    char      buffer[263];

	size_t n = request->read(buffer, idNmethod + 1);
	if(n <= 0)
	{
        throw ": can't read data!";
	}
	if(SOCKSVER5 != buffer[idVer])
	{
        throw ": get not socks5 data!";
	}

    char wbuf[2] = {0x05, 0x00};
	if(2 != request->write(wbuf, 2))
	{
        throw ": write error!";
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
        throw ": get not socks5 data!";
	}

    char wbuf[] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x08, 0x43};
    request->write(wbuf, 10);

    n -= idType;
    memcpy(request->buffer, &buffer[3], n);

    return n;
}

void ShadowsocksPipe::forward(ShadowsocksConnect *src, ShadowsocksConnect *dst, const char * msg)
{
    for(;;)
    {
        int    n;
        if((n = src->read(src->buffer, 4096)) > 0 ) 
        {
            dst->write(src->buffer, n);
        }
        cout << msg << ": forward n=" << n << " sockfd=" << src->getsockfd() << endl;
    }
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

        /*
        thread t(forward, response, request, "response->request");
        forward(request, response, "request->response");
        */
        thread t(forward, request, response, "request->response");
        forward(response, request, "response->request");
    }
    catch (const char* msg)
    {
        cerr << msg << endl;
    }
}

void ShadowsocksPipe::operator() (void)
{
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
        list<ShadowsocksPipe> pipelist;
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
    for(;;)
    {
		if((conn = accept(sockfd, NULL, NULL)) < 0)
        {
            cerr << __func__ << ": accept error" << endl;
            break;
        }

        pipe = new ShadowsocksPipe();
        pipe->request = new ShadowsocksConnect(conn);

        thread t(*pipe);
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
#ifdef DEBUG
	cout << "config=" << configFile << endl;
#endif
	/*
	 * TODO: Parse config file
	 */


    // Start a service
    string host = "0.0.0.0";
    int    port = 1080;
    SocketService localservice(host, port);
    localservice.Run();
}
