cmake_minimum_required(VERSION 3.0.0)
project(testCopy)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})


AUX_SOURCE_DIRECTORY("${CMAKE_SOURCE_DIR}/src/thread" SRCS) # 只能是目录
AUX_SOURCE_DIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}" TEST_SRCS)

add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/src/copyfile.cpp" ${TEST_SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/googletest-main/googletest/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/googletest-main/googlemock/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} gtest gtest_main)


add_dependencies(${PROJECT_NAME} gtest gtest_main)