#include "stdafx.h"
#include "rhinoSdkPlugInDeclare.h"
#include "ChooseOneHook.h"
#include "Settings.h"

#pragma warning(push)
#pragma warning(disable : 4073)
#pragma init_seg(lib)
#pragma warning(pop)

RHINO_PLUG_IN_DECLARE
RHINO_PLUG_IN_NAME(L"SelectionMenuPlus");
RHINO_PLUG_IN_ID(L"205C988D-9559-4DD9-B631-7572B3331E5F");
RHINO_PLUG_IN_VERSION("0.2.1")
RHINO_PLUG_IN_DESCRIPTION(L"Replaces Rhino's selection menu with context-aware picking.");
RHINO_PLUG_IN_DEVELOPER_ORGANIZATION(L"Todd Faulls");
RHINO_PLUG_IN_DEVELOPER_ADDRESS(L"");
RHINO_PLUG_IN_DEVELOPER_COUNTRY(L"");
RHINO_PLUG_IN_DEVELOPER_PHONE(L"");
RHINO_PLUG_IN_DEVELOPER_FAX(L"");
RHINO_PLUG_IN_DEVELOPER_EMAIL(L"");
RHINO_PLUG_IN_DEVELOPER_WEBSITE(L"");
RHINO_PLUG_IN_UPDATE_URL(L"");

// MFC regular DLL application object.
class CSelectionMenuPlusApp : public CWinApp
{
public:
  BOOL InitInstance() override { return CWinApp::InitInstance(); }
  int ExitInstance() override { return CWinApp::ExitInstance(); }
};
CSelectionMenuPlusApp theApp;

class CSelectionMenuPlusPlugIn : public CRhinoUtilityPlugIn
{
public:
  const wchar_t* PlugInName() const override { return RhinoPlugInName(); }
  const wchar_t* PlugInVersion() const override { return m_version; }
  GUID PlugInID() const override { return ON_UuidFromString(RhinoPlugInId()); }
  plugin_load_time PlugInLoadTime() override { return load_plugin_at_startup; }

  CSelectionMenuPlusPlugIn()
  {
    m_version = RhinoPlugInVersion();
  }

  BOOL OnLoadPlugIn() override
  {
    Settings::Load();
    Settings::History::Load();
    if (ChooseOneHook::Install())
      RhinoApp().Print(L"SelectionMenuPlus: active (mode %ls). Run SelectionMenuPlus for options.\n",
                       Settings::ModeName(Settings::g.mode));
    // Even if the hook fails, stay loaded so the command can report status.
    return TRUE;
  }

  void OnUnloadPlugIn() override
  {
    ChooseOneHook::Uninstall();
  }

private:
  ON_wString m_version;
};

static CSelectionMenuPlusPlugIn thePlugIn;

CRhinoPlugIn& SelectionMenuPlusPlugIn() { return thePlugIn; }

// SelectionMenuPlus command: status and options.
class CCommandSelectionMenuPlus : public CRhinoCommand
{
public:
  CCommandSelectionMenuPlus() : CRhinoCommand(false, false, &thePlugIn) {}
  UUID CommandUUID() override
  {
    static const GUID id = ON_UuidFromString(L"48CD8A68-80D0-43CF-A9C3-29E2DDAC8851");
    return id;
  }
  const wchar_t* EnglishCommandName() override { return L"SelectionMenuPlus"; }

  CRhinoCommand::result RunCommand(const CRhinoCommandContext&) override
  {
    if (!ChooseOneHook::IsInstalled())
    {
      RhinoApp().Print(L"SelectionMenuPlus: hook is NOT installed; trying again...\n");
      if (!ChooseOneHook::Install())
        return CRhinoCommand::failure;
    }
    RhinoApp().Print(L"SelectionMenuPlus: hook installed. %d learned choice(s).\n",
                     Settings::History::Total());

    Settings::Values& v = Settings::g;
    const CRhinoCommandOptionValue modes[] = {RHCMDOPTVALUE(L"Auto"), RHCMDOPTVALUE(L"Suggest"),
                                              RHCMDOPTVALUE(L"Off")};
    for (;;)
    {
      CRhinoGetOption go;
      go.SetCommandPrompt(L"SelectionMenuPlus options");
      go.AcceptNothing();
      const int mode_opt = go.AddCommandOptionList(RHCMDOPTNAME(L"Mode"), 3, modes, static_cast<int>(v.mode));
      go.AddCommandOptionNumber(RHCMDOPTNAME(L"Margin"), &v.margin, L"Score lead needed to auto-pick", false,
                                0.0, 10.0);
      go.AddCommandOptionToggle(RHCMDOPTNAME(L"WheelCycles"), RHCMDOPTVALUE(L"No"), RHCMDOPTVALUE(L"Yes"),
                                v.wheel_cycles, &v.wheel_cycles);
      go.AddCommandOptionToggle(RHCMDOPTNAME(L"Learn"), RHCMDOPTVALUE(L"No"), RHCMDOPTVALUE(L"Yes"), v.learn,
                                &v.learn);
      go.AddCommandOptionToggle(RHCMDOPTNAME(L"Verbose"), RHCMDOPTVALUE(L"No"), RHCMDOPTVALUE(L"Yes"),
                                v.verbose, &v.verbose);
      const int forget_opt = go.AddCommandOption(RHCMDOPTNAME(L"ForgetHistory"));

      const CRhinoGet::result res = go.GetOption();
      if (res == CRhinoGet::option)
      {
        const CRhinoCommandOption* opt = go.Option();
        if (opt && opt->m_option_index == mode_opt)
          v.mode = static_cast<Settings::Mode>(opt->m_list_option_current);
        else if (opt && opt->m_option_index == forget_opt)
        {
          Settings::History::Clear();
          RhinoApp().Print(L"SelectionMenuPlus: learned choices cleared.\n");
        }
        Settings::Save();
        continue;
      }
      Settings::Save();
      if (res == CRhinoGet::cancel)
        return CRhinoCommand::cancel;
      break;
    }
    RhinoApp().Print(L"SelectionMenuPlus: mode %ls, margin %g, wheel %ls, learn %ls, verbose %ls.\n",
                     Settings::ModeName(v.mode), v.margin, v.wheel_cycles ? L"yes" : L"no",
                     v.learn ? L"yes" : L"no", v.verbose ? L"yes" : L"no");
    return CRhinoCommand::success;
  }
};

static CCommandSelectionMenuPlus theSelectionMenuPlusCommand;
