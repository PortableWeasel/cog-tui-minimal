# Set every package variable before CPack snapshots its configuration.
set(CPACK_PACKAGE_NAME "cog-tui-minimal")
set(CPACK_PACKAGE_VENDOR "Cog TUI contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight C11/readline terminal UI for Cog")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-source")
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_CHECKSUM SHA256)
if(NOT CPACK_GENERATOR)
 set(CPACK_GENERATOR TGZ)
endif()
if(NOT CPACK_SOURCE_GENERATOR)
 set(CPACK_SOURCE_GENERATOR TGZ)
endif()
# An allowlisted source staging tree prevents local build output, installs,
# configuration files, credentials and Git metadata from entering an archive.
configure_file("${CMAKE_CURRENT_LIST_DIR}/CPackSource.cmake.in"
 "${PROJECT_BINARY_DIR}/CPackSourceStage.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CPackSourceStage.cmake")
include(CPack)
