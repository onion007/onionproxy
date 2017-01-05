#include "local.h"
#include "ShadowsocksPipe.cpp"

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
