# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(UIsample)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the uWebSockets include directory
find_path(UWEBSOCKETS_INCLUDE_DIRS "uwebsockets/App.h" REQUIRED)

# Find libuv
find_package(uv CONFIG REQUIRED)

# Find OpenSSL
find_package(OpenSSL CONFIG REQUIRED)

# Find zlib
find_package(ZLIB CONFIG REQUIRED)

# Add executable
add_executable(server server.cpp)

# Include directories
target_include_directories(server PRIVATE ${UWEBSOCKETS_INCLUDE_DIRS})

# Link libraries
target_link_libraries(server PRIVATE uv OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
