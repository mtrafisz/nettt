cmake_minimum_required(VERSION 3.5)
project (nettt)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(LIBRARY REQUIRED library)

execute_process(COMMAND git submodule init
                COMMAND git submodule update)

set (BUILD_EXAMPLES OFF CACHE STRING "" FORCE)
add_subdirectory(third-party/raylib)

add_library(logger STATIC third-party/logger/logger.c)

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

link_libraries(nettt logger)
include_directories(third-party)

add_executable(nettt_client ${NETTT_CLIENT_SOURCES})
add_executable(nettt_server ${NETTT_SERVER_SOURCES})

target_link_libraries(nettt_client raylib)
