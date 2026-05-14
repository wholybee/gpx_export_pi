/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Main plugin class - implementation                                    *
 **************************************************************************/
 /*************************************************************************
 * Copyright (c) 2026 Warren Holybee                                      *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 3 of the License, or      *
 * (at your option) any later version.                                    *
 *************************************************************************/
 
#include "gpx_export_pi.h"

#include <wx/filename.h>
#include <wx/fileconf.h>
#include <wx/filefn.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>

#include <cstdio>
#include <memory>
#include <set>

#include "export_dlg.h"
#include "export_model.h"

// ---------------------------------------------------------------------------
// Plugin factory functions required by OpenCPN
//
// These MUST be exported from the plugin DLL.  We cannot rely on DECL_EXP
// because the _opencpn interface target may redefine it to dllimport or
// empty.  Use an explicit export attribute instead.
// ---------------------------------------------------------------------------

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
#define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define PLUGIN_EXPORT extern "C"
#endif

PLUGIN_EXPORT opencpn_plugin* create_pi(void* ppimgr) {
  return new gpx_export_pi(ppimgr);
}

PLUGIN_EXPORT void destroy_pi(opencpn_plugin* p) { delete p; }

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

gpx_export_pi::gpx_export_pi(void* ppimgr)
    : opencpn_plugin_120(ppimgr),
      m_ppimgr(ppimgr),
      m_context_menu_route_id(-1),
      m_context_menu_waypoint_id(-1),
      m_toolbar_id(-1) {}

gpx_export_pi::~gpx_export_pi() {}

// ---------------------------------------------------------------------------
// Plugin interface
// ---------------------------------------------------------------------------

int gpx_export_pi::Init() {
  // Add entries to OpenCPN's object-specific context menus instead of the
  // generic chart context menu.  OpenCPN will show these only when the user
  // right-clicks the matching object type, and will pass that object's GUID to
  // OnContextMenuItemCallbackExt().
  wxMenuItem* pmi_route =
      new wxMenuItem(nullptr, -1, wxT("Export this Route to GPX..."));
  m_context_menu_route_id = AddCanvasContextMenuItemExt(pmi_route, this, "Route");
  SetCanvasContextMenuItemViz(m_context_menu_route_id, true);

  wxMenuItem* pmi_waypoint =
      new wxMenuItem(nullptr, -1, wxT("Export this Waypoint to GPX..."));
  m_context_menu_waypoint_id =
      AddCanvasContextMenuItemExt(pmi_waypoint, this, "Waypoint");
  SetCanvasContextMenuItemViz(m_context_menu_waypoint_id, true);

  // Add a normal toolbar button as an alternate entry point.  When launched
  // from the toolbar there is no chart object context, so the dialog opens
  // with no preselected route or waypoint.
  //
  // Important: for Plugin Manager/imported tarball installs, plugin data is
  // not necessarily under GetpSharedDataLocation().  Use GetPluginDataDir()
  // first so OpenCPN can find this plugin's relocated read-only data folder.
  wxString plugin_data_dir = GetPluginDataDir("gpx_export_pi");
  if (!plugin_data_dir.IsEmpty()) {
    wxFileName data_dir(plugin_data_dir, wxEmptyString);
    data_dir.AppendDir(wxT("data"));
    m_shareLocn = data_dir.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
  }

  // Fallback for manual/in-tree installs where the plugin data may still live
  // in OpenCPN's traditional shared-data tree.
  if (m_shareLocn.IsEmpty()) {
    wxString* shared_data = GetpSharedDataLocation();
    if (shared_data) {
      wxFileName data_dir(*shared_data, wxEmptyString);
      data_dir.AppendDir(wxT("plugins"));
      data_dir.AppendDir(wxT("gpx_export_pi"));
      data_dir.AppendDir(wxT("data"));
      m_shareLocn = data_dir.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
    }
  }

  wxString icon_path = m_shareLocn + wxT("gpx_export_pi.svg");
  if (!wxFileExists(icon_path)) {
    wxLogWarning(wxT("GPX Export: toolbar icon not found: %s"), icon_path);
  } else {
    wxLogMessage(wxT("GPX Export: using toolbar icon: %s"), icon_path);
  }

  wxString icon_path_light = m_shareLocn + wxT("gpx_export_pi-light.svg");
  if (!wxFileExists(icon_path)) {
    wxLogWarning(wxT("GPX Export: toolbar icon not found: %s"), icon_path);
  } else {
    wxLogMessage(wxT("GPX Export: using toolbar icon: %s"), icon_path);
  }

  m_panelBitmap = GetBitmapFromSVGFile(icon_path_light, 32, 32);

  m_toolbar_id = InsertPlugInToolSVG(
      wxT("GPX Export"), icon_path, icon_path, icon_path, wxITEM_NORMAL,
      wxT("GPX Export"),
      wxT("Export OpenCPN routes and waypoints to GPX files"), nullptr, -1, 0,
      this);
  if (m_toolbar_id >= 0) {
    SetToolbarToolViz(m_toolbar_id, true);
  } else {
    wxLogWarning(wxT("GPX Export: InsertPlugInToolSVG failed for icon: %s"),
                 icon_path);
  }

  return INSTALLS_CONTEXTMENU_ITEMS | INSTALLS_TOOLBAR_TOOL |
         WANTS_TOOLBAR_CALLBACK;
}

