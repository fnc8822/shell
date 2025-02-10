# Verify cmake version
cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# Create Project
project(
    "ShellProject"
    VERSION 1.0.0
    DESCRIPTION "A nice shell project"
    LANGUAGES C
)

# Find dependencies
find_package(cJSON REQUIRED)
find_package(unity REQUIRED)
find_package(libmicrohttpd REQUIRED)

# Enable testing
include(CTest)

# Flags for compiling
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -Werror -Wunused-parameter -Wmissing-prototypes -Wstrict-prototypes -fhardened")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov --coverage")

# Pass PROJECT_DIR
add_definitions(-DPROJECT_DIR=\"${PROJECT_DIR}\")

# Conan integration
include(${CMAKE_BINARY_DIR}/Release/generators/conan_toolchain.cmake)

# Include headers
include_directories(include)

# Add metricas as a subdirectory
add_subdirectory(shell/metricas)

# Include directories for metricas
include_directories(metricas/lib/prom-c-client/prom/include)
include_directories(metricas/lib/prom-c-client/promhttp/include)

# Link directories for metricas
link_directories(${CMAKE_BINARY_DIR}/metricas/lib/prom-c-client/prom)
link_directories(${CMAKE_BINARY_DIR}/metricas/lib/prom-c-client/promhttp)

# Add subdirectories
add_subdirectory(lib/memory)
add_subdirectory(shell)

# Set enviroment variables
add_custom_target(run_with_env
    COMMAND bash -c "export CONFIG_FILE_PATH=${CMAKE_SOURCE_DIR}/shell/config.json && ./shell/ShellProject"
)