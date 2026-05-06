/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   GPX file writer - implementation                                      *
 **************************************************************************/

#include "gpx_writer.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "name_policy.h"
#include "xml_util.h"

namespace gpx_writer {

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool Validate(const ExportDocument& doc, const GpxCompatibilityPreset& preset,
              std::vector<std::string>& errors) {
  errors.clear();

  if (doc.waypoints.empty() && doc.routes.empty() && doc.tracks.empty()) {
    errors.push_back("Export document is empty - nothing to export.");
    return false;
  }

  auto check_coord = [&](const std::string& label, double lat, double lon) {
    if (lat < -90.0 || lat > 90.0) {
      errors.push_back(label + ": latitude " + std::to_string(lat) +
                       " out of range [-90, 90].");
    }
    if (lon < -180.0 || lon > 180.0) {
      errors.push_back(label + ": longitude " + std::to_string(lon) +
                       " out of range [-180, 180].");
    }
  };

  for (size_t i = 0; i < doc.waypoints.size(); ++i) {
    const auto& wp = doc.waypoints[i];
    std::string label = "Waypoint[" + std::to_string(i) + "]";
    check_coord(label, wp.latitude, wp.longitude);
    if (preset.require_nonblank_names && wp.name.empty()) {
      errors.push_back(label + ": name is blank.");
    }
  }

  for (size_t r = 0; r < doc.routes.size(); ++r) {
    const auto& rte = doc.routes[r];
    std::string rlabel = "Route[" + std::to_string(r) + "]";
    if (rte.points.size() < 2) {
      errors.push_back(rlabel + ": route has fewer than 2 points.");
    }
    for (size_t p = 0; p < rte.points.size(); ++p) {
      std::string plabel = rlabel + ".point[" + std::to_string(p) + "]";
      check_coord(plabel, rte.points[p].latitude, rte.points[p].longitude);
    }
  }

  for (size_t t = 0; t < doc.tracks.size(); ++t) {
    const auto& trk = doc.tracks[t];
    std::string tlabel = "Track[" + std::to_string(t) + "]";
    if (trk.points.empty()) {
      errors.push_back(tlabel + ": track has no points.");
    }
    for (size_t p = 0; p < trk.points.size(); ++p) {
      std::string plabel = tlabel + ".point[" + std::to_string(p) + "]";
      check_coord(plabel, trk.points[p].latitude, trk.points[p].longitude);
    }
  }

  return errors.empty();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

std::string FormatFixed(double value, int precision) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(precision) << value;
  return ss.str();
}

std::string FormatScale(double value) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(0) << value;
  return ss.str();
}

bool HasOpenCPNWaypointExtensions(const ExportWaypoint& wp) {
  return !wp.guid.empty() || wp.is_name_visible || wp.is_shared ||
         wp.arrival_radius_nm > 0.0 || wp.range_ring_count > 0 ||
         wp.show_range_rings || wp.use_scale || wp.scale_max > 0.0 ||
         wp.scale_min > 0.0;
}

void WriteWaypointExtensions(std::ostringstream& out, const ExportWaypoint& wp,
                             const GpxCompatibilityPreset& preset,
                             const std::string& indent) {
  if (!preset.include_extensions || !preset.include_opencpn_extensions) return;
  if (!HasOpenCPNWaypointExtensions(wp)) return;

  out << indent << "<extensions>\n";
  if (!wp.guid.empty()) {
    out << indent << "  <opencpn:guid>" << xml_util::EscapeXml(wp.guid)
        << "</opencpn:guid>\n";
  }
  if (wp.is_name_visible) {
    out << indent << "  <opencpn:viz_name>1</opencpn:viz_name>\n";
  }
  if (wp.is_shared) {
    out << indent << "  <opencpn:shared>1</opencpn:shared>\n";
  }
  if (wp.arrival_radius_nm > 0.0) {
    out << indent << "  <opencpn:arrival_radius>"
        << FormatFixed(wp.arrival_radius_nm, 3)
        << "</opencpn:arrival_radius>\n";
  }

  out << indent << "  <opencpn:waypoint_range_rings visible=\""
      << (wp.show_range_rings ? "true" : "false") << "\" number=\""
      << wp.range_ring_count << "\" step=\"" << FormatFixed(wp.range_ring_step, 0)
      << "\" units=\"" << wp.range_ring_units << "\" colour=\""
      << xml_util::EscapeXml(wp.range_ring_color) << "\" />\n";

  out << indent << "  <opencpn:scale_min_max UseScale=\""
      << (wp.use_scale ? "true" : "false") << "\" ScaleMin=\""
      << FormatScale(wp.scale_min) << "\" ScaleMax=\""
      << FormatScale(wp.scale_max) << "\" />\n";

  out << indent << "</extensions>\n";
}

