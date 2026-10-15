#include "stdafx.h"
#include "MenuWheel.h"
#include "Settings.h"

namespace
{
// State for the one menu that can be open at a time (it is modal, on the UI thread).
int g_depth = 0;
HWND g_list = nullptr;       // the menu's list box, found as it is created
int g_wheel_accum = 0;       // partial wheel deltas from high-resolution wheels/touchpads
bool g_mbutton_down = false; // a middle-button press was swallowed; its release confirms

// Watches window creation while the menu is up, to find its list box without
// relying on Rhino's private dialog layout. The dialog can be rebuilt while
// open, so the most recent list box wins.
LRESULT CALLBACK CbtProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (code == HCBT_CREATEWND)
  {
    // Ask the window for its class: dialog templates pass the class as an
    // atom, so lpcs->lpszClass is not always a string.
    const HWND hwnd = reinterpret_cast<HWND>(wParam);
    wchar_t cls[32] = {};
    if (GetClassNameW(hwnd, cls, 32) > 0 && 0 == _wcsicmp(cls, L"ListBox"))
      g_list = hwnd;
  }
  else if (code == HCBT_DESTROYWND && reinterpret_cast<HWND>(wParam) == g_list)
    g_list = nullptr;
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

// Rewrites a queued message into a key press on the list box, exactly as if
// the key had been typed, so Rhino's own keyboard handling (selection,
// highlight, Enter to accept) runs unchanged.
void RewriteAsKey(MSG* msg, UINT vk)
{
  msg->hwnd = g_list;
  msg->message = WM_KEYDOWN;
  msg->wParam = vk;
  msg->lParam = 1;  // repeat count 1
}

void Swallow(MSG* msg)
{
  msg->message = WM_NULL;
  msg->wParam = 0;
  msg->lParam = 0;
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wParam, LPARAM lParam)
{
  // Only touch messages actually being removed from the queue; peeks must not
  // be altered or the same message would be processed twice.
  if (code == HC_ACTION && wParam == PM_REMOVE && g_list && IsWindow(g_list) && IsWindowVisible(g_list))
  {
    MSG* msg = reinterpret_cast<MSG*>(lParam);
    switch (msg->message)
    {
    case WM_MOUSEWHEEL:
    {
      g_wheel_accum += GET_WHEEL_DELTA_WPARAM(msg->wParam);
      if (g_wheel_accum >= WHEEL_DELTA || g_wheel_accum <= -WHEEL_DELTA)
      {
        const bool up = g_wheel_accum > 0;
        g_wheel_accum = 0;
        const LRESULT count = SendMessageW(g_list, LB_GETCOUNT, 0, 0);
        const LRESULT cur = SendMessageW(g_list, LB_GETCURSEL, 0, 0);
        UINT vk = up ? VK_UP : VK_DOWN;
        if (count > 0 && cur != LB_ERR)
        {
          if (!up && cur >= count - 1)
            vk = VK_HOME;  // wrap to the top
          else if (up && cur <= 0)
            vk = VK_END;   // wrap to the bottom
        }
        else if (count > 0)
          vk = up ? VK_END : VK_HOME;  // nothing selected yet
        RewriteAsKey(msg, vk);
      }
      else
        Swallow(msg);
      break;
    }
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
      // Fallback for when the low-level hook is unavailable. Swallow the press
      // so the viewport never sees it; act on release.
      g_mbutton_down = true;
      Swallow(msg);
      break;
    case WM_MBUTTONUP:
      if (g_mbutton_down)
      {
        g_mbutton_down = false;
        RewriteAsKey(msg, VK_RETURN);
      }
      break;
    default:
      break;
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool OwnedByThisProcess(HWND hwnd)
{
  DWORD pid = 0;
  return hwnd && GetWindowThreadProcessId(hwnd, &pid) && pid == GetCurrentProcessId();
}

// True when a middle-click at pt is meant for Rhino: Rhino is the active app
// and the cursor is over one of its windows (the menu, or the main window
// and viewports that the modal menu has disabled).
bool IsClickForRhino(POINT pt)
{
  if (!OwnedByThisProcess(GetForegroundWindow()))
    return false;
  if (OwnedByThisProcess(WindowFromPoint(pt)))
    return true;
  RECT rc;
  const HWND root = GetAncestor(g_list, GA_ROOTOWNER);
  return root && GetWindowRect(root, &rc) && PtInRect(&rc, pt);
}

// The modal menu disables Rhino's main window, and Windows sends no mouse
// messages at all to disabled windows, so a middle-click over a viewport
// never reaches the message queue. A low-level mouse hook sees it anyway.
LRESULT CALLBACK MouseLLProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (code == HC_ACTION && (wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP) && g_list &&
      IsWindow(g_list) && IsWindowVisible(g_list))
  {
    // MSLLHOOKSTRUCT::pt is in physical pixels, while WindowFromPoint and
    // GetWindowRect use this process's DPI-scaled coordinates; GetCursorPos
    // matches them.
    POINT pt;
    if (wParam == WM_MBUTTONDOWN && GetCursorPos(&pt) && IsClickForRhino(pt))
    {
      g_mbutton_down = true;
      return 1;  // swallow
    }
    if (wParam == WM_MBUTTONUP && g_mbutton_down)
    {
      g_mbutton_down = false;
      PostMessageW(g_list, WM_KEYDOWN, VK_RETURN, 1);
      return 1;  // swallow
    }
  }
  return CallNextHookEx(nullptr, code, wParam, lParam);
}
} // namespace

MenuWheelScope::MenuWheelScope()
{
  if (!Settings::g.wheel_cycles || g_depth > 0)
    return;
  g_list = nullptr;
  g_wheel_accum = 0;
  g_mbutton_down = false;
  const DWORD thread = GetCurrentThreadId();
  m_cbt = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, thread);
  m_msg = SetWindowsHookExW(WH_GETMESSAGE, GetMsgProc, nullptr, thread);
  if (!m_cbt || !m_msg)
  {
    if (m_cbt)
      UnhookWindowsHookEx(m_cbt);
    if (m_msg)
      UnhookWindowsHookEx(m_msg);
    m_cbt = m_msg = nullptr;
    return;
  }
  // Low-level hooks are global and need a module handle; they are called on
  // this thread while the modal menu pumps messages. If this fails, the
  // GetMsgProc fallback still handles middle-clicks over the menu itself.
  HMODULE self = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&MouseLLProc), &self);
  m_mouse_ll = SetWindowsHookExW(WH_MOUSE_LL, MouseLLProc, self, 0);
  m_active = true;
  g_depth++;
}

MenuWheelScope::~MenuWheelScope()
{
  if (!m_active)
    return;
  if (m_mouse_ll)
    UnhookWindowsHookEx(m_mouse_ll);
  UnhookWindowsHookEx(m_msg);
  UnhookWindowsHookEx(m_cbt);
  g_list = nullptr;
  g_depth--;
}
