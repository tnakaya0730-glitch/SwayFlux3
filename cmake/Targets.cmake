find_package(Threads REQUIRED)
add_library(swayflux3_core STATIC
    src/core/runtime.cpp src/core/scheduler.cpp src/core/job_context.cpp src/core/job_handle.cpp)
add_library(SwayFlux3::core ALIAS swayflux3_core)
set_target_properties(swayflux3_core PROPERTIES EXPORT_NAME core)
target_compile_features(swayflux3_core PUBLIC cxx_std_17)
target_include_directories(swayflux3_core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(swayflux3_core PUBLIC Threads::Threads)
swayflux3_configure_compiler(swayflux3_core)

add_library(swayflux3_communication STATIC src/communication/file_download_sink.cpp)
add_library(SwayFlux3::communication ALIAS swayflux3_communication)
set_target_properties(swayflux3_communication PROPERTIES EXPORT_NAME communication)
target_link_libraries(swayflux3_communication PUBLIC SwayFlux3::core)
swayflux3_configure_compiler(swayflux3_communication)

add_library(swayflux3 INTERFACE)
add_library(SwayFlux3::swayflux3 ALIAS swayflux3)
target_link_libraries(swayflux3 INTERFACE SwayFlux3::communication)
if(SWAYFLUX3_BUILD_HTTP)
    find_package(CURL 8.0 REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    if(TARGET zstd::libzstd_static)
        set(SWAYFLUX3_ZSTD_TARGET zstd::libzstd_static)
    else()
        set(SWAYFLUX3_ZSTD_TARGET zstd::libzstd_shared)
    endif()
    add_library(swayflux3_http STATIC
        src/http/curl_session.cpp src/http/http_options.cpp src/http/http_request.cpp
        src/http/http_callbacks.cpp src/http/http_transfer.cpp src/http/http_response_parser.cpp
        src/http/redirect_controller.cpp src/http/http_proxy.cpp src/compression/upload_stream.cpp)
    add_library(SwayFlux3::http ALIAS swayflux3_http)
    set_target_properties(swayflux3_http PROPERTIES EXPORT_NAME http)
    target_include_directories(swayflux3_http PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(swayflux3_http PUBLIC SwayFlux3::communication
        PRIVATE CURL::libcurl ZLIB::ZLIB ${SWAYFLUX3_ZSTD_TARGET})
    target_link_libraries(swayflux3 INTERFACE SwayFlux3::http)
    swayflux3_configure_compiler(swayflux3_http)
endif()
