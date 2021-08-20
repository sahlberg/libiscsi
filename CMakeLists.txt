cmake_minimum_required(VERSION 3.0)
project(libiscsi C)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)

add_definitions(-DHAVE_CONFIG_H)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/config.h)
    execute_process(COMMAND ./autogen.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
    execute_process(COMMAND ./configure
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR})

file(GLOB LIBISCSI_SRCS lib/*.c)
add_library(libiscsi_static STATIC ${LIBISCSI_SRCS})

set_target_properties(libiscsi_static PROPERTIES OUTPUT_NAME libiscsi)
set_target_properties(libiscsi_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS libiscsi_static ARCHIVE DESTINATION lib)
file(GLOB LIBISCSI_HEADERS include/*.h)
install(FILES
    config.h
    ${LIBISCSI_HEADERS}
    DESTINATION include
)
