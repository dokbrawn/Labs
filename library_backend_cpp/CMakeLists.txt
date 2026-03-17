cmake_minimum_required(VERSION 3.16)
project(LibraryBackendCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(library_backend
    src/main.cpp
    src/library_backend.cpp
)

target_include_directories(library_backend PRIVATE src)
