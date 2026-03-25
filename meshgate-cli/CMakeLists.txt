cmake_minimum_required(VERSION 3.15)

set(CLI_SOURCES
    src/main.c
    src/tcp_client.c
    src/output_format.c
)

add_executable(meshgate-cli ${CLI_SOURCES})

target_include_directories(meshgate-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# CLI only needs platform socket libs, not the full meshcore
if(WIN32)
    target_link_libraries(meshgate-cli PRIVATE ws2_32)
else()
    target_link_libraries(meshgate-cli PRIVATE m)
endif()
