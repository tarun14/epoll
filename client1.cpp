#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>

class client{
    public:
        int clientSockfd;
        client(): clientSockfd(-1){}
        explicit client(int fd): clientSockfd(fd){}

        client(client &&clref) noexcept{
            if(this != &clref){
                clientSockfd = std::move(clref.clientSockfd);
                clref.clientSockfd = -1;
            }
        }
        client& operator=(client &&clref) noexcept{
            if(this != &clref){
                if(clientSockfd >= 0){
                    close(clientSockfd);
                }
                clientSockfd = std::move(clref.clientSockfd);
                clref.clientSockfd = -1;
            }
            return *this;
        }
        //Copy constructor is invalidated
        client(const client&) = delete;
        //Copy assignment operator is invalidated
        client& operator=(const client&) = delete;

        ~client(){
            if(clientSockfd >= 0){
                close(clientSockfd);
                clientSockfd = -1;
            }
        }
};

int main()
{
    //1. Create Socket
    client cl(socket(AF_INET, SOCK_STREAM, 0));
    if(cl.clientSockfd < 0){
        std::cerr<<"Socket creation failed\n";
        return 1;
    }

    sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    if(inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0){
        std::cerr<<"inet_pton failed\n";
        return 1;
    }

    //2. Initiates a connection request on a socket. It connects the socket fd to the mentioned sockaddr
    // address
    if (connect(cl.clientSockfd, reinterpret_cast<sockaddr*>(&serverAddr), 
               sizeof(serverAddr))<0){
        std::cerr<<"connect failed\n";
	    return 1;
    }

    int i=10;

    const char* message = "hello! Server, from client1\n";
    int bytesSent=0;
    for(;i>0;i--){
    	//3. Transmit a message to another connected Socket
    	bytesSent = send(cl.clientSockfd, message, strlen(message), 0);

        if(bytesSent<0){
                perror("Send call failed");
        }else{
                std::cout<<"Message sent,bytes sent:"<<bytesSent<<"\n";
        }
	    std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}

