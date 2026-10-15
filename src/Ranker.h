#pragma once
#include <vector>

// Context-based ranking of the candidates Rhino would put in its selection menu.
namespace Ranker
{
enum class Shape : int
{
  PointLike = 0,  // points, vertices, grips, dots
  LineLike = 1,   // curves, edges, segments, annotations
  AreaLike = 2,   // surfaces, faces, polysurfaces, meshes, SubDs, hatches
};

struct Candidate
{
  int index = -1;          // index into Rhino's objref array
  ON_wString kind;         // e.g. "curve", "brep/edge", "subd/face"
  Shape shape = Shape::AreaLike;
  bool skipped = false;    // no object behind this objref
  bool selected = false;   // object already selected
  double distance = 0.0;   // 0 = direct hit, 2 = pick box corner
  double depth = 0.0;      // -1 = far, 1 = near
  double score = 0.0;
  ON_wString why;          // short explanation of the score, for verbose logging
};

struct Result
{
  std::vector<Candidate> candidates;  // in Rhino's original order
  int best = -1;                      // objref index of the top candidate
  int runner_up = -1;                 // objref index of the second candidate
  double lead = 0.0;                  // best score minus runner-up score
  ON_wString context;                 // command name or "<idle>"
  bool grouped = false;               // a candidate belongs to a group; Rhino folds those rows itself
};

ON_wString ContextKey();
ON_wString KindOf(const CRhinoObjRef& ref);
Result Rank(int count, const CRhinoObjRef* refs);
} // namespace Ranker
