cmake_minimum_required(VERSION 3.16)
project(KernelNet VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(CURL REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

# Собираем только файлы игры, без инструментов и тестов
file(GLOB_RECURSE SOURCES "src/game/*.cpp" "src/game/*.c" "src/base/*.cpp" "src/base/*.c" "src/engine/*.cpp" "src/engine/*.c")

add_executable(DDNet ${SOURCES})

target_link_libraries(DDNet PRIVATE 
    SDL2::SDL2 
    CURL::libcurl 
    Freetype::Freetype 
    PNG::PNG 
    OpenSSL::SSL 
    SQLite::SQLite3
    ws2_32 winmm
)

target_include_directories(DDNet PRIVATE src)