bool gpx_export_pi::DeInit() {
  if (m_toolbar_id >= 0) {
    RemovePlugInTool(m_toolbar_id);
    m_toolbar_id = -1;
  }

  if (m_context_menu_route_id >= 0) {
    RemoveCanvasContextMenuItem(m_context_menu_route_id);
    m_context_menu_route_id = -1;
  }

  if (m_context_menu_waypoint_id >= 0) {
    RemoveCanvasContextMenuItem(m_context_menu_waypoint_id);
    m_context_menu_waypoint_id = -1;
  }

  return true;
}

int gpx_export_pi::GetAPIVersionMajor() { return 1; }
int gpx_export_pi::GetAPIVersionMinor() { return 20; }
int gpx_export_pi::GetPlugInVersionMajor() { return GPX_VERSION_MAJOR; }
int gpx_export_pi::GetPlugInVersionMinor() { return GPX_VERSION_MINOR; }
int gpx_export_pi::GetPlugInVersionPatch() { return GPX_VERSION_PATCH; }

wxBitmap* gpx_export_pi::GetPlugInBitmap() { return &m_panelBitmap; }

int gpx_export_pi::GetToolbarToolCount() { return 1; }

wxString gpx_export_pi::GetCommonName() { return wxT("gpx_export_pi"); }

wxString gpx_export_pi::GetShortDescription() {
  return wxT("Export routes and waypoints to GPX files.");
}

wxString gpx_export_pi::GetLongDescription() {
  return wxT(
      "GPX Export Plugin for OpenCPN.\n"
      "Right-click a route or waypoint to export it directly\n"
      "as GPX files with configurable compatibility presets\n"
      "for various chartplotter brands.");
}

// ---------------------------------------------------------------------------
// Context menu callback
// ---------------------------------------------------------------------------

void gpx_export_pi::OnContextMenuItemCallback(int id) {
  // Compatibility fallback for older OpenCPN builds which may still dispatch
  // the legacy callback.  There is no object GUID available here, so fall back
  // to OpenCPN's active route/waypoint where possible.
  if (id == m_context_menu_route_id) {
    auto routes = EnumerateRoutes();
    int preselect = FindRouteIndexByGuid(routes, GetActiveRouteGUID());
    auto waypoints = EnumerateWaypoints();
    GpxExportDialog dlg(GetOCPNCanvasWindow(), GetOCPNConfigObject(), routes,
                        waypoints, preselect, -1);
    dlg.ShowModal();
  } else if (id == m_context_menu_waypoint_id) {
    auto waypoints = EnumerateWaypoints();
    int preselect = FindWaypointIndexByGuid(waypoints, GetActiveWaypointGUID());
    auto routes = EnumerateRoutes();
    GpxExportDialog dlg(GetOCPNCanvasWindow(), GetOCPNConfigObject(), routes,
                        waypoints, -1, preselect);
    dlg.ShowModal();
  }
}

