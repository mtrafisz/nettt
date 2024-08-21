cmake_minimum_required(VERSION 3.5)
project (nettt)

find_package(PkgConfig REQUIRED)
# pkg_check_modules(LIBRARY REQUIRED library)

execute_process(COMMAND git submodule update --init --recursive --depth=1
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

file(GLOB NETTT_SOURCES "src/nettt/*.c")
file(GLOB_RECURSE NETTT_CLIENT_SOURCES "src/client/*.c")
file(GLOB_RECURSE NETTT_SERVER_SOURCES "src/server/*.c")

add_library(nettt ${NETTT_SOURCES})
if (WIN32)
    # target_link_libraries(nettt ws2_32)
    # error out for now:
    message(FATAL_ERROR "Windows is not supported yet")
else ()
    target_link_libraries(nettt pthread)
endif ()

add_executable(nettt_client ${NETTT_CLIENT_SOURCES})
add_executable(nettt_server ${NETTT_SERVER_SOURCES})

target_link_libraries(nettt_client nettt)
target_link_libraries(nettt_server nettt)
