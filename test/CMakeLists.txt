# cmake -DNCNN_DIR=/home/qf28/cpp_projects/ncnn/build/install/lib/cmake/ncnn -DOCV_DIR=/home/qfeng10/opencv-mobile-4.9.0/build/install/lib/cmake/opencv4 -DTEST_CPP=ON ../test 
cmake_minimum_required(VERSION 3.0.0)
set (CMAKE_CXX_STANDARD 11)
project(detkit VERSION 0.1.0 LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/AddTest.cmake)

configure_file(${CMAKE_CURRENT_LIST_DIR}/../include/detector.h ${CMAKE_CURRENT_BINARY_DIR}/detector.h COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/../include/stb_image.h ${CMAKE_CURRENT_BINARY_DIR}/stb_image.h COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/../include/stb_image_write.h ${CMAKE_CURRENT_BINARY_DIR}/stb_image_write.h COPYONLY)

add_test_c(nanodet_plus nanodet-plus-m_416_int8 416)
add_test_c(fastestdet FastestDet 352)

configure_file(${CMAKE_CURRENT_LIST_DIR}/img/test1.jpg ${CMAKE_CURRENT_BINARY_DIR}/test1.jpg COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/img/test2.jpg ${CMAKE_CURRENT_BINARY_DIR}/test2.jpg COPYONLY)
configure_file(${CMAKE_CURRENT_LIST_DIR}/img/test3.jpg ${CMAKE_CURRENT_BINARY_DIR}/test3.jpg COPYONLY)