# SelectionMenuPlus

A Rhino 8 (Windows) plug-in that takes over Rhino's **selection menu**: the
"choose one object" popup that appears when a click hits several overlapping
objects.

The long-term goal is to pick the right object automatically: rank the
candidates using context such as the active command, sub-object type and your
past choices. The popup would then appear only for genuine ties, as a
non-blocking overlay or by clicking again to cycle.

> **Status: milestone 2 (context ranking).** When a click hits several objects,
> the plug-in scores the candidates and **picks the clear winner itself**. Rhino's
> menu appears only for real ties. Your choices in
> that menu are remembered per command, so the ranking adapts to you.

---

## Requirements

- **Rhino 8.35 or later.** Older service releases reject the plug-in with
  *"designed to run on a service release of Rhino 8…"*. Update with
  *Help → Check for Updates*.
- Windows 10/11, x64. There is no Mac version.

## Install

1. In Rhino, run `PackageManager`.
2. Search for **SelectionMenuPlus** and click **Install**.
3. Restart Rhino. The command line should print:
   ```
   SelectionMenuPlus: active (mode Auto). Run SelectionMenuPlus for options.
   ```

Rhino loads the plug-in automatically at every startup from then on.

You can also download the `.yak` package from the
[Releases](../../releases) page and install it with
`yak install <file>.yak`, or drag the `.rhp` inside it onto Rhino.

**Updating:** `PackageManager` → **Updates** tab, then restart Rhino.

