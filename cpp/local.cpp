#include <iostream>
#include <cstdlib>
#include <cstring>
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
		}
		int  handshake();
		int  getrequest();
		int  read(char *, int);
		int  write(const char *, const int);
		void setreadtimeout(int);
	private:
		int  sockfd;
		char buffer[BUFFERSIZE];
};

void Connect::setreadtimeout(int timeout)
{
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
}

int Connect::read(char * buffer, int size)
{
	return recv(sockfd, buffer, size, 0);
}

int Connect::write(const char * buffer, const int size)
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

int Connect::getrequest()
{
	char    buffer[263];
	
	size_t  n = read(buffer, 263);
	cout << "getrequest read n=" << n << endl;
 
	return 0;
}

class ssServer
{
	public:
		ssServer(const char *, const int);
		~ssServer();
		int Run();
	private:
		Connect * connect[QUEUE];
		char host[64];
		int port;
		struct sockaddr_in server;
		int sockfd;
};

ssServer::ssServer(const char * h, const int p)
{
	//host = h;
	strcpy(host, h);
	port = p;
	server.sin_family = AF_INET;
	server.sin_port   = htons(port);
	server.sin_addr.s_addr   = inet_addr(host);
}

ssServer::~ssServer()
{
}

void * handle(void * t)
{
	Connect * conn = (Connect *)t;
	//cout << "handle [" << conn->sockfd << "]..." << endl;

	// set timeout
	conn->setreadtimeout(10 * 1000);

	int err;
	// socks5 handshake
	if(0 != conn->handshake())
	{
		cerr << "handshake error!" << endl;
	}

	// getrequest
	if(0 != conn->getrequest())
	{
		cerr << "getrequest error!" << endl;
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
	pthread_exit((void *)0);
}

int ssServer::Run()
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
	ssServer server = ssServer("0.0.0.0", 1080);
	server.Run();
}
