/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Internal neutral model for routes, waypoints, and tracks              *
 *                                                                         *
 *   These structures decouple the rest of the plugin from OpenCPN's       *
 *   plugin API object types, making GPX generation, naming policies,      *
 *   and destinations independent of the API version.                      *
 **************************************************************************/

#ifndef _EXPORT_MODEL_H_
#define _EXPORT_MODEL_H_

#include <ctime>
#include <optional>
#include <string>
#include <vector>

/** A single waypoint, either standalone or part of a route. */
struct ExportWaypoint {
  std::string guid;
  std::string name;
  std::string description;
  std::string symbol;
  std::string type = "WPT";
  double latitude = 0.0;
  double longitude = 0.0;
  std::optional<std::time_t> timestamp;
  bool is_route_point = false;

  // OpenCPN full-fidelity extension fields.
  bool is_visible = true;
  bool is_name_visible = false;
  bool is_shared = false;
  bool use_scale = false;
  double scale_min = 2147483646.0;
  double scale_max = 0.0;
  int range_ring_count = 0;
  double range_ring_step = 1.0;
  int range_ring_units = 0;  // 0:nm, 1:km
  std::string range_ring_color = "#FF0000";
  bool show_range_rings = false;
  double arrival_radius_nm = 0.0;
  std::optional<double> planned_speed_knots;
  std::optional<std::time_t> planned_departure;
  std::string tide_station;
};

/** A route consisting of an ordered list of waypoints. */
struct ExportRoute {
  std::string guid;
  std::string name;
  std::string description;
  bool is_active = false;
  bool is_visible = true;
  std::string start;
  std::string end;
  std::optional<double> planned_speed_knots;
  std::optional<std::time_t> planned_departure;
  std::string time_display;
  bool is_auto_named = false;
  std::vector<ExportWaypoint> points;
};

/** A single track point with optional speed/course. */
struct ExportTrackPoint {
  double latitude = 0.0;
  double longitude = 0.0;
  std::optional<std::time_t> timestamp;
  std::optional<double> sog_knots;
  std::optional<double> cog_degrees;
};

/** A track consisting of an ordered list of track points. */
struct ExportTrack {
  std::string guid;
  std::string name;
  std::string description;
  std::vector<ExportTrackPoint> points;
};

/** A complete export document that can contain multiple objects. */
struct ExportDocument {
  std::vector<ExportWaypoint> waypoints;
  std::vector<ExportRoute> routes;
  std::vector<ExportTrack> tracks;
};

#endif  // _EXPORT_MODEL_H_
