cmake_minimum_required(VERSION 3.23)

project(epoll)

add_executable(multiclientServer)
target_sources(multiclientServer
  PRIVATE
    multiclientServer.cpp
)


add_executable(client1)
target_sources(client1
  PRIVATE
    client1.cpp
)


add_executable(client2)
target_sources(client2
  PRIVATE
    client2.cpp
)
