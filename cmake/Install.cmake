include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS swayflux3 swayflux3_core swayflux3_communication EXPORT SwayFlux3Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(SWAYFLUX3_BUILD_HTTP)
    install(TARGETS swayflux3_http EXPORT SwayFlux3Targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SwayFlux3Targets NAMESPACE SwayFlux3:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SwayFlux3)
configure_package_config_file(cmake/SwayFlux3Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SwayFlux3Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SwayFlux3)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/SwayFlux3ConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SwayFlux3Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SwayFlux3ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SwayFlux3)
install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY docs/licenses/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs/licenses)
