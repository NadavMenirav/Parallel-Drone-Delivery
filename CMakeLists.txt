cmake_minimum_required(VERSION 4.1)
project(final_project C)

set(CMAKE_C_STANDARD 11)

set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
set(OpenMP_C_LIB_NAMES "omp")
set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")

find_package(OpenMP REQUIRED)

add_executable(final_project
        src/main.c
        src/entities.c)

target_link_libraries(final_project PRIVATE OpenMP::OpenMP_C)

target_include_directories(final_project PRIVATE /opt/homebrew/opt/libomp/include)