void gpx_export_pi::OnContextMenuItemCallbackExt(int id, std::string obj_ident,
                                                 std::string obj_type,
                                                 double lat, double lon) {
  (void)obj_type;
  (void)lat;
  (void)lon;

  wxString selected_guid = wxString::FromUTF8(obj_ident.c_str());

  if (id == m_context_menu_route_id) {
    auto routes = EnumerateRoutes();
    int preselect = FindRouteIndexByGuid(routes, selected_guid);

    // If OpenCPN did not provide an object GUID, keep a sensible fallback for
    // the active route rather than opening the dialog unselected.
    if (preselect < 0) {
      preselect = FindRouteIndexByGuid(routes, GetActiveRouteGUID());
    }

    auto waypoints = EnumerateWaypoints();
    GpxExportDialog dlg(GetOCPNCanvasWindow(), GetOCPNConfigObject(), routes,
                        waypoints, preselect, -1);
    dlg.ShowModal();
  } else if (id == m_context_menu_waypoint_id) {
    auto waypoints = EnumerateWaypoints();
    int preselect = FindWaypointIndexByGuid(waypoints, selected_guid);

    if (preselect < 0) {
      preselect = FindWaypointIndexByGuid(waypoints, GetActiveWaypointGUID());
    }

    auto routes = EnumerateRoutes();
    GpxExportDialog dlg(GetOCPNCanvasWindow(), GetOCPNConfigObject(), routes,
                        waypoints, -1, preselect);
    dlg.ShowModal();
  }
}

void gpx_export_pi::OnToolbarToolCallback(int id) {
  if (id != m_toolbar_id) return;

  // Toolbar button opens the export dialog with no preselection.
  DoExport();
}


