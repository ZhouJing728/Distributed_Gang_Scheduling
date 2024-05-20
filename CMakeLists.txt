cmake_minimum_required(VERSION 3.10)
project(DGS)

SET(LIBMESSAGES ./MESSAGES/message.pb.cc)
SET(LIBCLIENT client.cc)
SET(LIBSERVER server.cc)
SET(LIBSTRATEGY ./SCHED_STRATEGY/strategies.cc)
SET(LIBNTPSERVER ./ntp-master/ntp-master/server.o)
SET(LIBNTPCLIENT ./ntp_client-Gaa/build/CMakeFiles/ntp_cli.dir/src/ntp_client.c.o)
SET(LIBXTIME ./ntp_client-Gaa/build/CMakeFiles/ntp_cli.dir/src/xtime.c.o)


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/MESSAGES)
include_directories(${PROJECT_SOURCE_DIR}/JOBS)
include_directories(${PROJECT_SOURCE_DIR}/SCHED_STRATEGY)
include_directories(${PROJECT_SOURCE_DIR}/ntp-master/ntp-master)
include_directories(${PROJECT_SOURCE_DIR}/ntp_client-Gaa)

#aux_source_directory(${PROJECT_SOURCE_DIR} SrcFiles)
#aux_source_directory(./MESSAGES SrcFiles)
#aux_source_directory(./JOBS SrcFiles)

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

ADD_LIBRARY(LIBLAUCHER ${LIBMESSAGES} ${LIBCLIENT})
#ADD_LIBRARY(LIBLAUCHER ${LIBCLIENT})
ADD_LIBRARY(LIBGLOBAL ${LIBSERVER} ${LIBMESSAGES} ${LIBSTRATEGY} ${LIBNTPSERVER})
#ADD_LIBRARY(LIBGLOBAL ${LIBMESSAGES})
add_library(LIBLOCAL ${LIBMESSAGES} ${LIBCLIENT} ${LIBNTPCLIENT} ${LIBXTIME})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)

# file(
#     GLOB_RECURSE mains RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
#     "${CMAKE_CURRENT_SOURCE_DIR}/laucher.cc"
#     "${CMAKE_CURRENT_SOURCE_DIR}/global_scheduler.cc"
# )

# foreach(mainfile IN LISTS mains)
#     get_filename_component(mainname ${mainfile} NAME_WE)
#     add_executable(${mainname} ${mainfile})
    
# endforeach(mainfile IN LISTS mains)

SET(LAUCHER laucher.cc)
SET(GLOBAL global_scheduler.cc)
SET(LOCAL local_scheduler.cc)
add_executable(Laucher ${LAUCHER})
add_executable(Global_sched ${GLOBAL})
add_executable(Local_sched ${LOCAL})
target_link_libraries(Laucher LIBLAUCHER ${Protobuf_LIBRARIES})
target_link_libraries(Global_sched LIBGLOBAL ${Protobuf_LIBRARIES})
target_link_libraries(Local_sched LIBLOCAL ${Protobuf_LIBRARIES})
