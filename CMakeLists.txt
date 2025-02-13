cmake_minimum_required(VERSION 3.10.0)
project(cmake VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()

add_executable(cmorse main.c blackman.c midi.c morse.c)

if(UNIX AND NOT APPLE)
TARGET_LINK_LIBRARIES(cmorse m)
endif ()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

#target_compile_definitions(cmorse PUBLIC -DWABUN)

include(CPack)



