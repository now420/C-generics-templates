cmake_minimum_required(VERSION 3.15)
project(CTemplates C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(ctemplate_preproc preprocessor/ctemplate_preproc.cpp)

set(GEN "${CMAKE_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT  ${GEN}/main.c
    COMMAND ctemplate_preproc ${CMAKE_SOURCE_DIR}/src/main.c ${GEN}/main.c
    DEPENDS ${CMAKE_SOURCE_DIR}/src/main.c ctemplate_preproc
    COMMENT "Preprocessing src/main.c ..."
)

add_executable(main ${GEN}/main.c)
add_dependencies(main ctemplate_preproc)
