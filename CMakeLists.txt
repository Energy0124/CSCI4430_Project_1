cmake_minimum_required(VERSION 3.7)
project(CSCI4430_Project_1)

set(CMAKE_CXX_STANDARD 11)

include_directories(client)
include_directories(server)

set(SOURCE_FILES_CLIENT
        client/client.c
        client/mtcp_client.c
        client/mtcp_client.h
        client/mtcp_common.h)

set(SOURCE_FILES_SERVER
        server/mtcp_common.h
        server/mtcp_server.c
        server/mtcp_server.h
        server/server.c)

add_executable(CSCI4430_Project_1_CLIENT ${SOURCE_FILES_CLIENT})
add_executable(CSCI4430_Project_1_SERVER ${SOURCE_FILES_SERVER})


