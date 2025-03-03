# tests/CMakeLists.txt

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fprofile-arcs -ftest-coverage --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov --coverage")

# Add the main source file that contains the definitions of the functions and variables
add_executable(test_executable test_main.c ../src/main.c)  # Replace with the correct source file(s)
target_compile_definitions(test_executable PRIVATE TESTING)
target_link_libraries(test_executable PRIVATE cjson::cjson metricas unity::unity)
