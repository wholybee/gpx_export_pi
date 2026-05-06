# =============================================================================
# CMakeLists.txt for the GPX Export OpenCPN plugin, in-tree build.
#
# Expected layout:
#   <opencpn-source>/plugins/gpx_export_pi/CMakeLists.txt
#   <opencpn-source>/plugins/gpx_export_pi/metadata.xml.in
#   <opencpn-source>/plugins/gpx_export_pi/src/...
#
# Activate the plugin in the OpenCPN build by adding this line to
#   <opencpn-source>/plugins/CMakeLists.txt:
#       add_subdirectory(gpx_export_pi)
#
# Build the OpenCPN-installable tarball with:
#   cmake --build <build> --target gpx_export_pi-tarball --config <same-as-OCPN>
# =============================================================================

cmake_minimum_required(VERSION 3.10)
project(gpx_export_pi VERSION 0.3.6)

# File-side name. Used for the DLL/shared-library filename and tarball layout.
set(PLUGIN_NAME gpx_export_pi)

# Display name. This MUST match gpx_export_pi::GetCommonName() exactly.
set(PLUGIN_DISPLAY_NAME "GPX Export")

set(PLUGIN_VERSION ${PROJECT_VERSION})
set(OCPN_API_VERSION "1.20")

message(STATUS "")
message(STATUS "*** Configuring plugin: ${PLUGIN_NAME} ***")

# OpenCPN's parent build normally defines these package-target variables.
# The defaults below make the tarball target usable in simpler in-tree builds,
# but for official distribution the values should come from OpenCPN's build.
if(NOT DEFINED PKG_TARGET)
  if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(PKG_TARGET "msvc-64")
    else()
      set(PKG_TARGET "msvc")
    endif()
  elseif(APPLE)
    set(PKG_TARGET "darwin")
  else()
    set(PKG_TARGET "linux")
  endif()
endif()

if(NOT DEFINED PKG_TARGET_VERSION)
  set(PKG_TARGET_VERSION "${CMAKE_SYSTEM_VERSION}")
endif()

if(NOT DEFINED PKG_TARGET_ARCH)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PKG_TARGET_ARCH "x86_64")
  else()
    set(PKG_TARGET_ARCH "x86")
  endif()
endif()

set(SRC_GPX_EXPORT
  src/gpx_export_pi.h
  src/gpx_export_pi.cpp
  src/export_model.h
  src/gpx_writer.h
  src/gpx_writer.cpp
  src/xml_util.h
  src/xml_util.cpp
  src/name_policy.h
  src/name_policy.cpp
  src/gpx_preset.h
  src/gpx_preset.cpp
  src/export_dlg.h
  src/export_dlg.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC_GPX_EXPORT})

# Drop the "lib" prefix on Unix-like platforms so the file is named
# gpx_export_pi.so rather than libgpx_export_pi.so, matching OpenCPN plugins.
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# --- Include paths -----------------------------------------------------------
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# model/include exists in newer OpenCPN builds.
if(EXISTS "${CMAKE_SOURCE_DIR}/model/include")
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/model/include)
endif()

if(DEFINED wxWidgets_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PRIVATE ${wxWidgets_INCLUDE_DIRS})
endif()

# --- Compile definitions / language level -----------------------------------
# MAKING_PLUGIN tells ocpn_plugin.h that this target is building a plugin DLL.
target_compile_definitions(${PROJECT_NAME} PRIVATE MAKING_PLUGIN)

if(DEFINED wxWidgets_DEFINITIONS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ${wxWidgets_DEFINITIONS})
endif()

# std::optional is used in the data model; require C++17.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if(NOT MSVC)
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fvisibility=hidden")
endif()

if(MSVC)
  # Put the DLL in per-config directories next to opencpn.exe for convenient
  # local testing from the OpenCPN build tree.
  set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/MinSizeRel"
  )
endif()

# --- Link dependencies -------------------------------------------------------
if(DEFINED wxWidgets_LIBRARIES)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${wxWidgets_LIBRARIES})
endif()

# OpenCPN targets differ across versions/forks. Prefer exported/interface
# targets where available, then fall back to concrete library targets.
if(TARGET ocpn::opencpn)
  message(STATUS "${PLUGIN_NAME}: linking against ocpn::opencpn")
  target_link_libraries(${PROJECT_NAME} PRIVATE ocpn::opencpn)
elseif(TARGET _opencpn)
  message(STATUS "${PLUGIN_NAME}: linking against _opencpn")
  target_link_libraries(${PROJECT_NAME} PRIVATE _opencpn)
elseif(TARGET opencpn)
  message(STATUS "${PLUGIN_NAME}: linking against opencpn")
  target_link_libraries(${PROJECT_NAME} PRIVATE opencpn)
elseif(DEFINED PACKAGE_NAME AND TARGET ${PACKAGE_NAME})
  message(STATUS "${PLUGIN_NAME}: linking against ${PACKAGE_NAME}")
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PACKAGE_NAME})
else()
  message(WARNING
    "${PLUGIN_NAME}: Could not find ocpn::opencpn, _opencpn, opencpn, or "
    "PACKAGE_NAME targets. The plugin may fail to link on platforms which "
    "require unresolved OpenCPN plugin API symbols to be resolved at link time."
  )
endif()

# --- Configure metadata.xml from template -----------------------------------
set(METADATA_XML "${CMAKE_CURRENT_BINARY_DIR}/metadata.xml")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/metadata.xml.in"
  "${METADATA_XML}"
  @ONLY
)

# --- Build an OpenCPN Import Plugin tarball ---------------------------------
# Layout produced:
#   gpx_export_pi-<version>_<target>-<target-version>/metadata.xml
#   gpx_export_pi-<version>_<target>-<target-version>/plugins/gpx_export_pi.dll|so|dylib
#   gpx_export_pi-<version>_<target>-<target-version>/plugins/gpx_export_pi/data/gpx_export_pi.svg
set(TARBALL_BASE "${PLUGIN_NAME}-${PLUGIN_VERSION}_${PKG_TARGET}-${PKG_TARGET_VERSION}")
set(STAGING_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARBALL_BASE}")
set(TARBALL_PATH "${CMAKE_CURRENT_BINARY_DIR}/${TARBALL_BASE}.tar.gz")

set(PLUGIN_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

add_custom_target(
  ${PLUGIN_NAME}-tarball
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${STAGING_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${STAGING_DIR}/plugins/${PLUGIN_NAME}/data"
  COMMAND ${CMAKE_COMMAND} -E copy "${METADATA_XML}" "${STAGING_DIR}/metadata.xml"
  COMMAND ${CMAKE_COMMAND} -E copy
          "$<TARGET_FILE:${PROJECT_NAME}>"
          "${STAGING_DIR}/plugins/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PLUGIN_DATA_DIR}"
          "${STAGING_DIR}/plugins/${PLUGIN_NAME}/data"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${TARBALL_PATH}"
          --
          "${TARBALL_BASE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  DEPENDS ${PROJECT_NAME}
  COMMENT "Building plugin tarball: ${TARBALL_PATH}"
  VERBATIM
)

message(STATUS "${PLUGIN_NAME}: configuration complete")
message(STATUS "")
