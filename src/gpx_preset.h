/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   GPX compatibility presets                                             *
 **************************************************************************/

#ifndef _GPX_PRESET_H_
#define _GPX_PRESET_H_

#include <string>
#include <vector>

#include "name_policy.h"

/** Data-driven configuration for GPX output compatibility. */
struct GpxCompatibilityPreset {
  std::string id;
  std::string display_name;
  bool gpx_11 = true;
  bool include_extensions = false;
  bool include_opencpn_extensions = false;
  bool include_garmin_extensions = false;
  bool include_descriptions = false;
  bool include_symbols = false;
  bool include_timestamps = true;
  bool include_route_points_as_top_level_wpts = false;
  bool require_unique_names = true;
  bool require_nonblank_names = true;
  bool strip_special_chars = true;
  bool uppercase_names = false;
  std::optional<size_t> max_name_length;
  NamePolicy name_policy;
};

namespace gpx_preset {

/** Return the list of all built-in presets. */
std::vector<GpxCompatibilityPreset> GetBuiltinPresets();

/** Return the "Generic Marine GPX" preset (safe default). */
GpxCompatibilityPreset GenericMarinePreset();

/** Return the "OpenCPN Full-Fidelity GPX" preset. */
GpxCompatibilityPreset OpenCPNFullPreset();

/** Return the "Minimal GPX" preset. */
GpxCompatibilityPreset MinimalPreset();

/** Return the "Garmin Safe GPX" preset. */
GpxCompatibilityPreset GarminSafePreset();

/** Return the "Raymarine Safe GPX" preset. */
GpxCompatibilityPreset RaymarineSafePreset();

/** Return the "B&G / Navico Safe GPX" preset. */
GpxCompatibilityPreset NavicoSafePreset();

/** Return the "Furuno Safe GPX" preset. */
GpxCompatibilityPreset FurunoSafePreset();

}  // namespace gpx_preset

#endif  // _GPX_PRESET_H_
