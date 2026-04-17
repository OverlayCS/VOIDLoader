cmake_minimum_required(VERSION 3.20)
project(VoidLoader)

add_definitions(-D_WIN32_WINNT=0x0A00)

add_library(version SHARED src/dllmain.cpp)

target_compile_definitions(version PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

set_property(TARGET version PROPERTY CXX_STANDARD 17)

set_target_properties(version PROPERTIES
    OUTPUT_NAME "version"
    PREFIX ""
    SUFFIX ".dll"
)