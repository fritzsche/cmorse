cmake_minimum_required(VERSION 3.6.0)
project(cmake VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()



add_executable(cmorse main.c)

if(UNIX AND NOT APPLE)
 message(STATUS ">>> Linux")
TARGET_LINK_LIBRARIES(cmorse m)

# target_link_libraries(${PROJECT_NAME} m)
endif ()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)



