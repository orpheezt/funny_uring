cmake_minimum_required(VERSION 3.30)
project(echo_uring LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(opentelemetry-cpp CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(liburing REQUIRED)

#add_library(server)
#target_sources(server
#  PRIVATE
#  server.cpp
#)
#target_link_libraries(server
#  PUBLIC
#  liburing::liburing
#)

add_executable(echo_uring)
target_sources(echo_uring
  PRIVATE
  echo_uring.cpp
)
target_link_libraries(echo_uring
  PRIVATE
  liburing::liburing
)