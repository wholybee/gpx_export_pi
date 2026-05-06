# GPX Export Plugin for OpenCPN

## Overview

This plugin adds GPX export capabilities to OpenCPN via right-click context
menu items and a toolbar button. Users can select a route or waypoint and save it as a GPX file
with configurable compatibility presets for various chartplotter brands.

## Features

- **Object context menu integration**: Right-click a route or waypoint to access
  "Export this Route to GPX..." or "Export this Waypoint to GPX..."
- **Toolbar button integration**: click the GPX Export toolbar icon to open
  the export dialog with no preselected object
- **Route and waypoint enumeration** via the OpenCPN Plugin API (GUID-based)
- **Compatibility presets** for multiple chartplotter vendors:
  - Generic Marine GPX (safe default)
  - OpenCPN Full-Fidelity GPX
  - Minimal GPX
  - Garmin Safe GPX
  - Raymarine Safe GPX
  - B&G / Navico Safe GPX
  - Furuno Safe GPX
- **Name policies**: preserve, sequential numbering, prefix+sequential, with
  strip/uppercase/length-limit options
- **Validation** before export (coordinate ranges, blank names, empty routes)
- **FTP send destination** with persistent server, username, password, and directory settings saved in OpenCPN configuration
- **Standard GPX 1.1** output with proper XML escaping

## Architecture

The plugin follows the layered architecture from the design guide:

```
OpenCPN Plugin API
    ↓
gpx_export_pi  (plugin main: enumeration, object context menu)
    ↓
ExportRoute / ExportWaypoint  (neutral model, decoupled from API structs)
    ↓
GpxExportDialog  (UI: object selection, preset choice)
    ↓
gpx_writer::Generate()  (GPX XML generation)
    ↓
File save via wxFileDialog or passive-mode FTP upload
```

Key design decisions:
- OpenCPN API objects are converted to plugin-owned `ExportRoute` /
  `ExportWaypoint` structs immediately, keeping all downstream code
  independent of the API version.
- GPX generation is a pure function of `ExportDocument` + `GpxCompatibilityPreset`,
  with no dependency on wxWidgets or OpenCPN.
- Presets are data-driven (`GpxCompatibilityPreset` struct), not hard-coded branches.

## Building

This plugin is designed to be placed inside the OpenCPN source tree at
`plugins/gpx_export_pi/` and compiled as part of the OpenCPN build.

1. Copy or clone this directory into `plugins/gpx_export_pi/` in your
   OpenCPN source tree.
2. Add the following line to the top-level `plugins/CMakeLists.txt`:
   ```cmake
   add_subdirectory(gpx_export_pi)
   ```
3. Rebuild OpenCPN normally:
   ```bash
   cd build
   cmake ..
   cmake --build .
   ```


## Building an Installable Tarball

This package now includes `metadata.xml.in` and a CMake tarball target. After
adding the plugin to OpenCPN's `plugins/CMakeLists.txt` and configuring/building
OpenCPN, build the installable tarball with:

```bash
cmake --build <build> --target gpx_export_pi-tarball --config <same-as-OCPN>
```

The generated file will be placed in the plugin build directory and named like:

```text
gpx_export_pi-0.3.0_<target>-<target-version>.tar.gz
```

For a local Import Plugin install, the tarball contains:

```text
gpx_export_pi-0.3.0_<target>-<target-version>/metadata.xml
gpx_export_pi-0.3.0_<target>-<target-version>/plugins/gpx_export_pi.dll|so|dylib
gpx_export_pi-0.3.0_<target>-<target-version>/plugins/gpx_export_pi/data/gpx_export_pi.svg
```

`metadata.xml.in` uses the display name `GPX Export`, which must remain identical
to `gpx_export_pi::GetCommonName()`. The plugin API version is set to `1.20`,
matching `GetAPIVersionMajor()` and `GetAPIVersionMinor()`.

## File Structure

```
gpx_export_pi/
  CMakeLists.txt
  README.md
  data/
    gpx_export_pi.svg       # Toolbar icon
  src/
    gpx_export_pi.h        # Plugin class (opencpn_plugin_120)
    gpx_export_pi.cpp       # Plugin init, toolbar, object context menu, API enumeration
    export_model.h          # Neutral data model structs
    export_dlg.h            # Export dialog (wxDialog)
    export_dlg.cpp          # Dialog UI, file save logic
    gpx_writer.h            # GPX generation interface
    gpx_writer.cpp          # GPX XML generation
    gpx_preset.h            # Compatibility preset definitions
    gpx_preset.cpp          # Built-in preset configurations
    name_policy.h           # Waypoint naming policy
    name_policy.cpp         # Name transform + uniqueness logic
    xml_util.h              # XML escaping, coordinate formatting
    xml_util.cpp            # XML utility implementations
```

## API Version

Targets OpenCPN Plugin API version 1.20+ (uses `opencpn_plugin_120`).
Uses the following API calls:
- `GetRouteGUIDArray()` / `GetWaypointGUIDArray()`
- `GetRouteExV2_Plugin()` / `GetWaypointExV2_Plugin()`
- `GetActiveRouteGUID()` / `GetActiveWaypointGUID()`
- `AddCanvasContextMenuItemExt()` / `SetCanvasContextMenuItemViz()`
- `InsertPlugInToolSVG()` / `RemovePlugInTool()`
- `GetpSharedDataLocation()`
- `GetOCPNCanvasWindow()`

## Future Work

Per the design guide, planned extensions include:
- Track export
- Multiple object selection
- FTP/SFTP upload destination
- NMEA 0183 sentence output (WPL/RTE)
- NMEA 2000 PGN output
- GPX import with preview
- Chart context-menu object identification (select the object under cursor)

## License

GPLv2+, consistent with OpenCPN.

## OpenCPN Full-Fidelity GPX preset

The **OpenCPN Full-Fidelity GPX** preset now writes the OpenCPN/Garmin extension fields normally present in OpenCPN route exports, including:

- GPX root namespaces for `opencpn` and Garmin `gpxx` extensions.
- Route extensions: `opencpn:guid`, `opencpn:viz`, `opencpn:start`, `opencpn:end`, `opencpn:planned_speed`, `opencpn:planned_departure`, `opencpn:time_display`, and `gpxx:RouteExtension/gpxx:IsAutoNamed`.
- Waypoint/route-point fields: high-precision latitude/longitude, `time`, `name`, `sym`, and `type`.
- Waypoint/route-point extensions: `opencpn:guid`, `opencpn:viz_name`, `opencpn:shared`, `opencpn:arrival_radius`, `opencpn:waypoint_range_rings`, and `opencpn:scale_min_max`.

Most values are populated from `PlugIn_Route_ExV2` and `PlugIn_Waypoint_ExV2`. Fields not exposed by the current plugin API are emitted using OpenCPN-compatible defaults where appropriate.
