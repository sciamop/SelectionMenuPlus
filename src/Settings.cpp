#include "stdafx.h"
#include "Settings.h"
#include <ShlObj.h>
#include <map>
#include <string>
#include <cstdio>

namespace Settings
{
Values g;

namespace
{
const wchar_t* kSection = L"SelectionMenuPlus";

std::wstring DataDir()
{
  std::wstring dir;
  PWSTR roaming = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming)))
  {
    dir = roaming;
    dir += L"\\SelectionMenuPlus";
    CreateDirectoryW(dir.c_str(), nullptr);
  }
  CoTaskMemFree(roaming);
  return dir;
}

std::wstring IniPath() { return DataDir() + L"\\settings.ini"; }
std::wstring HistoryPath() { return DataDir() + L"\\history.tsv"; }

int ReadInt(const wchar_t* key, int def)
{
  return static_cast<int>(GetPrivateProfileIntW(kSection, key, def, IniPath().c_str()));
}

double ReadDouble(const wchar_t* key, double def)
{
  wchar_t buf[64] = {};
  GetPrivateProfileStringW(kSection, key, L"", buf, 64, IniPath().c_str());
  if (buf[0] == 0)
    return def;
  wchar_t* end = nullptr;
  const double v = wcstod(buf, &end);
  return end != buf ? v : def;
}

void WriteInt(const wchar_t* key, int v)
{
  WritePrivateProfileStringW(kSection, key, std::to_wstring(v).c_str(), IniPath().c_str());
}

void WriteDouble(const wchar_t* key, double v)
{
  wchar_t buf[64];
  swprintf_s(buf, L"%g", v);
  WritePrivateProfileStringW(kSection, key, buf, IniPath().c_str());
}
} // namespace

const wchar_t* ModeName(Mode m)
{
  switch (m)
  {
  case Mode::Auto:    return L"Auto";
  case Mode::Suggest: return L"Suggest";
  case Mode::Off:     return L"Off";
  }
  return L"?";
}

void Load()
{
  const int mode = ReadInt(L"Mode", static_cast<int>(g.mode));
  g.mode = (mode >= 0 && mode <= 2) ? static_cast<Mode>(mode) : Mode::Auto;
  g.margin = ReadDouble(L"Margin", g.margin);
  if (g.margin < 0.0 || g.margin > 10.0)
    g.margin = 0.35;
  g.learn = ReadInt(L"Learn", g.learn) != 0;
  g.wheel_cycles = ReadInt(L"WheelCycles", g.wheel_cycles) != 0;
  g.verbose = ReadInt(L"Verbose", g.verbose) != 0;
}

void Save()
{
  WriteInt(L"Mode", static_cast<int>(g.mode));
  WriteDouble(L"Margin", g.margin);
  WriteInt(L"Learn", g.learn);
  WriteInt(L"WheelCycles", g.wheel_cycles);
  WriteInt(L"Verbose", g.verbose);
}

namespace History
{
namespace
{
// context -> kind -> count
std::map<std::wstring, std::map<std::wstring, int>> g_counts;
}

void Load()
{
  g_counts.clear();
  FILE* f = nullptr;
  if (_wfopen_s(&f, HistoryPath().c_str(), L"rt, ccs=UTF-8") != 0 || f == nullptr)
    return;
  wchar_t line[512];
  while (fgetws(line, 512, f))
  {
    // context \t kind \t count
    wchar_t* ctx_end = wcschr(line, L'\t');
    if (!ctx_end)
      continue;
    *ctx_end = 0;
    wchar_t* kind = ctx_end + 1;
    wchar_t* kind_end = wcschr(kind, L'\t');
    if (!kind_end)
      continue;
    *kind_end = 0;
    const int count = _wtoi(kind_end + 1);
    if (count > 0)
      g_counts[line][kind] = count;
  }
  fclose(f);
}

void Save()
{
  FILE* f = nullptr;
  if (_wfopen_s(&f, HistoryPath().c_str(), L"wt, ccs=UTF-8") != 0 || f == nullptr)
    return;
  for (const auto& ctx : g_counts)
    for (const auto& kind : ctx.second)
      fwprintf(f, L"%ls\t%ls\t%d\n", ctx.first.c_str(), kind.first.c_str(), kind.second);
  fclose(f);
}

void Record(const ON_wString& context, const ON_wString& kind)
{
  int& c = g_counts[static_cast<const wchar_t*>(context)][static_cast<const wchar_t*>(kind)];
  // Cap so old habits can still be unlearned in reasonable time.
  if (c < 50)
    c++;
  else
    for (auto& k : g_counts[static_cast<const wchar_t*>(context)])
      k.second = k.second / 2;
  Save();
}

int Count(const ON_wString& context, const ON_wString& kind)
{
  const auto ctx = g_counts.find(static_cast<const wchar_t*>(context));
  if (ctx == g_counts.end())
    return 0;
  const auto k = ctx->second.find(static_cast<const wchar_t*>(kind));
  return k == ctx->second.end() ? 0 : k->second;
}

int Total()
{
  int total = 0;
  for (const auto& ctx : g_counts)
    for (const auto& kind : ctx.second)
      total += kind.second;
  return total;
}

void Clear()
{
  g_counts.clear();
  Save();
}
} // namespace History
} // namespace Settings
