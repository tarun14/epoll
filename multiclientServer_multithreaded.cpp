#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>
#include <thread>
#include <vector>
#include <atomic>
#include <csignal>

#define MAX_EVENTS 10

std::atomic<bool> running(true);

void stopHandler(int signum)
{
    std::cerr<<"Signal received, stopping server...\n";
    running = false;
}

void myThreadHandler(int clientSockfd)
{
    char buf[512] = {0};

    while(running){
        ssize_t n = read(clientSockfd, buf, sizeof(buf));
        if(n>0){
            std::cout<<"Data read from client:"<<std::string(buf, n)<<std::endl;
        }else if(n==0){//client closed the socket, we must close connection
            std::cout<<"Client disconnected.Closing fd:"<<clientSockfd<<std::endl;
            break;
        }else if(errno == EINTR){
            if(!running){
                std::cerr<<"Server is stopping, exiting thread...\n";
                break;
            }
            continue; // Interrupted by signal, retry the read operation
        }else{
            std::cerr<<"read failed\n";
            break;
        }
    }
    close(clientSockfd);
}

int main()
{
    // Register signal handlers for graceful shutdown
    struct sigaction sa{};
    sa.sa_handler = stopHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    //1. Create server socket
    int servSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(servSockfd < 0){
        std::cerr<<"socket creation failed!\n";
        return 1;
    }
    std::cout<<"Socket call succesfull\n";
    //2. Configure server address
    sockaddr_in servaddress{};
    servaddress.sin_family = AF_INET;
    servaddress.sin_port = htons(8080);
    servaddress.sin_addr.s_addr = INADDR_ANY;

    //3. Bind server address with server socket
    if(bind(servSockfd, (struct sockaddr *)&servaddress, 
            sizeof(servaddress)) < 0){
        std::cerr<<"bind call failed\n";
        perror("bind failed");
        close(servSockfd);
        return 1;
    }
    std::cout<<"bind call successfull\n";
    //4. listen for connection request
    if(listen(servSockfd, 5) < 0){
        std::cerr<<"listen call failed\n";
        close(servSockfd);
        return 1;
    }
    std::cout<<"listen call successfull\n";
    //5. Create new epoll instance (fd)
    int epfd = epoll_create1(0);
    if(epfd == -1){
        std::cerr<<"epoll_create1 failed\n";
        close(servSockfd);
        return 1;
    }

    std::cout<<"epoll_create1 successfull\n";
    epoll_event ev{}, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = servSockfd;

    //6. Add entry for servSockfd to interest list of epoll instance(epfd).
    // we ask kernel to look for any connection request for servSockfd
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, servSockfd, &ev) < 0){
        std::cerr<<"epoll_ctl failed for adding servSockfd\n";
        close(epfd);
        close(servSockfd);
        return 1;
    }
    std::cout<<"epoll_ctl call successfull\n";

    std::vector<std::thread> threads;
    while(running){
        //7. Keep waiting indefinitely for any event for file descriptors
        // present in epoll instance (epfd) interest list. Return such events
        // in buffer 'events'. In present case, only servSockfd file descriptor
        // is present in epfd interest list. Hence waiting will be done for
        // any events for that fd. Returns number of fds ready for requested I/O
        // operation.
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if(nfds < 0){
            std::cerr<<"epoll_wait failed\n";
            if(errno == EINTR){
                break; // Interrupted by signal, exit the loop to shutdown gracefully
            }
            close(epfd);
            close(servSockfd);
            return 1;
        }
        std::cout<<"epoll_wait returned, nfds:"<<nfds<<"\n";

        for(int i=0; i<nfds; i++){
            //Event(connection request) came for servSockfd
            if(events[i].data.fd == servSockfd){
                std::cout<<"connection request for server\n";
                //Accept connection from client
                int clientSockfd = accept(servSockfd, nullptr, nullptr);
                if(clientSockfd < 0){
                    if((errno == EINTR)||(errno == EAGAIN) || (errno == EWOULDBLOCK)){
                        continue;
                    }else if(errno == EBADF){
                        std::cerr<<"Server socket fd is closed, stopping server...\n";
                        running = false;
                        break;
                    }
                    std::cerr<<"accept failed!\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep briefly before retrying
                    continue;
                };
                std::cout<<"accept call successfull for server connection request\n";
                // Add thread to vector for cleanup later
                threads.emplace_back(myThreadHandler, clientSockfd);
            }
        }//For loop
    }//While loop

    if(!running){
        std::cerr<<"Server is stopping, closing epoll fd and server socket...\n";
    }
    close(epfd);
    close(servSockfd);
    //Close all client worker threads
    for(auto &t: threads){
        if(t.joinable()){
            t.join();
        }
    }
    
    return 0;
}
