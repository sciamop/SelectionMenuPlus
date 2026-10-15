#pragma once

// Redirects Rhino's RhinoChooseOneObject() (the "selection menu") to our own
// handler. Every ambiguous click in Rhino - idle selection and selection inside
// commands - goes through that one exported function.
namespace ChooseOneHook
{
  // Returns true when the hook is active. On failure, Rhino is left untouched
  // and the reason is printed to the command line.
  bool Install();
  void Uninstall();
  bool IsInstalled();
}
