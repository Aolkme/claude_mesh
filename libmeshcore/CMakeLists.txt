cmake_minimum_required(VERSION 3.15)
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/protobufs_protobuf-c)

# 官方 Meshtastic protobuf-c 文件
file(GLOB PROTO_SOURCES "${PROTO_DIR}/meshtastic/*.pb-c.c")
# deviceonly.pb-c.h 依赖缺失的 descriptor.pb-c.h，排除
list(REMOVE_ITEM PROTO_SOURCES "${PROTO_DIR}/meshtastic/deviceonly.pb-c.c")

# thingseye 私有配置 protobuf-c 文件
file(GLOB THINGSEYE_SOURCES "${PROTO_DIR}/thingseye/*.pb-c.c")

set(MESHCORE_SOURCES
    src/frame_parser.c
    src/frame_builder.c
    src/serial_port.c
    src/proto_parser.c
    src/proto_builder.c
    src/admin_builder.c
    src/node_manager.c
    src/heartbeat.c
)
add_library(meshcore STATIC
    ${MESHCORE_SOURCES}
    ${PROTO_SOURCES}
    ${THINGSEYE_SOURCES}
)
target_include_directories(meshcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTO_DIR}
    ${PROTO_DIR}/thingseye
    ${PROTOBUF_C_INCLUDE_DIR}
)
target_link_libraries(meshcore PUBLIC 
    ${PROTOBUF_C_RUNTIME_LIB}
    ${PLATFORM_LIBS}
)