cmake_minimum_required(VERSION 3.14)
project(nxvnc C)

set(CMAKE_C_STANDARD 99)

add_executable(simple-srv simple-srv.c)
add_executable(simple-client simple-client.c)


add_executable(fb-tests fb-tests.c)

add_executable(linux_input  linux_input.c)

add_executable(mouse  mouse.c)


find_package(ZLIB)

add_executable(tight_2_fb  tight_2_fb.c)
target_link_libraries(tight_2_fb  ${ZLIB_LIBRARIES})


add_executable(zdata_2fb  zdata_2fb.c)
target_link_libraries(zdata_2fb  ${ZLIB_LIBRARIES})


add_executable(z-decompress  z-decompress.c)
target_link_libraries(z-decompress  ${ZLIB_LIBRARIES})


add_executable(decompress-orig  decompress-orig.c)
target_link_libraries(decompress-orig  ${ZLIB_LIBRARIES})