void WriteWptElement(std::ostringstream& out, const std::string& tag,
                     const ExportWaypoint& wp, const std::string& disp_name,
                     const GpxCompatibilityPreset& preset,
                     const std::string& indent) {
  out << indent << "<" << tag << " lat=\"" << xml_util::FormatLat(wp.latitude)
      << "\" lon=\"" << xml_util::FormatLon(wp.longitude) << "\">\n";

  if (preset.include_timestamps && wp.timestamp.has_value()) {
    out << indent << "  <time>"
        << xml_util::FormatTimeISO8601(*wp.timestamp) << "</time>\n";
  }

  out << indent << "  <name>" << xml_util::EscapeXml(disp_name) << "</name>\n";

  if (preset.include_descriptions && !wp.description.empty()) {
    out << indent << "  <desc>" << xml_util::EscapeXml(wp.description)
        << "</desc>\n";
  }

  if (preset.include_symbols && !wp.symbol.empty()) {
    out << indent << "  <sym>" << xml_util::EscapeXml(wp.symbol) << "</sym>\n";
  }

  if (preset.include_opencpn_extensions && !wp.type.empty()) {
    out << indent << "  <type>" << xml_util::EscapeXml(wp.type) << "</type>\n";
  }

  WriteWaypointExtensions(out, wp, preset, indent + "  ");

  out << indent << "</" << tag << ">\n";
}

