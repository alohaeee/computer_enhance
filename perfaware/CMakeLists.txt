cmake_minimum_required(VERSION 3.20)
project(perfaware)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sim86_lib STATIC sim86/sim86_lib.cpp)
target_include_directories(sim86_lib PUBLIC sim86)

add_executable(my_exec myexec/main.cpp)
target_include_directories(my_exec PUBLIC myexec)
target_link_libraries(my_exec PUBLIC sim86_lib)


add_custom_command(TARGET my_exec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/my_exec ${CMAKE_SOURCE_DIR}/my_exec
)
