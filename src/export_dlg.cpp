/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Export dialog - implementation                                        *
 **************************************************************************/

#include "export_dlg.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/statbox.h>
#include <wx/wfstream.h>
#include <wx/socket.h>
#include <wx/tokenzr.h>

#include <fstream>

#include "gpx_writer.h"

namespace {

void AddCheckBox(wxWindow* parent, wxSizer* sizer, wxCheckBox*& checkbox,
                 const wxString& label) {
  checkbox = new wxCheckBox(parent, wxID_ANY, label);
  sizer->Add(checkbox, 0, wxEXPAND | wxALL, 3);
}

}  // anonymous namespace

enum {
  ID_NOTEBOOK = 10100,
  ID_ROUTE_LIST,
  ID_WAYPOINT_LIST,
  ID_PRESET_CHOICE,
  ID_BTN_OPTIONS,
  ID_BTN_FTP,
  ID_BTN_EXPORT,
  ID_BTN_CANCEL,
  ID_BTN_OPTIONS_SAVE,
  ID_BTN_OPTIONS_CANCEL,
  ID_BTN_FTP_SEND,
  ID_BTN_FTP_CANCEL,
  ID_LIMIT_NAME_LENGTH,
};

wxBEGIN_EVENT_TABLE(FtpSendDialog, wxDialog)
    EVT_BUTTON(ID_BTN_FTP_SEND, FtpSendDialog::OnSend)
    EVT_BUTTON(ID_BTN_FTP_CANCEL, FtpSendDialog::OnCancel)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(GpxOptionsDialog, wxDialog)
    EVT_BUTTON(ID_BTN_OPTIONS_SAVE, GpxOptionsDialog::OnSave)
    EVT_BUTTON(ID_BTN_OPTIONS_CANCEL, GpxOptionsDialog::OnCancel)
    EVT_CHECKBOX(ID_LIMIT_NAME_LENGTH, GpxOptionsDialog::OnMaxLengthChanged)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(GpxExportDialog, wxDialog)
    EVT_BUTTON(ID_BTN_EXPORT, GpxExportDialog::OnExport)
    EVT_BUTTON(ID_BTN_CANCEL, GpxExportDialog::OnCancel)
    EVT_BUTTON(ID_BTN_OPTIONS, GpxExportDialog::OnOptions)
    EVT_BUTTON(ID_BTN_FTP, GpxExportDialog::OnFtp)
    EVT_NOTEBOOK_PAGE_CHANGED(ID_NOTEBOOK, GpxExportDialog::OnTabChanged)
    EVT_CHOICE(ID_PRESET_CHOICE, GpxExportDialog::OnPresetChanged)
wxEND_EVENT_TABLE()


// ---------------------------------------------------------------------------
// FTP send dialog and simple passive-mode FTP client
// ---------------------------------------------------------------------------

namespace {

const wxChar* kFtpConfigPath = wxT("/PlugIns/GPXExport/FTP");

bool IsAsciiDigit(wxChar ch) { return ch >= wxT('0') && ch <= wxT('9'); }

bool ParseServer(const wxString& input, wxString& host, int& port) {
  host = input;
  host.Trim(true).Trim(false);
  port = 21;
  if (host.IsEmpty()) return false;

  // Allow users to enter either "example.com" or "example.com:2121".
  if (!host.StartsWith(wxT("["))) {
    int colon = host.Find(wxT(':'), true);
    if (colon != wxNOT_FOUND) {
      long parsed_port = 0;
      wxString port_part = host.Mid(colon + 1);
      if (port_part.ToLong(&parsed_port) && parsed_port > 0 &&
          parsed_port <= 65535) {
        host = host.Left(colon);
        port = static_cast<int>(parsed_port);
      }
    }
  }
  return !host.IsEmpty();
}

class PassiveFtpUploader {
public:
  PassiveFtpUploader(wxTextCtrl* log, const wxString& host, int port,
                     const wxString& user, const wxString& pass)
      : m_log(log), m_host(host), m_port(port), m_user(user), m_pass(pass) {}

