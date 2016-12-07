#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//#include "json/reader.h"

#define DEBUG
#define QUEUE    20

using namespace std;

class ssServer
{
	public:
		ssServer(const char * host, const int port);
		~ssServer();
		int Run();
	private:
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
		int conn = accept(sockfd, (struct sockaddr *)&client, &length);
		if(conn < 0)
		{
			cerr << "accept error" << endl;
			break;
		}

		cout << "connecting..." << endl;

		close(conn);
		break;
	}
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
	/*
	int sockfd;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_sockaddr;
	server_sockaddr.sin_family      = AF_INET;
	server_sockaddr.sin_port        = htons(MYPORT);
	server_sockaddr.sin_addr.s_addr = htons(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1)
	{
		cerr << "bind error" << endl;
		exit(1);
	}
	*/
	ssServer server = ssServer("0.0.0.0", 1080);
	server.Run();
}
