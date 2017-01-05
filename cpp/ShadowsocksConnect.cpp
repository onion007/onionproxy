#include "local.h"

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