  bool Upload(const wxString& directory, const wxString& filename,
              const std::string& payload) {
    wxIPV4address addr;
    addr.Hostname(m_host);
    addr.Service(m_port);

    m_control.SetTimeout(20);
    m_control.SetFlags(wxSOCKET_WAITALL);

    Log(wxString::Format(wxT("Connecting to %s:%d"), m_host, m_port));
    m_control.Connect(addr, false);
    if (!m_control.WaitOnConnect(20) || !m_control.IsConnected()) {
      Log(wxT("ERROR: Could not connect to FTP server."));
      return false;
    }

    int code = ReadReply();
    if (code < 200 || code >= 400) return FailQuit(wxT("Server greeting failed."));

    if (!SendCommand(wxT("USER ") + m_user, false)) return false;
    code = ReadReply();
    if (code == 331) {
      if (!SendCommand(wxT("PASS ") + m_pass, true)) return false;
      code = ReadReply();
    }
    if (code < 200 || code >= 300) return FailQuit(wxT("Login failed."));

    if (!SendCommand(wxT("TYPE I"), false)) return false;
    code = ReadReply();
    if (code < 200 || code >= 300) return FailQuit(wxT("Could not set binary transfer mode."));

    wxString clean_directory = directory;
    clean_directory.Trim(true).Trim(false);
    if (!clean_directory.IsEmpty()) {
      if (!SendCommand(wxT("CWD ") + clean_directory, false)) return false;
      code = ReadReply();
      if (code < 200 || code >= 300) return FailQuit(wxT("Could not change to destination directory."));
    }

    wxString pasv_host;
    int pasv_port = 0;
    if (!EnterPassiveMode(pasv_host, pasv_port)) return false;

    wxSocketClient data;
    wxIPV4address data_addr;
    data_addr.Hostname(pasv_host);
    data_addr.Service(pasv_port);
    data.SetTimeout(20);
    data.SetFlags(wxSOCKET_WAITALL);

    Log(wxString::Format(wxT("Opening passive data connection to %s:%d"),
                         pasv_host, pasv_port));
    data.Connect(data_addr, false);
    if (!data.WaitOnConnect(20) || !data.IsConnected()) {
      return FailQuit(wxT("Could not open passive data connection."));
    }

    if (!SendCommand(wxT("STOR ") + filename, false)) return false;
    code = ReadReply();
    if (code < 100 || code >= 200) {
      data.Close();
      return FailQuit(wxT("Server did not accept STOR command."));
    }

    Log(wxString::Format(wxT("Sending %lu byte(s) of GPX data."),
                         static_cast<unsigned long>(payload.size())));
    if (!payload.empty()) {
      data.Write(payload.data(), payload.size());
      if (data.LastCount() != payload.size() || data.Error()) {
        data.Close();
        return FailQuit(wxT("Data connection write failed."));
      }
    }
    data.Close();

    code = ReadReply();
    if (code < 200 || code >= 300) return FailQuit(wxT("Upload did not complete successfully."));

    SendCommand(wxT("QUIT"), false);
    ReadReply();
    m_control.Close();
    Log(wxT("Upload complete."));
    return true;
  }

private:
  void Log(const wxString& line) {
    if (m_log) m_log->AppendText(line + wxT("\n"));
  }

  bool SendCommand(const wxString& command, bool hide_value) {
    wxString logged = command;
    if (hide_value) logged = wxT("PASS ********");
    Log(wxT("> ") + logged);

    wxCharBuffer utf8 = command.ToUTF8();
    std::string line(utf8.data() ? utf8.data() : "");
    line += "\r\n";
    m_control.Write(line.data(), line.size());
    if (m_control.LastCount() != line.size() || m_control.Error()) {
      Log(wxT("ERROR: Failed to write command to control connection."));
      return false;
    }
    return true;
  }

  wxString ReadPhysicalLine() {
    std::string line;
    char c = 0;
    while (true) {
      if (!m_control.WaitForRead(20)) break;
      m_control.Read(&c, 1);
      if (m_control.LastCount() != 1 || m_control.Error()) break;
      if (c == '\n') break;
      if (c != '\r') line.push_back(c);
    }
    return wxString::FromUTF8(line.c_str());
  }

  int ReadReply() {
    int reply_code = -1;
    wxString expected_prefix;

    while (true) {
      wxString line = ReadPhysicalLine();
      if (line.IsEmpty()) {
        Log(wxT("ERROR: Timed out waiting for FTP reply."));
        return -1;
      }
      Log(wxT("< ") + line);
      m_lastReply = line;

      if (line.Length() >= 3 && IsAsciiDigit(line[0]) &&
          IsAsciiDigit(line[1]) && IsAsciiDigit(line[2])) {
        long code_long = -1;
        line.Left(3).ToLong(&code_long);
        if (reply_code < 0) {
          reply_code = static_cast<int>(code_long);
          expected_prefix = line.Left(3) + wxT(" ");
          if (line.Length() >= 4 && line[3] == wxT(' ')) return reply_code;
        } else if (line.StartsWith(expected_prefix)) {
          return reply_code;
        }
      }
    }
  }

  bool EnterPassiveMode(wxString& data_host, int& data_port) {
    if (!SendCommand(wxT("PASV"), false)) return false;
    int code = ReadReply();
    if (code < 200 || code >= 300) return FailQuit(wxT("PASV command failed."));

    // Parse the host/port tuple from the PASV reply captured by ReadReply().
    return ParsePasvReply(m_lastReply, data_host, data_port);
  }

