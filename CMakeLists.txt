cmake_minimum_required(VERSION 3.24)
project(SwayFlux3 VERSION 0.2.0 LANGUAGES CXX)

option(SWAYFLUX3_BUILD_HTTP "libcurlによるHTTP通信Jobをビルド" ON)
option(SWAYFLUX3_BUILD_EXAMPLES "利用サンプルをビルド" ON)

include(cmake/CompilerOptions.cmake)
include(cmake/Targets.cmake)
if(SWAYFLUX3_BUILD_EXAMPLES)
    include(cmake/Examples.cmake)
endif()
include(cmake/Install.cmake)
