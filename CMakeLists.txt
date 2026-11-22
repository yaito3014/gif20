cmake_minimum_required(VERSION 3.23)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(YK_GIF20_IS_TOP_LEVEL ON)
else()
  set(YK_GIF20_IS_TOP_LEVEL OFF)
endif()

option(YK_GIF20_BUILD_TESTING "build tests" ${YK_GIF20_IS_TOP_LEVEL})

# Takes effect only when this is the first project(); a parent project, or a
# user passing their own CMAKE_PROJECT_TOP_LEVEL_INCLUDES, provides lzw20 and
# Catch2 itself
if(YK_GIF20_IS_TOP_LEVEL AND NOT DEFINED CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake")
endif()

project(yk-gif20 VERSION 0.1.0 LANGUAGES CXX)

find_package(lzw20 0.2 CONFIG REQUIRED)

add_library(gif20 INTERFACE)
add_library(yk::gif20 ALIAS gif20)

target_compile_features(gif20 INTERFACE cxx_std_20)
target_link_libraries(gif20 INTERFACE yk::lzw20)

file(GLOB_RECURSE YK_GIF20_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

target_sources(
    gif20
    INTERFACE
        FILE_SET yk_gif20_headers
            TYPE HEADERS
            BASE_DIRS include
            FILES ${YK_GIF20_HEADERS}
)

if(YK_GIF20_BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

option(YK_GIF20_INSTALL "generate install rules for gif20" ${PROJECT_IS_TOP_LEVEL})

if(YK_GIF20_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)

  install(TARGETS gif20 EXPORT gif20_targets FILE_SET yk_gif20_headers)
  install(
      EXPORT gif20_targets
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gif20
      FILE gif20-targets.cmake
      NAMESPACE yk::
  )

  configure_package_config_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gif20-config.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/gif20-config.cmake
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gif20
  )
  # pre-1.0, a minor version may break the API
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/gif20-config-version.cmake
      COMPATIBILITY SameMinorVersion
      ARCH_INDEPENDENT
  )
  install(
      FILES
          ${CMAKE_CURRENT_BINARY_DIR}/gif20-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/gif20-config-version.cmake
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gif20
  )
endif()
