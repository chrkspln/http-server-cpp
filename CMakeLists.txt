cmake_minimum_required(VERSION 3.13)
project(http-server-starter-cpp)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VCPKG_TARGET_TRIPLET "x64-linux")

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(server ${SOURCE_FILES})

target_link_libraries(server pthread)
target_link_libraries(server ZLIB::ZLIB)
target_link_libraries(server stdc++fs)

include_directories(/usr/include)
link_directories(/usr/lib)

include_directories(/mnt/c/Microsoft/AndroidNDK/android-ndk-r21e/sysroot/usr/include/zlib.h)

target_include_directories(server PRIVATE Threads::Threads)