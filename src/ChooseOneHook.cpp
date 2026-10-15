#include "stdafx.h"
#include "ChooseOneHook.h"
#include "MinHook.h"
#include "Ranker.h"
#include "Settings.h"
#include "MenuWheel.h"

namespace ChooseOneHook
{
namespace
{
using ChooseOneFn = int (*)(CRhinoView*, POINT, int, const CRhinoObjRef*, int*, int,
                            ON_SimpleArray<CRhinoObject*>*, bool);

void* g_target = nullptr;              // RhinoChooseOneObject in RhinoCore.dll
ChooseOneFn g_original = nullptr;      // MinHook trampoline to the original
bool g_installed = false;
bool g_minhook_initialized = false;

const wchar_t* ComponentTypeName(ON_COMPONENT_INDEX::TYPE type)
{
  switch (type)
  {
  case ON_COMPONENT_INDEX::invalid_type:         return L"-";
  case ON_COMPONENT_INDEX::brep_vertex:          return L"brep vertex";
  case ON_COMPONENT_INDEX::brep_edge:            return L"brep edge";
  case ON_COMPONENT_INDEX::brep_face:            return L"brep face";
  case ON_COMPONENT_INDEX::brep_trim:            return L"brep trim";
  case ON_COMPONENT_INDEX::brep_loop:            return L"brep loop";
  case ON_COMPONENT_INDEX::mesh_vertex:          return L"mesh vertex";
  case ON_COMPONENT_INDEX::meshtop_vertex:       return L"mesh top vertex";
  case ON_COMPONENT_INDEX::meshtop_edge:         return L"mesh edge";
  case ON_COMPONENT_INDEX::mesh_face:            return L"mesh face";
  case ON_COMPONENT_INDEX::polycurve_segment:    return L"polycurve segment";
  case ON_COMPONENT_INDEX::subd_vertex:          return L"subd vertex";
  case ON_COMPONENT_INDEX::subd_edge:            return L"subd edge";
  case ON_COMPONENT_INDEX::subd_face:            return L"subd face";
  default:                                       return L"other";
  }
}

void LogCandidates(int count, const CRhinoObjRef* refs, int default_index, bool show_all,
                   const Ranker::Result& rank)
{
  RhinoApp().Print(L"SelectionMenuPlus: %d candidates (context: %ls, default: %d, All allowed: %ls)\n",
                   count, static_cast<const wchar_t*>(rank.context), default_index, show_all ? L"yes" : L"no");

  for (const Ranker::Candidate& c : rank.candidates)
  {
    const CRhinoObjRef& ref = refs[c.index];
    const CRhinoObject* obj = ref.Object();
    if (nullptr == obj)
    {
      RhinoApp().Print(L"  [%d] <no object>\n", c.index);
      continue;
    }
    const ON_wString name = obj->Attributes().Name();
    const ON_COMPONENT_INDEX ci = ref.GeometryComponentIndex();
    RhinoApp().Print(L"  [%d] %ls  kind=%ls  name=\"%ls\"  layer=%d  component=%ls[%d]  %ls\n", c.index,
                     obj->ShortDescription(false), static_cast<const wchar_t*>(c.kind),
                     static_cast<const wchar_t*>(name), obj->Attributes().m_layer_index,
                     ComponentTypeName(ci.m_type), ci.m_index,
                     c.skipped ? L"(no object)" : static_cast<const wchar_t*>(c.why));
    if (!c.skipped)
      RhinoApp().Print(L"       score=%.3f%ls\n", c.score, c.index == rank.best ? L"  <- best" : L"");
  }
  RhinoApp().Print(L"  lead=%.3f (auto-pick needs %.3f)\n", rank.lead, Settings::g.margin);
}

int CountVisible(const Ranker::Result& rank)
{
  int visible = 0;
  for (const Ranker::Candidate& c : rank.candidates)
    if (!c.skipped)
      visible++;
  return visible;
}

int Detour(CRhinoView* view, POINT view_point, int objref_count, const CRhinoObjRef* objref_array,
           int* selected_object_index, int default_object_index,
           ON_SimpleArray<CRhinoObject*>* visibility_hint, bool bShowAllOption)
{
  const auto call_original = [&](int default_index) {
    MenuWheelScope wheel;  // wheel cycles the menu while it is open
    return g_original(view, view_point, objref_count, objref_array, selected_object_index, default_index,
                      visibility_hint, bShowAllOption);
  };

  if (Settings::g.mode == Settings::Mode::Off || nullptr == objref_array || nullptr == selected_object_index ||
      objref_count < 2)
    return call_original(default_object_index);

  const Ranker::Result rank = Ranker::Rank(objref_count, objref_array);
  if (Settings::g.verbose)
    LogCandidates(objref_count, objref_array, default_object_index, bShowAllOption, rank);

  // Nothing to decide: let Rhino reduce it to one the way it normally does.
  if (rank.best < 0 || CountVisible(rank) < 2)
    return call_original(default_object_index);

  // Rhino folds group members that were not hit into their group's row, and
  // which ones it folds is not exposed by the SDK. Leave grouped picks to Rhino.
  if (rank.grouped)
  {
    if (Settings::g.verbose)
      RhinoApp().Print(L"SelectionMenuPlus: grouped objects involved, using Rhino's menu\n");
    return call_original(default_object_index);
  }

  if (Settings::g.mode == Settings::Mode::Auto && rank.lead >= Settings::g.margin)
  {
    *selected_object_index = rank.best;
    if (Settings::g.verbose)
      RhinoApp().Print(L"SelectionMenuPlus: auto-picked [%d] %ls\n", rank.best,
                       static_cast<const wchar_t*>(rank.candidates[rank.best].kind));
    return 1;  // "one object selected"
  }

  // Tie or Suggest mode: Rhino's menu, with our best guess highlighted.
  const int default_index = default_object_index >= 0 ? default_object_index : rank.best;
  const int rc = call_original(default_index);

  if (rc == 1 && Settings::g.learn && *selected_object_index >= 0 && *selected_object_index < objref_count)
  {
    const Ranker::Candidate& chosen = rank.candidates[*selected_object_index];
    Settings::History::Record(rank.context, chosen.kind);
    if (Settings::g.verbose)
      RhinoApp().Print(L"SelectionMenuPlus: learned %ls -> %ls\n", static_cast<const wchar_t*>(rank.context),
                       static_cast<const wchar_t*>(chosen.kind));
  }
  else if (Settings::g.verbose)
    RhinoApp().Print(L"SelectionMenuPlus: Rhino returned %d, index %d\n", rc, *selected_object_index);
  return rc;
}
} // namespace

bool Install()
{
  if (g_installed)
    return true;

  // Taking the address of the dllimport'ed SDK function yields its real
  // address inside RhinoCore.dll (resolved through the import table).
  g_target = reinterpret_cast<void*>(&::RhinoChooseOneObject);

  MH_STATUS status = MH_OK;
  if (!g_minhook_initialized)
  {
    status = MH_Initialize();
    if (status != MH_OK)
    {
      RhinoApp().Print(L"SelectionMenuPlus: MH_Initialize failed (%hs)\n", MH_StatusToString(status));
      return false;
    }
    g_minhook_initialized = true;
  }

  status = MH_CreateHook(g_target, reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_original));
  if (status != MH_OK)
  {
    RhinoApp().Print(L"SelectionMenuPlus: MH_CreateHook failed (%hs)\n", MH_StatusToString(status));
    return false;
  }

  status = MH_EnableHook(g_target);
  if (status != MH_OK)
  {
    RhinoApp().Print(L"SelectionMenuPlus: MH_EnableHook failed (%hs)\n", MH_StatusToString(status));
    MH_RemoveHook(g_target);
    return false;
  }

  g_installed = true;
  return true;
}

void Uninstall()
{
  if (g_installed)
  {
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    g_installed = false;
  }
  if (g_minhook_initialized)
  {
    MH_Uninitialize();
    g_minhook_initialized = false;
  }
}

bool IsInstalled()
{
  return g_installed;
}
} // namespace ChooseOneHook
