cmake_minimum_required(VERSION 3.6.0)
project(sinus VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()

add_executable(cmorse main.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