void WriteRouteExtensions(std::ostringstream& out, const ExportRoute& rte,
                          const GpxCompatibilityPreset& preset,
                          const std::string& indent) {
  if (!preset.include_extensions) return;
  if (!preset.include_opencpn_extensions && !preset.include_garmin_extensions)
    return;

  out << indent << "<extensions>\n";
  if (preset.include_opencpn_extensions) {
    if (!rte.guid.empty()) {
      out << indent << "  <opencpn:guid>" << xml_util::EscapeXml(rte.guid)
          << "</opencpn:guid>\n";
    }
    out << indent << "  <opencpn:viz>" << (rte.is_visible ? "1" : "0")
        << "</opencpn:viz>\n";
    if (!rte.start.empty()) {
      out << indent << "  <opencpn:start>" << xml_util::EscapeXml(rte.start)
          << "</opencpn:start>\n";
    }
    if (!rte.end.empty()) {
      out << indent << "  <opencpn:end>" << xml_util::EscapeXml(rte.end)
          << "</opencpn:end>\n";
    }
    if (rte.planned_speed_knots.has_value()) {
      out << indent << "  <opencpn:planned_speed>"
          << FormatFixed(*rte.planned_speed_knots, 2)
          << "</opencpn:planned_speed>\n";
    }
    if (rte.planned_departure.has_value()) {
      out << indent << "  <opencpn:planned_departure>"
          << xml_util::FormatTimeISO8601(*rte.planned_departure)
          << "</opencpn:planned_departure>\n";
    }
    if (!rte.time_display.empty()) {
      out << indent << "  <opencpn:time_display>"
          << xml_util::EscapeXml(rte.time_display)
          << "</opencpn:time_display>\n";
    }
  }

  if (preset.include_garmin_extensions) {
    out << indent << "  <gpxx:RouteExtension>\n";
    out << indent << "    <gpxx:IsAutoNamed>"
        << (rte.is_auto_named ? "true" : "false")
        << "</gpxx:IsAutoNamed>\n";
    out << indent << "  </gpxx:RouteExtension>\n";
  }
  out << indent << "</extensions>\n";
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// GPX generation
// ---------------------------------------------------------------------------

std::string Generate(const ExportDocument& doc,
                     const GpxCompatibilityPreset& preset) {
  std::ostringstream out;

  // --- XML header ---
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<gpx version=\"1.1\" creator=\"OpenCPN GPX Export Plugin\"\n";
  out << "  xmlns=\"http://www.topografix.com/GPX/1/1\"\n";
  out << "  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
  if (preset.include_extensions && preset.include_garmin_extensions) {
    out << "  xmlns:gpxx=\"http://www.garmin.com/xmlschemas/GpxExtensions/v3\"\n";
  }
  if (preset.include_extensions && preset.include_opencpn_extensions) {
    out << "  xmlns:opencpn=\"http://www.opencpn.org\"\n";
  }
  out << "  xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 "
         "https://www.topografix.com/GPX/1/1/gpx.xsd";
  if (preset.include_extensions && preset.include_garmin_extensions) {
    out << " http://www.garmin.com/xmlschemas/GpxExtensions/v3 "
           "http://www8.garmin.com/xmlschemas/GpxExtensionsv3.xsd";
  }
  out << "\">\n";

  // --- Prepare name policy for waypoints ---
  // Collect all waypoint names for batch policy application.
  std::vector<std::string> wp_orig_names;
  for (const auto& wp : doc.waypoints) wp_orig_names.push_back(wp.name);

  std::vector<std::string> wp_display_names = name_policy::ApplyPolicyBatch(
      wp_orig_names, preset.name_policy, preset.require_unique_names);

  // --- Top-level waypoints ---
  for (size_t i = 0; i < doc.waypoints.size(); ++i) {
    WriteWptElement(out, "wpt", doc.waypoints[i], wp_display_names[i], preset,
                    "  ");
  }

  // --- Routes ---
  for (const auto& rte : doc.routes) {
    out << "  <rte>\n";
    if (!rte.name.empty()) {
      out << "    <name>" << xml_util::EscapeXml(rte.name) << "</name>\n";
    }
    if (preset.include_descriptions && !rte.description.empty()) {
      out << "    <desc>" << xml_util::EscapeXml(rte.description)
          << "</desc>\n";
    }

    WriteRouteExtensions(out, rte, preset, "    ");

    // Prepare route-point names.
    std::vector<std::string> rp_orig;
    for (const auto& pt : rte.points) rp_orig.push_back(pt.name);

    std::vector<std::string> rp_names = name_policy::ApplyPolicyBatch(
        rp_orig, preset.name_policy, preset.require_unique_names);

    for (size_t p = 0; p < rte.points.size(); ++p) {
      WriteWptElement(out, "rtept", rte.points[p], rp_names[p], preset,
                      "    ");
    }

    out << "  </rte>\n";

    // Optionally also emit route points as top-level <wpt> entries.
    if (preset.include_route_points_as_top_level_wpts) {
      for (size_t p = 0; p < rte.points.size(); ++p) {
        WriteWptElement(out, "wpt", rte.points[p], rp_names[p], preset,
                        "  ");
      }
    }
  }

  // --- Tracks ---
  for (const auto& trk : doc.tracks) {
    out << "  <trk>\n";
    if (!trk.name.empty()) {
      out << "    <name>" << xml_util::EscapeXml(trk.name) << "</name>\n";
    }
    if (preset.include_descriptions && !trk.description.empty()) {
      out << "    <desc>" << xml_util::EscapeXml(trk.description)
          << "</desc>\n";
    }
    out << "    <trkseg>\n";
    for (const auto& pt : trk.points) {
      out << "      <trkpt lat=\"" << xml_util::FormatLat(pt.latitude)
          << "\" lon=\"" << xml_util::FormatLon(pt.longitude) << "\">\n";
      if (preset.include_timestamps && pt.timestamp.has_value()) {
        out << "        <time>"
            << xml_util::FormatTimeISO8601(*pt.timestamp) << "</time>\n";
      }
      out << "      </trkpt>\n";
    }
    out << "    </trkseg>\n";
    out << "  </trk>\n";
  }

  out << "</gpx>\n";

  return out.str();
}

}  // namespace gpx_writer
