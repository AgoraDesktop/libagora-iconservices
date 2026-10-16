#===----------------------------------------------------------------------===#
#
# Agora's build for Icon Services (libagora-iconservices).
#
# APPLE'S ICON SERVICES IS AN APPLICATION SERVICES FRAMEWORK: it sits ABOVE Core Graphics (a raster comes out
# as a `CGImageRef`) and calls ACROSS to Launch Services for the type model (a generic icon is chosen by
# UTI). So this target depends on `libagora-drawing` and `libagora-launchservices` and on Core Foundation,
# and it links no decoder of its own: PNG is `CGImageCreateWithPNGDataProvider`, which `libagora-drawing`
# already has.
#
# THE MODEL IS APPLE'S AND NOTHING FOREIGN IS READ. An item's icon is its application's or its type's
# generic; the generics are DRAWN rather than shipped as assets; the `.icns` reader is in-tree. See README.md.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.24)

project(AgoraIconServices LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_POSITION_INDEPENDENT_CODE YES)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(CoreFoundation CONFIG REQUIRED)
find_package(AgoraDrawing CONFIG REQUIRED)
find_package(AgoraLaunchServices CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Include shim: consumers write <IconServices/...>.
# ---------------------------------------------------------------------------
set(_agora_include_dir "${CMAKE_BINARY_DIR}/agora-include")
file(REMOVE_RECURSE "${_agora_include_dir}")
file(MAKE_DIRECTORY "${_agora_include_dir}")
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}" "${_agora_include_dir}/IconServices" SYMBOLIC)

file(GLOB IS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

if(NOT IS_SOURCES)
  message(FATAL_ERROR "libagora-iconservices has no implementation files.")
endif()

add_library(IconServices SHARED ${IS_SOURCES})

set_target_properties(IconServices PROPERTIES OUTPUT_NAME agora-iconservices)

target_compile_options(IconServices PRIVATE -fconstant-cfstrings)

target_include_directories(IconServices
    PUBLIC
        "$<BUILD_INTERFACE:${_agora_include_dir}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")

# CORE GRAPHICS AND LAUNCH SERVICES ARE PUBLIC: a consumer names the raster as a `CGImageRef` and the generic
# as a UTI, so it reaches both itself.
target_link_libraries(IconServices
    PUBLIC
        CoreFoundation::CoreFoundation
        AgoraDrawing::Drawing
        AgoraLaunchServices::LaunchServices)

enable_testing()
add_subdirectory(tests)

install(DIRECTORY . DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/IconServices
        FILES_MATCHING PATTERN "*.h" PATTERN "*_internal.h" EXCLUDE)
install(TARGETS IconServices
    EXPORT AgoraIconServicesTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)

install(EXPORT AgoraIconServicesTargets
    FILE AgoraIconServicesTargets.cmake
    NAMESPACE AgoraIconServices::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AgoraIconServices)

configure_package_config_file(cmake/AgoraIconServicesConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AgoraIconServicesConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AgoraIconServices)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AgoraIconServicesConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AgoraIconServicesConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AgoraIconServicesConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AgoraIconServices)
