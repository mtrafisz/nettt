cmake_minimum_required(VERSION 3.5)
project (nettt)

add_executable (nettt_server source/server.c)
add_executable (nettt_client source/client.c)

if (WIN32)
    target_link_libraries (nettt_server ws2_32)
    target_link_libraries (nettt_client ws2_32)
endif (WIN32)
