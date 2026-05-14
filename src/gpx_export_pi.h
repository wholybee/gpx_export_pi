/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Main plugin class                                                     *
 *                                                                         *
 *   Provides context-menu-driven GPX export of routes and waypoints       *
 *   using the OpenCPN Plugin API.                                         *
 **************************************************************************/
 /*************************************************************************
 * Copyright (c) 2026 Warren Holybee                                      *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 3 of the License, or      *
 * (at your option) any later version.                                    *
 *************************************************************************/
 
#ifndef _GPX_EXPORT_PI_H_
#define _GPX_EXPORT_PI_H_

#include "ocpn_plugin.h"

#include <string>
#include <vector>

#include "export_model.h"

#define GPX_VERSION_MAJOR 0
#define GPX_VERSION_MINOR 9
#define GPX_VERSION_PATCH 1
// ---------------------------------------------------------------------------
// Plugin class
// ---------------------------------------------------------------------------

class gpx_export_pi : public opencpn_plugin_120 {
public:
  gpx_export_pi(void* ppimgr);
  ~gpx_export_pi() override;

  // --- Required plugin interface ---
  int Init() override;
  bool DeInit() override;
  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;
  wxBitmap* GetPlugInBitmap() override;
  int GetToolbarToolCount() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

  // --- Context menu ---
  void OnContextMenuItemCallback(int id) override;
  void OnContextMenuItemCallbackExt(int id, std::string obj_ident,
                                    std::string obj_type, double lat,
                                    double lon) override;

  // --- Toolbar ---
  void OnToolbarToolCallback(int id) override;

private:
  /** Enumerate all routes via the plugin API and convert to neutral model. */
  std::vector<ExportRoute> EnumerateRoutes();

  /** Enumerate all standalone waypoints via the plugin API. */
  std::vector<ExportWaypoint> EnumerateWaypoints();

  /** Find an enumerated object by GUID. */
  int FindRouteIndexByGuid(const std::vector<ExportRoute>& routes,
                           const wxString& guid) const;
  int FindWaypointIndexByGuid(const std::vector<ExportWaypoint>& waypoints,
                              const wxString& guid) const;

  /** Show the export dialog and perform the export. */
  void DoExport(int preselect_route = -1, int preselect_wp = -1);

  void* m_ppimgr;
  int m_context_menu_route_id;
  int m_context_menu_waypoint_id;
  int m_toolbar_id;
  wxString m_shareLocn;
  wxBitmap m_panelBitmap;
};

#endif  // _GPX_EXPORT_PI_H_
