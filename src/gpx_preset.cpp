/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   GPX compatibility presets - implementation                            *
 **************************************************************************/

 /*************************************************************************
 * Copyright (c) 2026 Warren Holybee                                      *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 3 of the License, or      *
 * (at your option) any later version.                                    *
 *************************************************************************/

#include "gpx_preset.h"

namespace gpx_preset {

GpxCompatibilityPreset GenericMarinePreset() {
  GpxCompatibilityPreset p;
  p.id = "generic_marine";
  p.display_name = "Generic Marine GPX";
  p.include_descriptions = true;
  p.include_symbols = false;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = false;
  p.name_policy.preserve_names = true;
  return p;
}

GpxCompatibilityPreset OpenCPNFullPreset() {
  GpxCompatibilityPreset p;
  p.id = "opencpn_full";
  p.display_name = "OpenCPN Full Feature";
  p.include_extensions = true;
  p.include_opencpn_extensions = true;
  p.include_garmin_extensions = true;
  p.include_descriptions = true;
  p.include_symbols = true;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = false;
  p.require_nonblank_names = false;
  p.strip_special_chars = false;
  p.name_policy.preserve_names = true;
  return p;
}

GpxCompatibilityPreset MinimalPreset() {
  GpxCompatibilityPreset p;
  p.id = "minimal";
  p.display_name = "Minimal GPX";
  p.include_descriptions = false;
  p.include_symbols = false;
  p.include_timestamps = false;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = true;
  p.name_policy.preserve_names = false;
  p.name_policy.sequential_numbering = true;
  p.name_policy.width = 3;
  return p;
}

GpxCompatibilityPreset GarminSafePreset() {
  GpxCompatibilityPreset p;
  p.id = "garmin_safe";
  p.display_name = "Garmin Safe GPX";
  p.include_descriptions = false;
  p.include_symbols = false;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = true;
  p.name_policy.preserve_names = true;
  return p;
}

GpxCompatibilityPreset RaymarineSafePreset() {
  GpxCompatibilityPreset p;
  p.id = "raymarine_safe";
  p.display_name = "Raymarine Safe GPX";
  p.include_descriptions = false;
  p.include_symbols = false;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = true;
  p.name_policy.preserve_names = false;
  p.name_policy.sequential_numbering = true;
  p.name_policy.width = 3;
  return p;
}

GpxCompatibilityPreset NavicoSafePreset() {
  GpxCompatibilityPreset p;
  p.id = "navico_safe";
  p.display_name = "B&G / Navico Safe GPX";
  p.include_descriptions = false;
  p.include_symbols = false;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = false;
  p.name_policy.preserve_names = true;
  return p;
}

GpxCompatibilityPreset FurunoSafePreset() {
  GpxCompatibilityPreset p;
  p.id = "furuno_safe";
  p.display_name = "Furuno Safe GPX";
  p.include_descriptions = false;
  p.include_symbols = false;
  p.include_timestamps = true;
  p.include_route_points_as_top_level_wpts = false;
  p.require_unique_names = true;
  p.require_nonblank_names = true;
  p.strip_special_chars = true;
  p.name_policy.preserve_names = true;
  p.max_name_length = 20;
  p.name_policy.max_length = 20;
  return p;
}

std::vector<GpxCompatibilityPreset> GetBuiltinPresets() {
  return {GenericMarinePreset(), OpenCPNFullPreset(),   MinimalPreset() /* , 
          GarminSafePreset(),    RaymarineSafePreset(), NavicoSafePreset(), 
          FurunoSafePreset() */ };
}

}  // namespace gpx_preset
