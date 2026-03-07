cmake_minimum_required(VERSION 4.1)
project(final_project C)

set(CMAKE_C_STANDARD 11)

# OpenMP configuration for Apple Silicon
set(OpenMP_C_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
set(OpenMP_C_LIB_NAMES "omp")
set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")

find_package(OpenMP REQUIRED)

# ==========================================
# Target 1: The Main Simulation
# ==========================================
add_executable(final_project
        src/main.c
        src/entities.c
        src/algorithms.c)

target_link_libraries(final_project PRIVATE OpenMP::OpenMP_C)
target_include_directories(final_project PRIVATE /opt/homebrew/opt/libomp/include)

# ==========================================
# Target 2: The Algorithms Tester
# ==========================================
add_executable(algo_tests
        tests/AlgoTests.c
        src/algorithms.c
        src/entities.c)

target_link_libraries(algo_tests PRIVATE OpenMP::OpenMP_C)
target_include_directories(algo_tests PRIVATE /opt/homebrew/opt/libomp/include)

# ==========================================
# Target 3: The Produce Bread Tester
# ==========================================
add_executable(produce_tests
        tests/ProduceBreadTests.c
        src/main.c
        src/entities.c)

target_link_libraries(produce_tests PRIVATE OpenMP::OpenMP_C)
target_include_directories(produce_tests PRIVATE /opt/homebrew/opt/libomp/include)

include_directories("/opt/homebrew/opt/libomp/include")