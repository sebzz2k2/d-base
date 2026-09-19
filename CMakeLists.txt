cmake_minimum_required(VERSION 4.4)

project(db)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)

add_executable(db ${SOURCES})

target_include_directories(db PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

target_link_libraries(db PRIVATE CLI11::CLI11)
target_link_libraries(db PRIVATE spdlog::spdlog)