If you built the plug-in yourself (see [Building](#building)), drag
`bin\SelectionMenuPlus.rhp` onto the Rhino window instead. Rhino locks the
`.rhp` while it runs, so close Rhino before replacing it with a new build.

## Usage

Click as usual. For an ambiguous click, one of two things happens:

| Situation | What happens |
|---|---|
| One candidate clearly scores best | It is selected straight away, with no menu |
| Two or more candidates score about the same | Rhino's normal menu, with the plug-in's best guess pre-highlighted |

When the menu does appear, the plug-in records which **kind** of thing you
chose in that **context**, for example `brep/face` while in `<idle>`, or
`curve` in `Join`. After a couple of consistent choices it starts auto-picking
that kind for you. If it keeps picking wrong, switch `Mode` to `Suggest` for a
while: the menu then always appears, and every choice teaches it.

### Driving the menu with the mouse wheel

While Rhino's menu is open:

| Input | Action |
|---|---|
| Wheel down / up | Move to the next / previous row, wrapping at the ends, the same as pressing the arrow keys |
| Middle-click (press the wheel) anywhere over Rhino | Accept the highlighted row. The cursor can be over the viewport; it does not need to be on the menu. |
| Left-click a row, or Enter | Accept, as usual |
| Esc | Cancel, as usual |

So when the menu appears, the fastest way through it is to roll
the wheel until the right object lights up, then **click the wheel**. You never
have to move the mouse over to the menu.

Each wheel notch is delivered to the menu as an Up or Down arrow key, and
middle-click as Enter, so Rhino's own keyboard handling does the work. While
the menu is open the viewport does not zoom. The Windows hooks exist only
while the menu is on screen.

The modal menu disables Rhino's main window, and Windows sends no mouse
messages to disabled windows. A middle-click over a viewport therefore never
reaches Rhino's message queue. While the menu is open, the plug-in watches the
middle button with a low-level mouse hook (`WH_MOUSE_LL`). It reacts only when
Rhino is the active app and the cursor is over a Rhino window, and it swallows
both the press and the release so nothing else sees the click.

### How candidates are scored

| Signal | Effect |
|---|---|
| Thin targets | Points and vertices (+1.0), then curves and edges (+0.8), when the cursor is right on them. They are hard to hit by accident, so a hit means you were aiming at them. Surfaces get no bonus. |
| Pick distance | −0.3 per unit of Rhino's pick distance (0 = direct hit, 2 = corner of the pick box) |
| Depth | Slightly favours what is nearer the camera (±0.15) |
| Ctrl / Shift | With no command running, Ctrl+click favours already-selected objects (you are deselecting). Shift+click favours unselected ones (you are adding). |
| Learned history | Up to ±1.0 toward the kinds you picked from the menu in this context |
| Rhino's order | A small tiebreak (up to −0.15) for candidates Rhino listed later |

A candidate is auto-picked when its lead over the runner-up is at least
**Margin** (0.35 by default). Raise Margin for more menus, lower it for fewer.

### Command: `SelectionMenuPlus`

Shows the hook status and the number of learned choices, then offers options.
Options are saved immediately.

| Option | Default | Meaning |
|---|---|---|
| `Mode` | `Auto` | `Auto`: pick clear winners, menu on ties. `Suggest`: always show the menu, with the best guess highlighted. `Off`: stock Rhino behaviour. |
| `Margin` | `0.35` | Score lead needed to auto-pick |
| `WheelCycles` | `Yes` | Wheel moves through the open menu, middle-click accepts |
| `Learn` | `Yes` | Remember choices made in the menu |
| `Verbose` | `No` | Print every candidate, its score and the decision to the command line |
| `ForgetHistory` | | Clear all learned choices |

Settings and history live in `%APPDATA%\SelectionMenuPlus\` as `settings.ini`
and `history.tsv`. Both are plain text and safe to edit or delete while Rhino
is closed.

### Verbose output

```
SelectionMenuPlus: 2 candidates (context: <idle>, default: -1, All allowed: yes)
  [0] polysurface  kind=brep  name=""  layer=0  component=-[-1]  shape=2 dist=0.00 depth=0.41 sel=0 hist=+0.00
       score=0.062
  [1] curve  kind=curve  name="rail"  layer=2  component=-[-1]  shape=1 dist=0.30 depth=0.43 sel=0 hist=+0.00
       score=0.505  <- best
  lead=0.443 (auto-pick needs 0.350)
SelectionMenuPlus: auto-picked [1] curve
```

`shape` is 0 for point-like, 1 for line-like and 2 for area-like.

When any candidate belongs to a group, the plug-in does not auto-pick. Rhino
folds group members into their group's row, so the click is left to Rhino's
own menu, which the mouse wheel still drives.

### Testing tips

- Make sure Rhino's own bypass is off, or the menu (and the hook) never fires:
  run `TestBypassSelectionMenu` until it prints
  *"Disabled bypassing selection menu."*
- Turn on `Verbose` to see why something was or was not auto-picked.
- Try both plain clicking and picks inside commands (`FilletEdge`, `Join`,
  sub-object selection with Ctrl+Shift+click, and so on).

## Uninstall / troubleshooting

- **Turn it off for a while:** `SelectionMenuPlus` → `Mode` → `Off`. Rhino's
  stock menu comes back immediately.
- **Uninstall:** `PackageManager` → **Installed** tab → **SelectionMenuPlus** →
  **Uninstall**, then restart Rhino.
- **Rhino crashes at startup or on click:** start Rhino, run `PluginManager`,
  uncheck **SelectionMenuPlus** and restart. Please report what you were doing
  when it crashed.
- **"Designed to run on a service release…":** your Rhino is older than 8.35.
  Update Rhino.
- **"hook is NOT installed":** the plug-in prints the MinHook error. Rhino keeps
  working normally with its original menu.
- **It keeps picking the wrong thing:** raise `Margin`, or set `Mode` to
  `Suggest` and choose the right object a couple of times.
- **Learned something silly:** `SelectionMenuPlus` → `ForgetHistory`.

## How it works

Every ambiguous pick in Rhino, whether in idle selection or inside any command,
goes through one exported function in `RhinoCore.dll`:

```cpp
int RhinoChooseOneObject(CRhinoView*, POINT, int objref_count,
                         const CRhinoObjRef* objref_array, int* selected_object_index,
                         int default_object_index, ON_SimpleArray<CRhinoObject*>* visibility_hint,
                         bool bShowAllOption);
```

On load, the plug-in redirects this function with
[MinHook](https://github.com/TsudaKageyu/minhook) (v1.3.4, in `third_party/`).
Rhino still does its own picking, depth culling, filtering and selection. Only
the "which one?" decision passes through the plug-in. To auto-pick, the hook
writes the chosen index and returns `1` ("one object selected"), exactly as if
you had clicked that row. Otherwise it calls Rhino's original menu.

The ranking uses only the public SDK. Rhino folds group members into their
group's row, and the SDK does not expose which ones, so picks that involve a
grouped object always go to Rhino's menu. The function is located
by its exported name, not a hard-coded address, so it survives Rhino updates
as long as the signature stays the same. Unloading the plug-in removes the hook.

Before the menu is ever shown, Rhino already removes some candidates
automatically, using the hidden test commands `PickRejectionRatio` (default
0.15) and `PickRejectionFuzz` (default 160). `TestBypassSelectionMenu` turns
the menu off entirely and takes the best candidate.

**Caveat:** hooking a Rhino function is not supported by McNeel. Set `Mode`
to `Off` or disable the plug-in before reporting Rhino bugs to McNeel.

## Building

Requires the **Rhino 8 C++ SDK**, an MSVC C++ compiler with MSBuild C++ support,
and **MFC** headers and libraries.

```bash
"C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
  SelectionMenuPlus.vcxproj -p:Configuration=Release -p:Platform=x64
```

Output: `bin\SelectionMenuPlus.rhp`.

The project uses toolset `v145` (Build Tools 2026). Because the v145 Build
Tools do not ship MFC, the project sets `UseOfMfc=false` and points at an MFC
install through the `MfcDir` property, which defaults to VS 2022 Community's
MSVC 14.43 `atlmfc` folder. Override it to match your machine:

```bash
MSBuild.exe SelectionMenuPlus.vcxproj -p:Configuration=Release -p:Platform=x64 -p:MfcDir="<path>\atlmfc"
```

### Packaging

Copy `bin\SelectionMenuPlus.rhp`, `packaging\manifest.yml`,
`packaging\icon.png`, `README.md`, `LICENSE.txt` and `THIRD_PARTY_NOTICES.txt`
into one folder, bump `version` in `manifest.yml` (and `RHINO_PLUG_IN_VERSION`
in `src/SelectionMenuPlusPlugIn.cpp`), then run `yak build --platform win`
there.

### Layout

```
SelectionMenuPlus.vcxproj
src/
  stdafx.h / .cpp              precompiled header (MFC + Rhino SDK)
  SelectionMenuPlusPlugIn.cpp  plug-in declaration, load/unload, SelectionMenuPlus command
  ChooseOneHook.h / .cpp       the RhinoChooseOneObject hook: rank, auto-pick or fall back to the menu
  Ranker.h / .cpp              candidate classification and scoring
  Settings.h / .cpp            options (settings.ini) and learned history (history.tsv)
  MenuWheel.h / .cpp           wheel / middle-click control of the open menu
third_party/minhook/           MinHook v1.3.4 (BSD-2-Clause), vendored unmodified
packaging/                     Yak manifest.yml and icon.png
bin/, obj/                     build output and intermediates (not committed)
```

## Roadmap

1. ~~Hook + candidate logging~~ (milestone 1)
2. ~~**Context ranking:** auto-pick using target shape, pick distance, Ctrl/Shift
   intent and learned per-command preferences; fall back to Rhino's menu on
   ties~~ (milestone 2)
   - ~~Mouse wheel cycles the open menu, middle-click accepts~~ (current)
3. **Click-to-cycle:** clicking again at the same spot takes the next candidate
4. **Non-modal overlay:** a cursor-side list, with the scroll wheel or Tab to
   cycle, keys 1–9 to pick, and no focus stealing

## License

MIT. See [LICENSE.txt](LICENSE.txt). MinHook is BSD-2-Clause; see
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
