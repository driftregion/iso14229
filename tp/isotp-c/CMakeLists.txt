cmake_minimum_required(VERSION 3.10)

###
# Project definition
###
project(isotp LANGUAGES C)

###
# Get all include directories
###
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

###
# Compile isotp as Shared Lib
###
add_library(isotp SHARED
            isotp.c)