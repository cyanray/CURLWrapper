cmake_minimum_required (VERSION 3.1)

project (CURLWrapper)

set(CMAKE_CXX_STANDARD 11)

find_package(CURL CONFIG REQUIRED)


add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

option(CURLWrapper_BUILD_SAMPLES "Build examples" ON)
if(CURLWrapper_BUILD_SAMPLES)
    add_subdirectory(examples)
endif()
