cmake_minimum_required(VERSION 3.10)

project(aoa_simple)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR}/src DIR_SRCS)

add_executable(${PROJECT_NAME} ${DIR_SRCS})

target_link_libraries(${PROJECT_NAME} usb-1.0)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