  bool ParsePasvReply(const wxString& reply, wxString& data_host,
                      int& data_port) {
    int open = reply.Find(wxT('('));
    int close = reply.Find(wxT(')'), true);
    if (open == wxNOT_FOUND || close == wxNOT_FOUND || close <= open) {
      return FailQuit(wxT("Could not parse PASV response."));
    }

    wxString body = reply.Mid(open + 1, close - open - 1);
    long nums[6] = {0, 0, 0, 0, 0, 0};
    wxStringTokenizer tok(body, wxT(","));
    for (int i = 0; i < 6; ++i) {
      if (!tok.HasMoreTokens() || !tok.GetNextToken().ToLong(&nums[i])) {
        return FailQuit(wxT("Could not parse PASV address."));
      }
    }
    data_host = wxString::Format(wxT("%ld.%ld.%ld.%ld"), nums[0], nums[1],
                                 nums[2], nums[3]);
    data_port = static_cast<int>(nums[4] * 256 + nums[5]);
    return true;
  }

  bool FailQuit(const wxString& message) {
    Log(wxT("ERROR: ") + message);
    if (m_control.IsConnected()) {
      SendCommand(wxT("QUIT"), false);
      ReadReply();
      m_control.Close();
    }
    return false;
  }

  wxTextCtrl* m_log;
  wxString m_host;
  int m_port;
  wxString m_user;
  wxString m_pass;
  wxSocketClient m_control;
  wxString m_lastReply;
};

}  // anonymous namespace

