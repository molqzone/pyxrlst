cmake_minimum_required(VERSION 3.12)

project(pyxrlst LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LIBXR_STATIC_BUILD ON CACHE BOOL "" FORCE)
set(LIBXR_SYSTEM linux CACHE STRING "" FORCE)
set(LIBXR_DRIVER linux CACHE STRING "" FORCE)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

if(NOT UNIX OR APPLE)
  message(FATAL_ERROR "pyxrlst currently targets the LibXR Linux shared-topic backend.")
endif()

add_subdirectory(libxr)

pybind11_add_module(pyxrlst MODULE src/bindings.cpp)
target_include_directories(pyxrlst PRIVATE libxr/src libxr/system/linux)
target_link_libraries(pyxrlst PRIVATE xr)
target_compile_definitions(pyxrlst PRIVATE LIBXR_SYSTEM_POSIX_HOST=True)
install(TARGETS pyxrlst LIBRARY DESTINATION .)
