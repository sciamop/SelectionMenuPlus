#pragma once

// User settings and learned pick history, persisted under
// %APPDATA%\SelectionMenuPlus\ (settings.ini and history.tsv).
namespace Settings
{
enum class Mode : int
{
  Auto = 0,     // auto-pick clear winners, Rhino's menu for ties
  Suggest = 1,  // always show Rhino's menu, with the best guess pre-highlighted
  Off = 2,      // pure pass-through (Rhino's stock behaviour)
};

struct Values
{
  Mode mode = Mode::Auto;
  double margin = 0.35;          // score lead needed to auto-pick
  bool learn = true;             // learn from choices made in the menu
  bool wheel_cycles = true;      // wheel moves through the menu, middle-click accepts
  bool verbose = false;          // print candidates and scores
};

extern Values g;

void Load();
void Save();
const wchar_t* ModeName(Mode m);

// Learned history: how often a kind of candidate (e.g. "curve", "brep/edge")
// was chosen from the menu in a context (command name, or "<idle>").
namespace History
{
void Load();
void Save();
void Record(const ON_wString& context, const ON_wString& kind);
int Count(const ON_wString& context, const ON_wString& kind);
int Total();                                   // all recorded choices
void Clear();
}
} // namespace Settings
