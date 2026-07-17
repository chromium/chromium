cmake_minimum_required(VERSION 3.5)

project(cmake-example
  VERSION 1.0
  LANGUAGES C)

find_package(libzip 1.10 REQUIRED)

add_executable(cmake-example cmake-example.c)
target_link_libraries(cmake-example PRIVATE libzip::zip)