int gpx_export_pi::FindRouteIndexByGuid(const std::vector<ExportRoute>& routes,
                                        const wxString& guid) const {
  if (guid.IsEmpty()) return -1;
  for (size_t i = 0; i < routes.size(); ++i) {
    if (wxString::FromUTF8(routes[i].guid.c_str()) == guid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int gpx_export_pi::FindWaypointIndexByGuid(
    const std::vector<ExportWaypoint>& waypoints, const wxString& guid) const {
  if (guid.IsEmpty()) return -1;
  for (size_t i = 0; i < waypoints.size(); ++i) {
    if (wxString::FromUTF8(waypoints[i].guid.c_str()) == guid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Export action
// ---------------------------------------------------------------------------

void gpx_export_pi::DoExport(int preselect_route, int preselect_wp) {
  auto routes = EnumerateRoutes();
  auto waypoints = EnumerateWaypoints();
  GpxExportDialog dlg(GetOCPNCanvasWindow(), GetOCPNConfigObject(), routes,
                      waypoints, preselect_route, preselect_wp);
  dlg.ShowModal();
}


namespace {

std::string WxColourToHtml(const wxColour& colour) {
  if (!colour.IsOk()) return "#FF0000";
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", colour.Red(),
                colour.Green(), colour.Blue());
  return std::string(buf);
}

void PopulateWaypointFromApi(ExportWaypoint& dst, PlugIn_Waypoint_ExV2* src,
                             bool is_route_point) {
  dst.guid = src->m_GUID.ToStdString();
  dst.name = src->m_MarkName.ToStdString();
  dst.description = src->m_MarkDescription.ToStdString();
  dst.symbol = src->IconName.ToStdString();
  dst.type = "WPT";
  dst.latitude = src->m_lat;
  dst.longitude = src->m_lon;
  if (src->m_CreateTime.IsValid()) {
    dst.timestamp = src->m_CreateTime.GetTicks();
  }
  dst.is_route_point = is_route_point;

  dst.is_visible = src->IsVisible;
  dst.is_name_visible = src->IsNameVisible;
  dst.is_shared = is_route_point && src->GetFSStatus();
  dst.use_scale = src->b_useScamin;
  dst.scale_min = src->scamin;
  dst.scale_max = src->scamax;
  dst.range_ring_count = src->nrange_rings;
  dst.range_ring_step = src->RangeRingSpace;
  dst.range_ring_units = src->RangeRingSpaceUnits;
  dst.range_ring_color = WxColourToHtml(src->RangeRingColor);
  dst.show_range_rings = src->m_bShowWaypointRangeRings;
  dst.arrival_radius_nm = src->m_WaypointArrivalRadius;
  if (src->m_PlannedSpeed > 0.0) {
    dst.planned_speed_knots = src->m_PlannedSpeed;
  }
  if (src->m_ETD.IsValid()) {
    dst.planned_departure = src->m_ETD.GetTicks();
  }
  dst.tide_station = src->m_TideStation.ToStdString();
}

}  // namespace

// ---------------------------------------------------------------------------
// Object enumeration helpers
// ---------------------------------------------------------------------------

std::vector<ExportRoute> gpx_export_pi::EnumerateRoutes() {
  std::vector<ExportRoute> result;

  wxArrayString guids = GetRouteGUIDArray();
  wxString active_guid = GetActiveRouteGUID();

  for (size_t i = 0; i < guids.GetCount(); ++i) {
    wxString guid = guids[i];

    // Fetch the full route object from the plugin API.
    std::unique_ptr<PlugIn_Route_ExV2> api_route =
        GetRouteExV2_Plugin(guid);
    if (!api_route) {
      wxLogWarning(wxT("GPX Export: Could not fetch route GUID=%s"), guid);
      continue;
    }

    ExportRoute rte;
    rte.guid = guid.ToStdString();
    rte.name = api_route->m_NameString.ToStdString();
    rte.description = api_route->m_Description.ToStdString();
    rte.is_active = (guid == active_guid);
    rte.is_visible = api_route->m_isVisible;
    rte.start = api_route->m_StartString.ToStdString();
    rte.end = api_route->m_EndString.ToStdString();
    rte.time_display = "GLOBAL SETTING";

    // Iterate over the route's waypoint list.
    auto* wp_list = api_route->pWaypointList;
    if (wp_list) {
      for (auto node = wp_list->GetFirst(); node; node = node->GetNext()) {
        PlugIn_Waypoint_ExV2* api_wp = node->GetData();
        if (!api_wp) continue;

        ExportWaypoint pt;
        PopulateWaypointFromApi(pt, api_wp, true);
        if (!rte.planned_speed_knots.has_value() && pt.planned_speed_knots.has_value()) {
          rte.planned_speed_knots = pt.planned_speed_knots;
        }
        if (!rte.planned_departure.has_value() && pt.planned_departure.has_value()) {
          rte.planned_departure = pt.planned_departure;
        }
        rte.points.push_back(pt);
      }
    }

    result.push_back(rte);
  }

  return result;
}

std::vector<ExportWaypoint> gpx_export_pi::EnumerateWaypoints() {
  std::vector<ExportWaypoint> result;
  std::set<std::string> seen_guids;

  // Request user waypoints from OpenCPN.
  wxArrayString guids = GetWaypointGUIDArray();

  for (size_t i = 0; i < guids.GetCount(); ++i) {
    wxString guid = guids[i];

    std::unique_ptr<PlugIn_Waypoint_ExV2> api_wp =
        GetWaypointExV2_Plugin(guid);
    if (!api_wp) {
      wxLogWarning(wxT("GPX Export: Could not fetch waypoint GUID=%s"), guid);
      continue;
    }

    ExportWaypoint wp;
    PopulateWaypointFromApi(wp, api_wp.get(), false);
    if (seen_guids.insert(wp.guid).second) {
      result.push_back(wp);
    }
  }

  // Object-specific context callbacks may identify route points as waypoint
  // objects.  On some OpenCPN builds those route-point GUIDs are not included
  // by GetWaypointGUIDArray(), so include the points from all routes as
  // selectable waypoint exports as well.
  for (const auto& rte : EnumerateRoutes()) {
    for (const auto& pt : rte.points) {
      if (seen_guids.insert(pt.guid).second) {
        result.push_back(pt);
      }
    }
  }

  return result;
}
