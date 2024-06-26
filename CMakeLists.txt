cmake_minimum_required(VERSION 3.10)
project(DGS)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

SET(LIBMESSAGES ./MESSAGES/message.pb.cc)
SET(LIBCLIENT ./src/client.cc)
SET(LIBSERVER ./src/server.cc)
SET(LIBSTRATEGY ./src/strategies.cc)
SET(LIBNTPSERVER ./lib/ntp-master/server.o)
SET(LIBNTPCLIENT ./lib/ntp_client-Gaa/build/CMakeFiles/ntp_cli.dir/src/ntp_client.c.o)
SET(LIBXTIME ./lib/ntp_client-Gaa/build/CMakeFiles/ntp_cli.dir/src/xtime.c.o)
SET(LIBPRINTLEVEL ./src/print_level.cc)


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/MESSAGES)
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/lib)

#aux_source_directory(${PROJECT_SOURCE_DIR} SrcFiles)
#aux_source_directory(./MESSAGES SrcFiles)
#aux_source_directory(./JOBS SrcFiles)

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

ADD_LIBRARY(LIBLAUCHER ${LIBMESSAGES} ${LIBCLIENT} ${LIBPRINTLEVEL})
#ADD_LIBRARY(LIBLAUCHER ${LIBCLIENT})
ADD_LIBRARY(LIBGLOBAL ${LIBSERVER} ${LIBMESSAGES} ${LIBSTRATEGY} ${LIBNTPSERVER} ${LIBPRINTLEVEL})
#ADD_LIBRARY(LIBGLOBAL ${LIBMESSAGES})
add_library(LIBLOCAL ${LIBMESSAGES} ${LIBCLIENT} ${LIBNTPCLIENT} ${LIBXTIME} ${LIBPRINTLEVEL})
add_library(LIBJOB ${LIBPRINTLEVEL})

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

SET(LAUCHER ./src/laucher.cc)
SET(GLOBAL ./src/global_scheduler.cc)
SET(LOCAL ./src/local_scheduler.cc)
SET(JOB0 ./src/JOBS/job0.cc) 
SET(JOB1 ./src/JOBS/job1.cc)
SET(JOB2 ./src/JOBS/job2.cc)
SET(JOB3 ./src/JOBS/job3.cc)
SET(JOB4 ./src/JOBS/job4.cc)
add_executable(Job0 ${JOB0})
add_executable(Job1 ${JOB1})
add_executable(Job2 ${JOB2})
add_executable(Job3 ${JOB3})
add_executable(Job4 ${JOB4})
add_executable(Laucher ${LAUCHER})
add_executable(Global_sched ${GLOBAL})
add_executable(Local_sched ${LOCAL})
target_link_libraries(Job0 LIBJOB)
target_link_libraries(Job1 LIBJOB)
target_link_libraries(Job2 LIBJOB)
target_link_libraries(Job3 LIBJOB)
target_link_libraries(Job4 LIBJOB)
target_link_libraries(Laucher LIBLAUCHER ${Protobuf_LIBRARIES})
target_link_libraries(Global_sched LIBGLOBAL ${Protobuf_LIBRARIES})
target_link_libraries(Local_sched LIBLOCAL ${Protobuf_LIBRARIES})
