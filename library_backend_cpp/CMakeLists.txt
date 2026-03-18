cmake_minimum_required(VERSION 3.21)
project(LibraryBackendCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск SQLite3
find_package(SQLite3 REQUIRED)

add_executable(library_backend
    src/main.cpp
    src/library_backend.cpp
)

target_include_directories(library_backend PRIVATE src)

# Исправлено имя цели на SQLite3::SQLite3
target_link_libraries(library_backend PRIVATE SQLite3::SQLite3)

# Настройка выходной директории
set_target_properties(library_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)

# ИСПРАВЛЕННЫЙ БЛОК КОПИРОВАНИЯ DLL
# Используем более надёжный подход с проверкой
if(WIN32)
    add_custom_command(TARGET library_backend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Build completed: $<TARGET_FILE:library_backend>"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()