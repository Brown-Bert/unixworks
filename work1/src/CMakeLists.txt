cmake_minimum_required(VERSION 3.0.0)
project(work1)
# find_package(Readline REQUIRED)
# 手动指定 readline 库的路径

add_executable(main systemMain.cpp filesystem.cpp copyfile.cpp)
