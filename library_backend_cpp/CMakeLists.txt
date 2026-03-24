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

add_executable(library_backend_console
    src/console_menu.cpp
    src/library_backend.cpp
)

target_include_directories(library_backend PRIVATE src)
target_include_directories(library_backend_console PRIVATE src)
target_link_libraries(library_backend PRIVATE SQLite::SQLite3)
target_link_libraries(library_backend_console PRIVATE SQLite::SQLite3)

set_target_properties(library_backend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

set_target_properties(library_backend_console PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

if(WIN32)
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/copy_runtime_dlls_backend_$<CONFIG>.cmake"
        CONTENT [=[
set(runtime_dlls "$<JOIN:$<TARGET_RUNTIME_DLLS:library_backend>,;>")
if(NOT runtime_dlls STREQUAL "")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_dlls} "$<TARGET_FILE_DIR:library_backend>"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
]=]
    )

    add_custom_command(TARGET library_backend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/copy_runtime_dlls_backend_$<CONFIG>.cmake"
    )

    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/copy_runtime_dlls_console_$<CONFIG>.cmake"
        CONTENT [=[
set(runtime_dlls "$<JOIN:$<TARGET_RUNTIME_DLLS:library_backend_console>,;>")
if(NOT runtime_dlls STREQUAL "")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_dlls} "$<TARGET_FILE_DIR:library_backend_console>"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
]=]
    )

    add_custom_command(TARGET library_backend_console POST_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/copy_runtime_dlls_console_$<CONFIG>.cmake"
    )
endif()
