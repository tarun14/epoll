#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fstream>
#include <vector>
#include <array>

#define MAX_EVENTS 10

// NOTES (as per listen manpage):
//        To accept connections, the following steps are performed:

//            (1)  A socket is created with socket(2).

//            (2)  The socket is bound to a local address using bind(2), so  that
//                 other sockets may be connect(2)ed to it.

//            (3)  A willingness to accept incoming connections and a queue limit
//                 for incoming connections are specified with listen().

//            (4)  Connections are accepted with accept(2).
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

class server{
       public:
        int servSockfd;//Server socket fd
        int epfd;//epoll fd
        std::vector<client> clientSockfd;
        server(){
            servSockfd=-1;
            epfd=-1;
        }
        ~server(){
            if(servSockfd>=0){
                close(servSockfd);
                servSockfd = -1;
            }
            if(epfd>=0){
                close(epfd);
                epfd = -1;
            }
        }
};


int main()
{
    server srv;
    srv.clientSockfd.reserve(32);

    int servSockfd=-1;
    //1. Create server socket
    servSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(servSockfd < 0){
        std::cerr<<"socket creation failed!\n";
        return 1;
    }
    //Only valid fd is copied
    srv.servSockfd = servSockfd;
    std::cout<<"Socket call succesfull\n";
    //2. Configure server address
    sockaddr_in servaddress{};
    servaddress.sin_family = AF_INET;
    servaddress.sin_port = htons(8080);
    servaddress.sin_addr.s_addr = INADDR_ANY;

    //3. Bind server address with server socket
    if(bind(srv.servSockfd, (struct sockaddr *)&servaddress, 
            sizeof(servaddress)) < 0){
        std::cerr<<"bind call failed\n";
        return 1;
    }

    //4. listen for connection request, socket is marked passive i.e.
    // willingness to accept connections is indicated.
    if(listen(srv.servSockfd, 5) < 0){
        std::cerr<<"listen call failed\n";
        return 1;
    }

    //5. Create new epoll instance (fd)
    int epfd=-1;
    epfd = epoll_create1(0);
    if(epfd == -1){
        std::cerr<<"epoll_create1 failed\n";
        return 1;
    }
    srv.epfd = epfd;

    epoll_event ev{}, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = srv.servSockfd;

    //6. Add entry for servSockfd to interest list of epoll instance(epfd).
    // we ask kernel to look for any connection request for srv.servSockfd
    if(epoll_ctl(srv.epfd, EPOLL_CTL_ADD, srv.servSockfd, &ev) < 0){
        std::cerr<<"epoll_ctl failed for adding servSockfd\n";
        return 1;
    }

    for(;;){
        //7. Keep waiting indefinitely for any event for file descriptors
        // present in epoll instance (epfd) interest list. Return such events
        // in buffer 'events'. In present case, only srv.servSockfd file descriptor
        // is present in epfd interest list. Hence waiting will be done for
        // any events for that fd. Returns number of fds ready (present in ready list)
        // for requested I/O operation.
        int nfds = epoll_wait(srv.epfd, events, MAX_EVENTS, -1);
        if(nfds < 0){
            if(nfds == -1 && errno == EINTR){
                std::cerr<<"User terminated, Exiting!!!\n";
                return 1;
            }
            std::cerr<<"epoll_wait failed\n";
            return 1;
        }

        for(int i=0; i<nfds; i++){
            //Event(connection request) came for servSockfd
            if(events[i].data.fd == srv.servSockfd){
                std::cout<<"connection request for server\n";
                
                // Accept connection from client
                // Extracts the first connection request on the queue of 
                // pending connections for the listening socket.
                int clientSockfd = accept(srv.servSockfd, nullptr, nullptr);
                if(clientSockfd < 0){
                    if(clientSockfd == -1 && errno == EINTR){
                        std::cerr<<"User terminated, Exiting!!!\n";
                        return 1;
                    }
                    std::cerr<<"accept failed!\n";
                    return 1;
                };
                client accepted(clientSockfd);
                //will create a new client and pass clientSockfd to its constructor
                srv.clientSockfd.emplace_back(std::move(accepted));

                //Create event for client socket
                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = clientSockfd;//Update this to use srv based client

                if(epoll_ctl(srv.epfd, EPOLL_CTL_ADD, clientSockfd, &client_ev) < 0){
                    std::cerr<<"epoll_ctl failed for adding clientSockfd\n";
                    srv.clientSockfd.pop_back();
                    return 1;
                }
            //Else: event(communication bytes) came for client
            }else{
                char buf[512] = {0};
                std::cout<<"Connection request for client\n";
                int n = read(events[i].data.fd, buf, sizeof(buf));
                if(n<0){
                    if(n == -1 && errno == EINTR){
                        std::cerr<<"User terminated, Exiting!!!\n";
                        return 1;
                    }
                    std::cerr<<"read failed\n";
                    return 1;
                }else if(n==0){//when client closed the socket, we must close connection
                    std::cout<<"Client disconnected.Closing fd:"<<events[i].data.fd<<std::endl;
                    epoll_ctl(srv.epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                    //Delete this specific client from vector
                    for(auto it=srv.clientSockfd.begin();it!=srv.clientSockfd.end();){
                        if(it->clientSockfd == events[i].data.fd){
                            it = srv.clientSockfd.erase(it);
                            break;
                        }
                        ++it;
                    }
                }else{
                    std::cout<<"Data read from client:"<<buf<<std::endl;
                }
            }
        }//For loop
    }//While loop

    return 0;
}
