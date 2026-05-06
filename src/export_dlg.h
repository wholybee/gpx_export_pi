/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Export dialog                                                         *
 **************************************************************************/

#ifndef _EXPORT_DLG_H_
#define _EXPORT_DLG_H_

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <string>
#include <vector>

#include "export_model.h"
#include "gpx_preset.h"


class wxButton;

class FtpSendDialog : public wxDialog {
public:
  FtpSendDialog(wxWindow* parent, wxFileConfig* config,
                const wxString& suggestedFilename,
                const std::string& payload);
  ~FtpSendDialog() override = default;

private:
  void OnSend(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);

  void LoadSettings();
  void SaveSettings() const;
  void AppendLog(const wxString& line);

  wxFileConfig* m_config;
  wxString m_suggestedFilename;
  std::string m_payload;

  wxTextCtrl* m_server;
  wxTextCtrl* m_username;
  wxTextCtrl* m_password;
  wxTextCtrl* m_directory;
  wxTextCtrl* m_filename;
  wxTextCtrl* m_log;
  wxButton* m_sendButton;

  wxDECLARE_EVENT_TABLE();
};

class GpxOptionsDialog : public wxDialog {
public:
  GpxOptionsDialog(wxWindow* parent, const GpxCompatibilityPreset& options);
  ~GpxOptionsDialog() override = default;

  GpxCompatibilityPreset GetOptions() const { return m_options; }

private:
  void OnSave(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);
  void OnMaxLengthChanged(wxCommandEvent& event);

  void TransferOptionsToControls();
  void TransferControlsToOptions();

  GpxCompatibilityPreset m_options;

  wxCheckBox* m_includeExtensions;
  wxCheckBox* m_includeOpenCPNExtensions;
  wxCheckBox* m_includeGarminExtensions;
  wxCheckBox* m_includeDescriptions;
  wxCheckBox* m_includeSymbols;
  wxCheckBox* m_includeTimestamps;
  wxCheckBox* m_includeRoutePointsAsWpts;
  wxCheckBox* m_requireUniqueNames;
  wxCheckBox* m_requireNonblankNames;
  wxCheckBox* m_stripSpaces;
  wxCheckBox* m_stripSpecialChars;
  wxCheckBox* m_uppercaseNames;

  wxCheckBox* m_preserveNames;
  wxCheckBox* m_sequentialNumbering;
  wxTextCtrl* m_prefix;
  wxSpinCtrl* m_startNumber;
  wxSpinCtrl* m_numberWidth;
  wxCheckBox* m_limitNameLength;
  wxSpinCtrl* m_maxNameLength;

  wxDECLARE_EVENT_TABLE();
};

class GpxExportDialog : public wxDialog {
public:
  GpxExportDialog(wxWindow* parent, wxFileConfig* config,
                  const std::vector<ExportRoute>& routes,
                  const std::vector<ExportWaypoint>& waypoints,
                  int preselect_route_idx = -1,
                  int preselect_wp_idx = -1);
  ~GpxExportDialog() override = default;

  /** Return the index of the selected route, or -1 if none. */
  int GetSelectedRouteIndex() const;

  /** Return the index of the selected waypoint, or -1 if none. */
  int GetSelectedWaypointIndex() const;

  /** Return the currently active GPX options. */
  GpxCompatibilityPreset GetSelectedPreset() const;

private:
  void OnExport(wxCommandEvent& event);
  void OnCancel(wxCommandEvent& event);
  void OnTabChanged(wxBookCtrlEvent& event);
  void OnPresetChanged(wxCommandEvent& event);
  void OnOptions(wxCommandEvent& event);
  void OnFtp(wxCommandEvent& event);

  void PopulateRoutes();
  void PopulateWaypoints();
  bool BuildSelectedDocument(ExportDocument& doc, std::string& suggested_name);
  wxString MakeSuggestedFilename(const std::string& suggested_name) const;

  wxNotebook* m_notebook;
  wxListBox* m_routeList;
  wxListBox* m_waypointList;
  wxChoice* m_presetChoice;
  wxStaticText* m_statusText;

  wxFileConfig* m_config;
  std::vector<ExportRoute> m_routes;
  std::vector<ExportWaypoint> m_waypoints;
  std::vector<GpxCompatibilityPreset> m_presets;
  GpxCompatibilityPreset m_currentOptions;
  int m_preselect_route_idx;
  int m_preselect_wp_idx;

  wxDECLARE_EVENT_TABLE();
};

#endif  // _EXPORT_DLG_H_
