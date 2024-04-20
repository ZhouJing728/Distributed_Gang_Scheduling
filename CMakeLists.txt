cmake_minimum_required(VERSION 3.10)
project(DGS)

SET(LIBMESSAGES ./MESSAGES/message.pb.cc)
SET(LIBCLIENT client.cc)
SET(LIBSERVER server.cc)


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/MESSAGES)
include_directories(${PROJECT_SOURCE_DIR}/JOBS)

#aux_source_directory(${PROJECT_SOURCE_DIR} SrcFiles)
#aux_source_directory(./MESSAGES SrcFiles)
#aux_source_directory(./JOBS SrcFiles)

ADD_LIBRARY(LIBLAUCHER ${LIBMESSAGES} ${LIBCLIENT})
#ADD_LIBRARY(LIBLAUCHER ${LIBCLIENT})
ADD_LIBRARY(LIBGLOBAL ${LIBSERVER} ${LIBMESSAGES})
#ADD_LIBRARY(LIBGLOBAL ${LIBMESSAGES})

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
add_executable(Laucher ${LAUCHER})
add_executable(Global_sched ${GLOBAL})
target_link_libraries(Laucher LIBLAUCHER)
target_link_libraries(Global_sched LIBGLOBAL)
