#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
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
	protected:
		char host[64];
		int port;
		struct sockaddr_in server;
		int sockfd;
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
	private:
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
		// I don't why it can recv 1 byte, it's unuseful. Maybe it's ACK
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

void * handle2(void *t)
{
	Connect * conn = (Connect *)t;
	for(;;)
	{
		// client <- 1080 <- 8388
		char    buffer[2048];
		ssize_t n;
		n = conn->rServer->read(buffer, 2048);
		cout << "handle2 for loop n=" << n << endl;
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
	//cout << "handle [" << conn->sockfd << "]..." << endl;

	// set timeout
	conn->setreadtimeout(10 * 1000);

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
	conn->rServer = new ssServer(RSERVERIP, RSERVERPORT);
	conn->rServer->write(conn->buffer, len);
	pthread_t tids;
	err = pthread_create(&tids, NULL, handle2, (void *)conn);
	for(;;)
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
	/*
	char buf[1024];
	int  datalen;
	while(datalen = recv(conn->sockfd, buf, 1024, 0) > 0)
	{
		cout << "datalen=" << datalen << endl;
	}
	cout << "close connection" << conn << endl;
	close(conn);
	*/

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

int main(int argc, char* argv[])
{
	const char *configFile = NULL;

	int ch = 0;
	while((ch=getopt(argc, argv, "f:")) != -1)
	{
		switch(ch)
		{
			case 'f':
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
	ssLocal lserver = ssLocal("0.0.0.0", 1080);
	lserver.Run();
}
