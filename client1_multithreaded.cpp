#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>

int main()
{
    //1. Create Socket
    int clientSock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSock_fd < 0){
        std::cout<<"Socket creation failed\n";
    }

    sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    if(inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0){
        std::cerr<<"inet_pton failed\n";
        close(clientSock_fd);
        return 1;
    }

    //2. Initiates a connection request on a socket. It connects the socket fd to the mentioned sockaddr
    // address
    int ret = connect(clientSock_fd, reinterpret_cast<sockaddr*>(&serverAddr), 
               sizeof(serverAddr));
    if(ret<0){
        std::cout<<"connect failed\n";
	    close(clientSock_fd);
	    return 1;
    }else{
        std::cout<<"connect call successful\n";
    }

    const char* message = "hello! Server, from client1\n";
    int bytesSent=0;
    while(true){
    	//3. Transmit a message to another connected Socket
    	bytesSent = send(clientSock_fd, message, strlen(message), 0);

        if(bytesSent<0){
                perror("Send call failed");
        }else{
                std::cout<<"Message sent,bytes sent:"<<bytesSent<<"\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    close(clientSock_fd);

    return 0;
}

