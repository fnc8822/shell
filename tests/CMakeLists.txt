# tests/CMakeLists.txt

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../lib/memory/include)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")


# Add the main source file that contains the definitions of the functions and variables
add_executable(test_executable test_memory.c ../lib/memory/src/memory.c)  # Replace with the correct source file(s)
target_compile_definitions(test_executable PRIVATE TESTING)
target_link_libraries(test_executable PRIVATE unity::unity)
