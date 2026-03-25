cmake_minimum_required(VERSION 3.15)
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/protobufs_protobuf-c)
file(GLOB PROTO_SOURCES "${PROTO_DIR}/meshtastic/*.pb-c.c")
# deviceonly.pb-c.h 包含 nanopb.pb-c.h → google/protobuf/descriptor.pb-c.h（缺失）
# 我们的 proto_parser.c 不使用 DeviceOnly，排除这两个文件
list(REMOVE_ITEM PROTO_SOURCES "${PROTO_DIR}/meshtastic/deviceonly.pb-c.c")
set(MESHCORE_SOURCES
    src/frame_parser.c
    src/serial_port.c
    src/proto_parser.c
    src/node_manager.c
    src/heartbeat.c
)
add_library(meshcore STATIC
    ${MESHCORE_SOURCES}
    ${PROTO_SOURCES}
)
target_include_directories(meshcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_DIR}
    ${PROTOBUF_C_INCLUDE_DIR}
)
target_link_libraries(meshcore PUBLIC 
    ${PROTOBUF_C_RUNTIME_LIB}
    ${PLATFORM_LIBS}
)