# ~~~
# Summary:      Local, non-generic plugin setup
# Copyright (c) 2020-2021 Mike Rossiter
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.


# -------- Options ----------

set(OCPN_TEST_REPO
    "workspace-for-warren-085x/gpx_export_pi-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "workspace-for-warren-085x/gpx_export_pi-beta"
    CACHE STRING
    "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "workspace-for-warren-085x/gpx_export_pi-prod"
    CACHE STRING
    "Default repository for tagged builds not matching 'beta'"
)

#
#
# -------  Plugin setup --------
#
set(PKG_NAME gpx_export_pi)
set(PKG_VERSION 0.3.8)
set(PKG_PRERELEASE "")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME "GPX Export")    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME gpx_export_pi) # As of GetCommonName() in plugin API
set(PKG_SUMMARY "Export GPX to File or FTP")
set(PKG_DESCRIPTION [=[
	Export OpenCPN routes and waypoints to GPX files using configurable compatibility presets for marine chartplotters and navigation software.
]=])

set(PKG_AUTHOR "Warren Holybee")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/wholybee/gpx_export_pi)
set(PKG_INFO_URL https://github.com/wholybee/gpx_export_pi)

set(SRC
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

set(PKG_API_LIB api-20)  #  A dir in opencpn-libs/ e. g., api-17 or api-16

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.
  if (APPLE)
    target_compile_definitions(${PACKAGE_NAME} PUBLIC OCPN_GHC_FILESYSTEM)
  endif ()
endmacro ()

macro(add_plugin_libraries)
  # Add libraries required by this plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/libs/std_filesystem")
  target_link_libraries(${PACKAGE_NAME} ocpn::filesystem)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml")
  target_link_libraries(${PACKAGE_NAME} ocpn::tinyxml)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/plugin_dc")
  target_link_libraries(${PACKAGE_NAME} ocpn::plugin-dc)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
  target_link_libraries(${PACKAGE_NAME} ocpn::jsoncpp)

  # The wxsvg library enables SVG overall in the plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxsvg")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxsvg)
endmacro ()
