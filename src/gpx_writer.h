/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   GPX file writer                                                       *
 **************************************************************************/

#ifndef _GPX_WRITER_H_
#define _GPX_WRITER_H_

#include <string>
#include <vector>

#include "export_model.h"
#include "gpx_preset.h"

namespace gpx_writer {

/**
 * Validate an ExportDocument against the preset rules.
 *
 * @param doc     The document to validate.
 * @param preset  The preset supplying validation rules.
 * @param errors  Output: list of human-readable validation error strings.
 * @return true if valid, false if any errors were found.
 */
bool Validate(const ExportDocument& doc, const GpxCompatibilityPreset& preset,
              std::vector<std::string>& errors);

/**
 * Generate a GPX 1.1 XML string from an ExportDocument.
 *
 * The output respects the given preset's include/exclude options,
 * name policy, and compatibility settings.
 *
 * @param doc     The document to export.
 * @param preset  The compatibility preset controlling output format.
 * @return A complete GPX XML document as a UTF-8 string.
 */
std::string Generate(const ExportDocument& doc,
                     const GpxCompatibilityPreset& preset);

}  // namespace gpx_writer

#endif  // _GPX_WRITER_H_
