#pragma once

// Must be first: sets Windows version macros and runs the Rhino SDK checks.
#include "rhinoSdkStdafxPreamble.h"

#define VC_EXTRALEAN
#include <afxwin.h>     // MFC core and standard components
#include <afxext.h>     // MFC extensions
#include <afxole.h>     // MFC OLE classes
#include <afxdisp.h>    // MFC Automation classes
#include <afxdtctl.h>   // MFC support for Internet Explorer common controls
#include <afxcmn.h>     // MFC support for Windows common controls
#include <afxcview.h>   // MFC list/tree views
#include <afxrich.h>    // MFC rich edit

#if defined(_M_X64) && defined(WIN32) && defined(WIN64)
// afxwin.h unconditionally defines WIN32; Rhino wants exactly WIN64.
#undef WIN32
#endif

#define RHINO_V6_READY
#define RHINO_SDK_MFC
#include "RhinoSdk.h"
#include "rhinoSdkPlugInLinkingPragmas.h"