FtpSendDialog::FtpSendDialog(wxWindow* parent, wxFileConfig* config,
                             const wxString& suggestedFilename,
                             const std::string& payload)
    : wxDialog(parent, wxID_ANY, wxT("Send GPX by FTP"), wxDefaultPosition,
               wxSize(620, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_config(config),
      m_suggestedFilename(suggestedFilename),
      m_payload(payload) {
  auto* topSizer = new wxBoxSizer(wxVERTICAL);

  auto* settingsBox =
      new wxStaticBoxSizer(wxVERTICAL, this, wxT("FTP destination"));

  auto* serverSizer = new wxBoxSizer(wxHORIZONTAL);
  serverSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Server:")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_server = new wxTextCtrl(this, wxID_ANY);
  serverSizer->Add(m_server, 1, wxEXPAND);
  settingsBox->Add(serverSizer, 0, wxEXPAND | wxALL, 4);

  auto* userSizer = new wxBoxSizer(wxHORIZONTAL);
  userSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Username:")), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_username = new wxTextCtrl(this, wxID_ANY);
  userSizer->Add(m_username, 1, wxEXPAND);
  settingsBox->Add(userSizer, 0, wxEXPAND | wxALL, 4);

  auto* passSizer = new wxBoxSizer(wxHORIZONTAL);
  passSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Password:")), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_password = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                              wxDefaultSize, wxTE_PASSWORD);
  passSizer->Add(m_password, 1, wxEXPAND);
  settingsBox->Add(passSizer, 0, wxEXPAND | wxALL, 4);

  auto* dirSizer = new wxBoxSizer(wxHORIZONTAL);
  dirSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Directory:")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_directory = new wxTextCtrl(this, wxID_ANY);
  dirSizer->Add(m_directory, 1, wxEXPAND);
  settingsBox->Add(dirSizer, 0, wxEXPAND | wxALL, 4);

  auto* filenameSizer = new wxBoxSizer(wxHORIZONTAL);
  filenameSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Remote filename:")),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_filename = new wxTextCtrl(this, wxID_ANY, m_suggestedFilename);
  filenameSizer->Add(m_filename, 1, wxEXPAND);
  settingsBox->Add(filenameSizer, 0, wxEXPAND | wxALL, 4);

  topSizer->Add(settingsBox, 0, wxEXPAND | wxALL, 8);

  auto* logBox =
      new wxStaticBoxSizer(wxVERTICAL, this, wxT("FTP command log"));
  m_log = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                         wxSize(-1, 220), wxTE_MULTILINE | wxTE_READONLY |
                                             wxTE_DONTWRAP);
  logBox->Add(m_log, 1, wxEXPAND | wxALL, 4);
  topSizer->Add(logBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

  auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
  btnSizer->AddStretchSpacer();
  m_sendButton = new wxButton(this, ID_BTN_FTP_SEND, wxT("Send"));
  btnSizer->Add(m_sendButton, 0, wxRIGHT, 8);
  btnSizer->Add(new wxButton(this, ID_BTN_FTP_CANCEL, wxT("Cancel")));
  topSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(topSizer);
  LoadSettings();
}

void FtpSendDialog::LoadSettings() {
  wxFileConfig* cfg = m_config;
  if (!cfg) return;
  wxString old_path = cfg->GetPath();
  cfg->SetPath(kFtpConfigPath);
  m_server->SetValue(cfg->Read(wxT("Server"), wxEmptyString));
  m_username->SetValue(cfg->Read(wxT("Username"), wxEmptyString));
  m_password->SetValue(cfg->Read(wxT("Password"), wxEmptyString));
  m_directory->SetValue(cfg->Read(wxT("Directory"), wxEmptyString));
  cfg->SetPath(old_path);
}

void FtpSendDialog::SaveSettings() const {
  wxFileConfig* cfg = m_config;
  if (!cfg) return;
  wxString old_path = cfg->GetPath();
  cfg->SetPath(kFtpConfigPath);
  cfg->Write(wxT("Server"), m_server->GetValue());
  cfg->Write(wxT("Username"), m_username->GetValue());
  cfg->Write(wxT("Password"), m_password->GetValue());
  cfg->Write(wxT("Directory"), m_directory->GetValue());
  cfg->Flush();
  cfg->SetPath(old_path);
}

void FtpSendDialog::AppendLog(const wxString& line) {
  m_log->AppendText(line + wxT("\n"));
}

void FtpSendDialog::OnCancel(wxCommandEvent& event) { EndModal(wxID_CANCEL); }

void FtpSendDialog::OnSend(wxCommandEvent& event) {
  wxString host;
  int port = 21;
  if (!ParseServer(m_server->GetValue(), host, port)) {
    wxMessageBox(wxT("Please enter an FTP server name or IP address."),
                 wxT("FTP Send"), wxOK | wxICON_WARNING, this);
    return;
  }
  if (m_username->GetValue().IsEmpty()) {
    wxMessageBox(wxT("Please enter an FTP username."), wxT("FTP Send"),
                 wxOK | wxICON_WARNING, this);
    return;
  }

  wxString remote_filename = m_filename->GetValue();
  remote_filename.Trim(true).Trim(false);
  if (remote_filename.IsEmpty()) {
    wxMessageBox(wxT("Please enter a remote filename for the GPX file."),
                 wxT("FTP Send"), wxOK | wxICON_WARNING, this);
    return;
  }
  if (remote_filename.Find(wxT('/')) != wxNOT_FOUND ||
      remote_filename.Find(wxT('\\')) != wxNOT_FOUND) {
    wxMessageBox(wxT("Please enter only a filename. Use the Directory field for the remote destination path."),
                 wxT("FTP Send"), wxOK | wxICON_WARNING, this);
    return;
  }

  SaveSettings();
  m_log->Clear();
  m_sendButton->Enable(false);

  PassiveFtpUploader uploader(m_log, host, port, m_username->GetValue(),
                              m_password->GetValue());
  bool ok = uploader.Upload(m_directory->GetValue(), remote_filename,
                            m_payload);
  m_sendButton->Enable(true);

  if (ok) {
    wxMessageBox(wxT("GPX file sent successfully by FTP."), wxT("FTP Send"),
                 wxOK | wxICON_INFORMATION, this);
    EndModal(wxID_OK);
  } else {
    wxMessageBox(wxT("FTP send failed. See the command log for details."),
                 wxT("FTP Send"), wxOK | wxICON_ERROR, this);
  }
}

// ---------------------------------------------------------------------------
// Options dialog
// ---------------------------------------------------------------------------

GpxOptionsDialog::GpxOptionsDialog(wxWindow* parent,
                                   const GpxCompatibilityPreset& options)
    : wxDialog(parent, wxID_ANY, wxT("GPX Export Options"), wxDefaultPosition,
               wxSize(520, 620),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_options(options) {
  auto* topSizer = new wxBoxSizer(wxVERTICAL);

  auto* fieldBox =
      new wxStaticBoxSizer(wxVERTICAL, this, wxT("GPX fields and metadata"));
  AddCheckBox(this, fieldBox, m_includeDescriptions,
              wxT("Include descriptions"));
  AddCheckBox(this, fieldBox, m_includeSymbols, wxT("Include symbols/icons"));
  AddCheckBox(this, fieldBox, m_includeTimestamps, wxT("Include timestamps"));
  AddCheckBox(this, fieldBox, m_includeRoutePointsAsWpts,
              wxT("Also export route points as top-level waypoints"));
  AddCheckBox(this, fieldBox, m_includeExtensions,
              wxT("Include GPX extension blocks"));
  AddCheckBox(this, fieldBox, m_includeOpenCPNExtensions,
              wxT("Include OpenCPN extensions"));
  AddCheckBox(this, fieldBox, m_includeGarminExtensions,
              wxT("Include Garmin GPX extensions"));
  topSizer->Add(fieldBox, 0, wxEXPAND | wxALL, 8);

  auto* validationBox =
      new wxStaticBoxSizer(wxVERTICAL, this, wxT("Compatibility validation"));
  AddCheckBox(this, validationBox, m_requireUniqueNames,
              wxT("Require/export unique waypoint names"));
  AddCheckBox(this, validationBox, m_requireNonblankNames,
              wxT("Require non-blank waypoint names"));
  AddCheckBox(this, validationBox, m_stripSpaces,
              wxT("Strip spaces from exported names"));
  AddCheckBox(this, validationBox, m_stripSpecialChars,
              wxT("Strip special characters from exported names"));
  AddCheckBox(this, validationBox, m_uppercaseNames,
              wxT("Uppercase exported names"));
  topSizer->Add(validationBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto* nameBox =
      new wxStaticBoxSizer(wxVERTICAL, this, wxT("Waypoint naming policy"));
  AddCheckBox(this, nameBox, m_preserveNames, wxT("Preserve original names"));
  AddCheckBox(this, nameBox, m_sequentialNumbering,
              wxT("Number waypoints sequentially"));

  auto* prefixSizer = new wxBoxSizer(wxHORIZONTAL);
  prefixSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Prefix:")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_prefix = new wxTextCtrl(this, wxID_ANY);
  prefixSizer->Add(m_prefix, 1, wxEXPAND);
  nameBox->Add(prefixSizer, 0, wxEXPAND | wxALL, 3);

  auto* seqSizer = new wxBoxSizer(wxHORIZONTAL);
  seqSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Start number:")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_startNumber = new wxSpinCtrl(this, wxID_ANY);
  m_startNumber->SetRange(0, 999999);
  seqSizer->Add(m_startNumber, 1, wxRIGHT, 12);
  seqSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Width:")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_numberWidth = new wxSpinCtrl(this, wxID_ANY);
  m_numberWidth->SetRange(1, 12);
  seqSizer->Add(m_numberWidth, 1);
  nameBox->Add(seqSizer, 0, wxEXPAND | wxALL, 3);

  auto* lengthSizer = new wxBoxSizer(wxHORIZONTAL);
  m_limitNameLength =
      new wxCheckBox(this, ID_LIMIT_NAME_LENGTH, wxT("Limit name length"));
  lengthSizer->Add(m_limitNameLength, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_maxNameLength = new wxSpinCtrl(this, wxID_ANY);
  m_maxNameLength->SetRange(1, 255);
  lengthSizer->Add(m_maxNameLength, 1);
  nameBox->Add(lengthSizer, 0, wxEXPAND | wxALL, 3);

  topSizer->Add(nameBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

  auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
  btnSizer->AddStretchSpacer();
  btnSizer->Add(new wxButton(this, ID_BTN_OPTIONS_SAVE, wxT("Save")), 0,
                wxRIGHT, 8);
  btnSizer->Add(new wxButton(this, ID_BTN_OPTIONS_CANCEL, wxT("Cancel")));
  topSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 8);

  SetSizer(topSizer);
  TransferOptionsToControls();
}

void GpxOptionsDialog::TransferOptionsToControls() {
  m_includeExtensions->SetValue(m_options.include_extensions);
  m_includeOpenCPNExtensions->SetValue(m_options.include_opencpn_extensions);
  m_includeGarminExtensions->SetValue(m_options.include_garmin_extensions);
  m_includeDescriptions->SetValue(m_options.include_descriptions);
  m_includeSymbols->SetValue(m_options.include_symbols);
  m_includeTimestamps->SetValue(m_options.include_timestamps);
  m_includeRoutePointsAsWpts->SetValue(
      m_options.include_route_points_as_top_level_wpts);
  m_requireUniqueNames->SetValue(m_options.require_unique_names);
  m_requireNonblankNames->SetValue(m_options.require_nonblank_names);
  m_stripSpaces->SetValue(m_options.name_policy.strip_spaces);
  m_stripSpecialChars->SetValue(m_options.strip_special_chars ||
                                 m_options.name_policy.strip_special_chars);
  m_uppercaseNames->SetValue(m_options.uppercase_names ||
                              m_options.name_policy.uppercase);

  m_preserveNames->SetValue(m_options.name_policy.preserve_names);
  m_sequentialNumbering->SetValue(m_options.name_policy.sequential_numbering);
  m_prefix->SetValue(wxString(m_options.name_policy.prefix));
  m_startNumber->SetValue(m_options.name_policy.start_number);
  m_numberWidth->SetValue(m_options.name_policy.width);

  const bool hasMaxLength = m_options.max_name_length.has_value() ||
                            m_options.name_policy.max_length.has_value();
  m_limitNameLength->SetValue(hasMaxLength);
  size_t maxLength = 20;
  if (m_options.name_policy.max_length.has_value())
    maxLength = *m_options.name_policy.max_length;
  else if (m_options.max_name_length.has_value())
    maxLength = *m_options.max_name_length;
  m_maxNameLength->SetValue((int)maxLength);
  m_maxNameLength->Enable(hasMaxLength);
}

void GpxOptionsDialog::TransferControlsToOptions() {
  m_options.id.clear();
  m_options.display_name = "Custom GPX Options";

  m_options.include_extensions = m_includeExtensions->GetValue();
  m_options.include_opencpn_extensions = m_includeOpenCPNExtensions->GetValue();
  m_options.include_garmin_extensions = m_includeGarminExtensions->GetValue();
  if (m_options.include_opencpn_extensions || m_options.include_garmin_extensions)
    m_options.include_extensions = true;

  m_options.include_descriptions = m_includeDescriptions->GetValue();
  m_options.include_symbols = m_includeSymbols->GetValue();
  m_options.include_timestamps = m_includeTimestamps->GetValue();
  m_options.include_route_points_as_top_level_wpts =
      m_includeRoutePointsAsWpts->GetValue();
  m_options.require_unique_names = m_requireUniqueNames->GetValue();
  m_options.require_nonblank_names = m_requireNonblankNames->GetValue();
  m_options.strip_special_chars = m_stripSpecialChars->GetValue();
  m_options.uppercase_names = m_uppercaseNames->GetValue();

  m_options.name_policy.preserve_names = m_preserveNames->GetValue();
  m_options.name_policy.sequential_numbering = m_sequentialNumbering->GetValue();
  m_options.name_policy.prefix = m_prefix->GetValue().ToStdString();
  m_options.name_policy.start_number = m_startNumber->GetValue();
  m_options.name_policy.width = m_numberWidth->GetValue();
  m_options.name_policy.strip_spaces = m_stripSpaces->GetValue();
  m_options.name_policy.strip_special_chars = m_stripSpecialChars->GetValue();
  m_options.name_policy.uppercase = m_uppercaseNames->GetValue();

  if (m_limitNameLength->GetValue()) {
    const size_t value = (size_t)m_maxNameLength->GetValue();
    m_options.max_name_length = value;
    m_options.name_policy.max_length = value;
  } else {
    m_options.max_name_length.reset();
    m_options.name_policy.max_length.reset();
  }
}

void GpxOptionsDialog::OnMaxLengthChanged(wxCommandEvent& event) {
  m_maxNameLength->Enable(m_limitNameLength->GetValue());
}

void GpxOptionsDialog::OnCancel(wxCommandEvent& event) { EndModal(wxID_CANCEL); }

void GpxOptionsDialog::OnSave(wxCommandEvent& event) {
  TransferControlsToOptions();
  EndModal(wxID_OK);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

GpxExportDialog::GpxExportDialog(wxWindow* parent,
                                 wxFileConfig* config,
                                 const std::vector<ExportRoute>& routes,
                                 const std::vector<ExportWaypoint>& waypoints,
                                 int preselect_route_idx,
                                 int preselect_wp_idx)
    : wxDialog(parent, wxID_ANY, wxT("Export GPX"), wxDefaultPosition,
               wxSize(640, 460),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_config(config),
      m_routes(routes),
      m_waypoints(waypoints),
      m_preselect_route_idx(preselect_route_idx),
      m_preselect_wp_idx(preselect_wp_idx) {
  m_presets = gpx_preset::GetBuiltinPresets();
  m_currentOptions = m_presets.empty() ? gpx_preset::GenericMarinePreset()
                                      : m_presets[0];

  auto* topSizer = new wxBoxSizer(wxVERTICAL);

  // --- Main content area: tabbed object lists with a right-side action column ---
  auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);
  auto* leftSizer = new wxBoxSizer(wxVERTICAL);

  m_notebook = new wxNotebook(this, ID_NOTEBOOK);

  auto* routePanel = new wxPanel(m_notebook, wxID_ANY);
  auto* routeSizer = new wxBoxSizer(wxVERTICAL);
  m_routeList =
      new wxListBox(routePanel, ID_ROUTE_LIST, wxDefaultPosition, wxSize(-1, 240));
  routeSizer->Add(m_routeList, 1, wxEXPAND | wxALL, 6);
  routePanel->SetSizer(routeSizer);
  m_notebook->AddPage(routePanel, wxT("Routes"));

  auto* waypointPanel = new wxPanel(m_notebook, wxID_ANY);
  auto* waypointSizer = new wxBoxSizer(wxVERTICAL);
  m_waypointList = new wxListBox(waypointPanel, ID_WAYPOINT_LIST,
                                 wxDefaultPosition, wxSize(-1, 240));
  waypointSizer->Add(m_waypointList, 1, wxEXPAND | wxALL, 6);
  waypointPanel->SetSizer(waypointSizer);
  m_notebook->AddPage(waypointPanel, wxT("Waypoints"));

  // Default to Routes if we have a route preselection, otherwise Waypoints.
  if (m_preselect_route_idx >= 0 || m_preselect_wp_idx < 0)
    m_notebook->SetSelection(0);
  else
    m_notebook->SetSelection(1);

  leftSizer->Add(m_notebook, 1, wxEXPAND | wxBOTTOM, 8);

  // --- Preset selector ---
  auto* presetSizer = new wxBoxSizer(wxHORIZONTAL);
  presetSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Preset:")), 0,
                   wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  wxArrayString presetNames;
  for (const auto& p : m_presets) presetNames.Add(wxString(p.display_name));
  m_presetChoice = new wxChoice(this, ID_PRESET_CHOICE, wxDefaultPosition,
                                wxDefaultSize, presetNames);
  m_presetChoice->SetSelection(0);  // Generic Marine GPX
  presetSizer->Add(m_presetChoice, 1, wxEXPAND);
  leftSizer->Add(presetSizer, 0, wxEXPAND | wxBOTTOM, 8);

  // --- Status ---
  m_statusText = new wxStaticText(this, wxID_ANY, wxEmptyString);
  leftSizer->Add(m_statusText, 0, wxEXPAND);

  contentSizer->Add(leftSizer, 1, wxEXPAND | wxALL, 8);

  // --- Right-side action buttons ---
  auto* btnSizer = new wxBoxSizer(wxVERTICAL);
  btnSizer->Add(new wxButton(this, ID_BTN_OPTIONS, wxT("Options...")), 0,
                wxEXPAND | wxBOTTOM, 8);
  btnSizer->Add(new wxButton(this, ID_BTN_FTP, wxT("FTP...")), 0,
                wxEXPAND | wxBOTTOM, 8);
  btnSizer->Add(new wxButton(this, ID_BTN_EXPORT, wxT("Export...")), 0,
                wxEXPAND | wxBOTTOM, 8);
  btnSizer->AddStretchSpacer();
  btnSizer->Add(new wxButton(this, ID_BTN_CANCEL, wxT("Cancel")), 0,
                wxEXPAND);
  contentSizer->Add(btnSizer, 0, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 8);

  topSizer->Add(contentSizer, 1, wxEXPAND);

  SetSizer(topSizer);

  // Populate lists and update status for the initially selected tab.
  PopulateRoutes();
  PopulateWaypoints();
  if (m_notebook->GetSelection() == 0) {
    m_statusText->SetLabel(
        wxString::Format(wxT("%zu route(s) available"), m_routes.size()));
  } else {
    m_statusText->SetLabel(
        wxString::Format(wxT("%zu waypoint(s) available"), m_waypoints.size()));
  }
}

// ---------------------------------------------------------------------------
// List population
// ---------------------------------------------------------------------------

void GpxExportDialog::PopulateRoutes() {
  m_routeList->Clear();
  for (const auto& rte : m_routes) {
    wxString label = wxString(rte.name);
    if (label.IsEmpty()) label = wxString(rte.guid);
    label += wxString::Format(wxT("  (%zu pts)"), rte.points.size());
    if (rte.is_active) label += wxT(" [active]");
    m_routeList->Append(label);
  }
  if (m_preselect_route_idx >= 0 &&
      m_preselect_route_idx < (int)m_routes.size()) {
    m_routeList->SetSelection(m_preselect_route_idx);
  }
}

void GpxExportDialog::PopulateWaypoints() {
  m_waypointList->Clear();
  for (const auto& wp : m_waypoints) {
    wxString label = wxString(wp.name);
    if (label.IsEmpty()) label = wxString(wp.guid);
    m_waypointList->Append(label);
  }
  if (m_preselect_wp_idx >= 0 &&
      m_preselect_wp_idx < (int)m_waypoints.size()) {
    m_waypointList->SetSelection(m_preselect_wp_idx);
  }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int GpxExportDialog::GetSelectedRouteIndex() const {
  if (m_notebook->GetSelection() != 0) return -1;
  return m_routeList->GetSelection();
}

int GpxExportDialog::GetSelectedWaypointIndex() const {
  if (m_notebook->GetSelection() != 1) return -1;
  return m_waypointList->GetSelection();
}

GpxCompatibilityPreset GpxExportDialog::GetSelectedPreset() const {
  return m_currentOptions;
}

bool GpxExportDialog::BuildSelectedDocument(ExportDocument& doc,
                                            std::string& suggested_name) {
  doc = ExportDocument();
  suggested_name.clear();

  if (m_notebook->GetSelection() == 0) {
    int sel = m_routeList->GetSelection();
    if (sel == wxNOT_FOUND) {
      wxMessageBox(wxT("Please select a route to export."),
                   wxT("GPX Export"), wxOK | wxICON_WARNING, this);
      return false;
    }
    if (sel < 0 || sel >= (int)m_routes.size()) return false;
    doc.routes.push_back(m_routes[sel]);
    suggested_name = m_routes[sel].name;
  } else {
    int sel = m_waypointList->GetSelection();
    if (sel == wxNOT_FOUND) {
      wxMessageBox(wxT("Please select a waypoint to export."),
                   wxT("GPX Export"), wxOK | wxICON_WARNING, this);
      return false;
    }
    if (sel < 0 || sel >= (int)m_waypoints.size()) return false;
    doc.waypoints.push_back(m_waypoints[sel]);
    suggested_name = m_waypoints[sel].name;
  }

  if (suggested_name.empty()) suggested_name = "export";
  return true;
}

wxString GpxExportDialog::MakeSuggestedFilename(
    const std::string& suggested_name) const {
  std::string filename = suggested_name.empty() ? "export" : suggested_name;
  for (char& c : filename) {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
        c == '"' || c == '<' || c == '>' || c == '|') {
      c = '_';
    }
  }
  filename += ".gpx";
  return wxString::FromUTF8(filename.c_str());
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void GpxExportDialog::OnTabChanged(wxBookCtrlEvent& event) {
  if (!m_statusText || !m_notebook) return;

  if (m_notebook->GetSelection() == 0) {
    m_statusText->SetLabel(
        wxString::Format(wxT("%zu route(s) available"), m_routes.size()));
  } else {
    m_statusText->SetLabel(
        wxString::Format(wxT("%zu waypoint(s) available"), m_waypoints.size()));
  }
}

void GpxExportDialog::OnPresetChanged(wxCommandEvent& event) {
  int sel = m_presetChoice->GetSelection();
  if (sel >= 0 && sel < (int)m_presets.size()) {
    m_currentOptions = m_presets[sel];
  }
}

void GpxExportDialog::OnOptions(wxCommandEvent& event) {
  GpxOptionsDialog dlg(this, m_currentOptions);
  if (dlg.ShowModal() == wxID_OK) {
    m_currentOptions = dlg.GetOptions();
    // Edited options are no longer guaranteed to match any built-in preset.
    m_presetChoice->SetSelection(wxNOT_FOUND);
  }
}

void GpxExportDialog::OnCancel(wxCommandEvent& event) { EndModal(wxID_CANCEL); }

void GpxExportDialog::OnFtp(wxCommandEvent& event) {
  GpxCompatibilityPreset preset = GetSelectedPreset();

  ExportDocument doc;
  std::string suggested_name;
  if (!BuildSelectedDocument(doc, suggested_name)) return;

  std::vector<std::string> errors;
  if (!gpx_writer::Validate(doc, preset, errors)) {
    wxString msg = wxT("Validation errors:\n");
    for (const auto& e : errors) msg += wxString(e) + wxT("\n");
    wxMessageBox(msg, wxT("GPX Export - Validation Failed"),
                 wxOK | wxICON_ERROR, this);
    return;
  }

  std::string gpx = gpx_writer::Generate(doc, preset);
  FtpSendDialog dlg(this, m_config, MakeSuggestedFilename(suggested_name), gpx);
  dlg.ShowModal();
}

void GpxExportDialog::OnExport(wxCommandEvent& event) {
  GpxCompatibilityPreset preset = GetSelectedPreset();

  ExportDocument doc;
  std::string suggested_name;
  if (!BuildSelectedDocument(doc, suggested_name)) return;

  std::vector<std::string> errors;
  if (!gpx_writer::Validate(doc, preset, errors)) {
    wxString msg = wxT("Validation errors:\n");
    for (const auto& e : errors) msg += wxString(e) + wxT("\n");
    wxMessageBox(msg, wxT("GPX Export - Validation Failed"),
                 wxOK | wxICON_ERROR, this);
    return;
  }

  std::string gpx = gpx_writer::Generate(doc, preset);
  wxString suggested_filename = MakeSuggestedFilename(suggested_name);

  wxFileDialog dlg(this, wxT("Save GPX File"), wxEmptyString,
                   suggested_filename, wxT("GPX files (*.gpx)|*.gpx"),
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (dlg.ShowModal() == wxID_CANCEL) return;

  wxString path = dlg.GetPath();
  std::ofstream ofs(path.ToStdString(), std::ios::binary);
  if (!ofs) {
    wxMessageBox(wxT("Failed to open file for writing:\n") + path,
                 wxT("GPX Export Error"), wxOK | wxICON_ERROR, this);
    return;
  }
  ofs.write(gpx.data(), gpx.size());
  ofs.close();

  wxMessageBox(
      wxString::Format(wxT("GPX file saved successfully.\n\n%s"), path),
      wxT("GPX Export"), wxOK | wxICON_INFORMATION, this);

  EndModal(wxID_OK);
}
