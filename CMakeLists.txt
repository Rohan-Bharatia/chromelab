cmake_minimum_required(VERSION 3.20)
project(chromelab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include("cmake/runtime.cmake")
use_runtime("static")

# Dependencies
find_package(gRPC REQUIRED)
find_package(Threads REQUIRED)
find_library(MHD_LIBRARY NAMES microhttpd REQUIRED)
find_library(LLAMA_LIBRARY NAMES llama REQUIRED)

add_subdirectory(third_party/toml++ EXCLUDE_FROM_ALL)

find_program(PROTOC_EXECUTABLE protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "gRPC version: ${gRPC_VERSION}")
message(STATUS "protoc: ${PROTOC_EXECUTABLE}")
message(STATUS "grpc_cpp_plugin: ${GRPC_CPP_PLUGIN_EXECUTABLE}")

# Proto Generation
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUT})

set(PROTO_FILES
    telemetry.proto
    events.proto
    services.proto
    ai.proto
    remote.proto
    labd.proto
)

set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(proto ${PROTO_FILES})
    get_filename_component(proto_name ${proto} NAME_WE)

    set(pb_h ${PROTO_OUT}/${proto_name}.pb.h)
    set(pb_cc ${PROTO_OUT}/${proto_name}.pb.cc)
    set(grpc_h ${PROTO_OUT}/${proto_name}.grpc.pb.h)
    set(grpc_cc ${PROTO_OUT}/${proto_name}.grpc.pb.cc)

    add_custom_command(
        OUTPUT ${pb_h} ${pb_cc}
        COMMAND ${PROTOC_EXECUTABLE}
            --cpp_out=${PROTO_OUT}
            --proto_path=${PROTO_DIR}
            ${PROTO_DIR}/${proto}
        DEPENDS ${PROTO_DIR}/${proto}
        COMMENT "protoc: ${proto} -> pb.{h,cc}"
    )

    add_custom_command(
        OUTPUT ${grpc_h} ${grpc_cc}
        COMMAND ${PROTOC_EXECUTABLE}
            --grpc_out=${PROTO_OUT}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
            --proto_path=${PROTO_DIR}
            ${PROTO_DIR}/${proto}
        DEPENDS ${pb_h}
        COMMENT "protoc-gen-grpc: ${proto} -> grpc.pb.{h,cc}"
    )

    list(APPEND PROTO_SRCS ${pb_cc} ${grpc_cc})
    list(APPEND PROTO_HDRS ${pb_h} ${grpc_h})
endforeach()

# Proto Library (shared between labd and labctl)
add_library(chromelab_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(chromelab_proto PUBLIC ${PROTO_OUT})
target_link_libraries(chromelab_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc
)

# labd (daemon)
add_executable(labd
    src/labd/main.cc
    src/labd/server.cc
    src/labd/config.cc
    src/labd/events/bus.cc
    src/labd/events/store.cc
    src/labd/services/manager.cc
    src/labd/dashboard/httpd.cc
    src/labd/ai/engine.cc
    src/labd/collector/collector.cc
    src/labd/collector/cpu.cc
    src/labd/collector/memory.cc
    src/labd/collector/disk.cc
    src/labd/collector/network.cc
    src/labd/collector/temperature.cc
    src/labd/collector/load.cc
    src/labd/collector/processes.cc
    src/labd/collector/uptime.cc
)
target_include_directories(labd PRIVATE src)
target_link_libraries(labd PRIVATE
    chromelab_proto
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    tomlplusplus::tomlplusplus
    ${MHD_LIBRARY}
    ncurses
    ${LLAMA_LIBRARY}
)
target_precompile_headers(labd PRIVATE src/pch.h)

# labctl (CLI)
add_executable(labctl
    src/labctl/main.cc
    src/labctl/tui.cc
)
target_include_directories(labctl PRIVATE src)
target_link_libraries(labctl PRIVATE
    chromelab_proto
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
    tomlplusplus::tomlplusplus
    ${MHD_LIBRARY}
    ncurses
    ${LLAMA_LIBRARY}
)
target_precompile_headers(labctl PRIVATE src/pch.h)

# Install
install(TARGETS labd labctl RUNTIME DESTINATION bin)
install(DIRECTORY etc/ DESTINATION etc/chromelab)
install(DIRECTORY web/ DESTINATION share/chromelab/web)
