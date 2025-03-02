cmake_minimum_required(VERSION 3.10)

# Set the project name
project(MyCProject)

find_package(cJSON REQUIRED)
find_package(unity REQUIRED)

# Add the subdirectory for the memory library
add_subdirectory(lib/memory)
add_subdirectory(shell/metricas)
add_subdirectory(tests)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shell/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/memory/include)

# Add the main executable
add_executable(main shell/src/main.c)

# Link the memory library with the main executable
target_link_libraries(main memory cjson::cjson)

enable_testing()