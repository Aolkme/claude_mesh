cmake_minimum_required(VERSION 3.15)

set(GATEWAY_SOURCES
    src/main.c
    src/config.c
    src/log.c
    src/event_loop.c
    src/tcp_server.c
    src/command_handler.c
)

add_executable(meshgateway ${GATEWAY_SOURCES})

target_include_directories(meshgateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(meshgateway PRIVATE meshcore)
