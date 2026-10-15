#include "stdafx.h"
#include "Ranker.h"
#include "Settings.h"

namespace Ranker
{
namespace
{
// Score weights. Distances and depths are Rhino's normalized pick values.
constexpr double kOrderWeight = 0.15;     // Rhino's own sort order, weak tiebreak
constexpr double kPointBonus = 1.0;       // a point-like target hit precisely
constexpr double kLineBonus = 0.8;        // a line-like target hit precisely
constexpr double kDistanceWeight = 0.3;   // per unit of pick distance (0..2)
constexpr double kDepthWeight = 0.15;     // per unit of depth (-1..1)
constexpr double kSelectionWeight = 0.8;  // Ctrl+click prefers selected, Shift+click unselected
constexpr double kHistoryWeight = 2.0;    // learned preference
constexpr double kHistoryPrior = 1.0;     // Laplace smoothing for history counts

const wchar_t* ObjectTypeKey(ON::object_type t)
{
  switch (t)
  {
  case ON::point_object:       return L"point";
  case ON::pointset_object:    return L"pointcloud";
  case ON::curve_object:       return L"curve";
  case ON::surface_object:     return L"surface";
  case ON::brep_object:        return L"brep";
  case ON::extrusion_object:   return L"extrusion";
  case ON::mesh_object:        return L"mesh";
  case ON::subd_object:        return L"subd";
  case ON::annotation_object:  return L"annotation";
  case ON::text_dot:     return L"dot";
  case ON::hatch_object:       return L"hatch";
  case ON::instance_reference: return L"block";
  case ON::light_object:       return L"light";
  case ON::grip_object:        return L"grip";
  case ON::clipplane_object:   return L"clippingplane";
  case ON::detail_object:      return L"detail";
  default:                     return L"other";
  }
}

const wchar_t* ComponentKey(ON_COMPONENT_INDEX::TYPE t)
{
  switch (t)
  {
  case ON_COMPONENT_INDEX::brep_vertex:
  case ON_COMPONENT_INDEX::mesh_vertex:
  case ON_COMPONENT_INDEX::meshtop_vertex:
  case ON_COMPONENT_INDEX::subd_vertex:
  case ON_COMPONENT_INDEX::pointcloud_point:
    return L"vertex";
  case ON_COMPONENT_INDEX::brep_edge:
  case ON_COMPONENT_INDEX::brep_trim:
  case ON_COMPONENT_INDEX::meshtop_edge:
  case ON_COMPONENT_INDEX::subd_edge:
    return L"edge";
  case ON_COMPONENT_INDEX::polycurve_segment:
    return L"segment";
  case ON_COMPONENT_INDEX::brep_face:
  case ON_COMPONENT_INDEX::mesh_face:
  case ON_COMPONENT_INDEX::subd_face:
    return L"face";
  case ON_COMPONENT_INDEX::brep_loop:
    return L"loop";
  default:
    return nullptr;
  }
}

Shape ShapeOf(ON::object_type type, ON_COMPONENT_INDEX::TYPE comp)
{
  const wchar_t* ck = ComponentKey(comp);
  if (ck)
  {
    if (0 == wcscmp(ck, L"vertex"))
      return Shape::PointLike;
    if (0 == wcscmp(ck, L"edge") || 0 == wcscmp(ck, L"segment") || 0 == wcscmp(ck, L"loop"))
      return Shape::LineLike;
    return Shape::AreaLike;
  }
  switch (type)
  {
  case ON::point_object:
  case ON::pointset_object:
  case ON::text_dot:
  case ON::grip_object:
  case ON::light_object:
    return Shape::PointLike;
  case ON::curve_object:
  case ON::annotation_object:
  case ON::clipplane_object:
    return Shape::LineLike;
  default:
    return Shape::AreaLike;
  }
}
} // namespace

ON_wString ContextKey()
{
  const CRhinoCommand* cmd = RhinoApp().ActiveCommand();
  return cmd ? ON_wString(cmd->EnglishCommandName()) : ON_wString(L"<idle>");
}

ON_wString KindOf(const CRhinoObjRef& ref)
{
  const CRhinoObject* obj = ref.Object();
  ON_wString kind = obj ? ObjectTypeKey(obj->ObjectType()) : L"none";
  if (const wchar_t* ck = ComponentKey(ref.GeometryComponentIndex().m_type))
  {
    kind += L"/";
    kind += ck;
  }
  return kind;
}

Result Rank(int count, const CRhinoObjRef* refs)
{
  Result r;
  r.context = ContextKey();
  const bool idle = nullptr == RhinoApp().ActiveCommand();
  const bool ctrl = GetKeyState(VK_CONTROL) < 0;
  const bool shift = GetKeyState(VK_SHIFT) < 0;

  // Count how many distinct kinds are in play, for the history term.
  ON_ClassArray<ON_wString> kinds;
  for (int i = 0; i < count; i++)
  {
    Candidate c;
    c.index = i;
    const CRhinoObjRef& ref = refs[i];
    const CRhinoObject* obj = ref.Object();
    c.kind = KindOf(ref);
    c.shape = obj ? ShapeOf(obj->ObjectType(), ref.GeometryComponentIndex().m_type) : Shape::AreaLike;
    c.selected = obj && obj->IsSelected() != 0;
    c.distance = ON_IsValid(ref.SelectionDistance()) ? ref.SelectionDistance() : 0.0;
    c.depth = ON_IsValid(ref.SelectionDepth()) ? ref.SelectionDepth() : 0.0;
    c.skipped = nullptr == obj;
    if (obj && obj->Attributes().GroupCount() > 0)
      r.grouped = true;
    if (!c.skipped)
    {
      bool seen = false;
      for (int k = 0; k < kinds.Count() && !seen; k++)
        seen = kinds[k] == c.kind;
      if (!seen)
        kinds.Append(c.kind);
    }
    r.candidates.push_back(c);
  }

  // History: smoothed share of past menu choices among the kinds on offer.
  double history_total = 0.0;
  for (int k = 0; k < kinds.Count(); k++)
    history_total += Settings::History::Count(r.context, kinds[k]) + kHistoryPrior;

  const int n = count > 1 ? count - 1 : 1;
  for (Candidate& c : r.candidates)
  {
    if (c.skipped)
      continue;
    ON_wString why;
    double s = 0.0;

    const double order = -kOrderWeight * c.index / n;
    s += order;

    // A thin target that the cursor actually touched was almost certainly
    // aimed at: it is much harder to hit by accident than a surface.
    const double closeness = ON_Max(0.0, 1.0 - c.distance / 2.0);
    if (c.shape == Shape::PointLike)
      s += kPointBonus * closeness;
    else if (c.shape == Shape::LineLike)
      s += kLineBonus * closeness;

    s -= kDistanceWeight * c.distance;
    s += kDepthWeight * c.depth;

    if (idle && ctrl && !shift)
      s += c.selected ? kSelectionWeight : -kSelectionWeight;  // Ctrl+click deselects
    else if (idle && shift && !ctrl)
      s += c.selected ? -kSelectionWeight : kSelectionWeight;  // Shift+click adds

    double hist = 0.0;
    if (kinds.Count() > 1 && history_total > 0.0)
    {
      const double share = (Settings::History::Count(r.context, c.kind) + kHistoryPrior) / history_total;
      hist = kHistoryWeight * (share - 1.0 / kinds.Count());
      s += hist;
    }

    c.score = s;
    c.why.Format(L"shape=%d dist=%.2f depth=%.2f sel=%d hist=%+.2f", static_cast<int>(c.shape), c.distance,
                 c.depth, c.selected ? 1 : 0, hist);
  }

  double best = -1e300, second = -1e300;
  for (const Candidate& c : r.candidates)
  {
    if (c.skipped)
      continue;
    if (c.score > best)
    {
      second = best;
      r.runner_up = r.best;
      best = c.score;
      r.best = c.index;
    }
    else if (c.score > second)
    {
      second = c.score;
      r.runner_up = c.index;
    }
  }
  r.lead = (r.best >= 0 && r.runner_up >= 0) ? best - second : (r.best >= 0 ? 1e9 : 0.0);
  return r;
}
} // namespace Ranker
