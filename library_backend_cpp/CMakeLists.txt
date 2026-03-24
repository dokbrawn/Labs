cmake_minimum_required(VERSION 3.21)
project(LibraryBackendCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

add_executable(library_backend
    src/library_backend.cpp
)

target_include_directories(library_backend PRIVATE src ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(library_backend PRIVATE ${PostgreSQL_LIBRARIES})

set_target_properties(library_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)

if(WIN32)
    add_custom_command(TARGET library_backend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== BUILD COMPLETE ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Binary: $<TARGET_FILE:library_backend>"
        VERBATIM
    )
endif()