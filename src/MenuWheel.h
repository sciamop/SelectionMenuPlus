#pragma once

// While Rhino's selection menu is open, turns the mouse wheel into Up/Down
// arrow presses on the menu's list (wrapping at the ends) and a middle-click
// anywhere over Rhino into Enter. Create one on the stack around the call to Rhino's menu; the
// Windows hooks exist only for that scope.
class MenuWheelScope
{
public:
  MenuWheelScope();
  ~MenuWheelScope();
  MenuWheelScope(const MenuWheelScope&) = delete;
  MenuWheelScope& operator=(const MenuWheelScope&) = delete;

private:
  HHOOK m_cbt = nullptr;
  HHOOK m_msg = nullptr;
  HHOOK m_mouse_ll = nullptr;
  bool m_active = false;
};
