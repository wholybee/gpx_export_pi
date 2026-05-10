# GPX Export Plugin for OpenCPN

## Overview

This plugin adds GPX export capabilities to OpenCPN via right-click context
menu items and a toolbar button. Users can select a route or waypoint and save it as a GPX file
with configurable compatibility presets for various chartplotter brands.

## Features

* **Object context menu integration**: Right-click a route or waypoint to access
"Export this Route to GPX..." or "Export this Waypoint to GPX..."
* **Toolbar button integration**: click the GPX Export toolbar icon to open
the export dialog with no preselected object
* **Route and waypoint enumeration** via the OpenCPN Plugin API (GUID-based)
* **Compatibility presets** for multiple chartplotter vendors:

  * Generic Marine GPX (safe default)
  * OpenCPN Full-Fidelity GPX
  * Minimal GPX
  * Garmin Safe GPX
  * Raymarine Safe GPX
  * B\&G / Navico Safe GPX
  * Furuno Safe GPX
* **Name policies**: preserve, sequential numbering, prefix+sequential, with
strip/uppercase/length-limit options
* **Validation** before export (coordinate ranges, blank names, empty routes)
* **FTP send destination** with persistent server, username, password, and directory settings saved in OpenCPN configuration
* **Standard GPX 1.1** output with proper XML escaping



## Building an Installable Tarball

cmake --build <build> --target gpx\_export\_pi-tarball --config <same-as-OCPN>

## License

GPLv3, consistent with OpenCPN.

