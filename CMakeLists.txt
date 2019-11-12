cmake_minimum_required(VERSION 3.14)
project(nxvnc C)

set(CMAKE_C_STANDARD 99)

set(SOURCE          main.c  utils.c  handshake.c  d3des.c  logerr.c  stream.c  vncmsg.c  fb.c  mouse.c  sdecoders.c  tight.c)
set(HEADER global.h main.h  utils.h  handshake.h  d3des.h  logerr.h  stream.h  vncmsg.h  fb.h  mouse.h  sdecoders.h  tight.h)

#set(SOURCE          main.c  utils.c  handshake.c  d3des.c  logerr.c  stream.c  clnt_msg.c  srv_msg.c )
#set(HEADER global.h main.h  utils.h  handshake.h  d3des.h  logerr.h  stream.h  clnt_msg.h  srv_msg.h

add_subdirectory(tst-apps)


find_package(ZLIB)

add_executable(nxvnc ${SOURCE})
target_link_libraries(nxvnc  ${ZLIB_LIBRARIES})