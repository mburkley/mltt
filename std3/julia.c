JULIA.C
/******************************Module*Header*******************************\
* Module Name: julia.c * * Main module for the Mandelbrot Dream *
contains almost everything; windows procedure + misc stuff * * Created:
24-Oct-1991 18:34:08 * * Copyright 1993 - 1998 Microsoft Corporation * *
The Mandelbrot Dream serves to demonstrate the GDI and USER *
functionalities in the setting of fractals. * * The Mandelbrot Dream
provides the following functions: *       1.  Drawing the Mandelbrot set
and the corres ponding julia set *       2.  Zooming into any of the set
*       3.  MDI fractal drawing windows *       4.  Floating Point
Math/Fix Point Math *       5.  Shifting color table entries *       6.
Changing palette entries and animating palatte aka color cycling *
7.  Loading/Saving bitmap created with special effect *       8.
Changing bitmap color with flood fill *       9.  Boundary tracing and
creating a clip region out of it for *           creating special effect
*      10.  Enume rate printers for printing *      11.  Load RLE (or
convert .bmp files to RLE) for playing in viewer *      12.  Save the
RLE in memory to disk. * * Note: Users can now draw and saves the julia
sets on disk as bmps in *       the Julia windows.  These bitmaps can
then be read into the memory *       (and converted to RLE format) one
by one for displaying in sequence *       in the viewer window.  Eg Load
the julia.rle in the viewer window *       and select the play or play
continuously men u item.     02-Jan-1993 * * Not e2: The fix point math
in this sample makes use of the LargeInteger *        64 bit math
library. * * Dependencies: * *       none *
\**************************************************************************/
#include <windows.h> #include <stdlib.h> #include <commdlg.h> #include
<stdarg.h> #include <math.h> #include <stdio.h> #include <shellapi.h>
#include "julia.h"  // // For T1 to create all pens in advance.  This is
not a good approach // because pens are per thread basis.  The drawing
threads won't be able // to use them if they are not created in their
threads.    18-Sep-1992 // //#define THRDONE  #define CYCLETHRD  #define
PRTTHRD #define NEWPRTAPI  #ifndef DEBUG    #undef OutputDebugString
#define OutputDebugString(LPCSTR) #endif  // // Forward declarations. //
BOOL InitializeApp            (INT*); LONG APIENTRY MainWndProc
(HWND, UINT, DWORD, LONG); LONG APIENTRY ChildWndProc    (HWND, UINT,
DWORD, LONG); BOOL CALLBACK About           (HWND, UINT, DWORD, LONG);
LONG APIENTRY Te xtWndProc     (H WND, UINT, DWORD, LONG); LONG APIENTRY
JuliaWndProc    (HWND, UINT, DWORD, LONG); LONG APIENTRY ViewerWndProc
(HWND, UINT, DWORD, LONG); LONG APIENTRY ViewSurfWndProc (HWND, UINT,
DWORD, LONG); BOOL APIENTRY SuspendDrawThrd (HWND, LONG); BOOL APIENTRY
ResumeDrawThrd  (HWND, LONG); BOOL StartDraw       (PINFO); BOOL
StartDrawFix    (PINFO); BOOL StartDraw2      (PINFO); BOOL
StartMandelbrot (PINFO); BOOL StartMandelbrotFix (PINFO); HBITMAP
SaveBitmap   (HWND, HPALETTE); voi d DrawBitmap      (HDC, PINFO, in t,
int, int, int); BOOL bDrawDIB        (HDC, PINFO, int, int, int, int);
LONG lMul(LONG, LONG); LONG lDiv(LONG, LONG); PINFO pGetInfoData(HWND);
BOOL bReleaseInfoData(HWND); BOOL bCheckMutexMenuItem(PINFO, HMENU,
UINT); VOID vChkMenuItem(PINFO, HMENU, UINT); BOOL bInitInfo(PINFO);
BOOL bResetGlobal(VOID); HBRUSH hBrCreateBrush(HDC, DWORD); BOOL
bPrintBmp(PPRTDATA); BOOL bStoreRleFile(HDC, PINFO, PSTR); BOOL
bFreeRleFile(PINFO); BOOL bPlayR le(PINFO); BOOL bSaveRleFile(HDC,
PINFO, PSTR); BOOL bPlayRleCont2(P INFO); BOOL bSelectDIBPal(HDC, PINFO,
LPBITMAPINFO, BOOL); HBITMAP DIBfromDDB(HDC, HBITMAP, PINFO);   // //
Global variable declarations. //  HPEN   hpnRed; HPEN   hpnBlack; HPEN
hpnGreen; INT    giPen = 0;  HANDLE ghModule; HWND   ghwndMain = NULL;
HWND   ghwndClient = NULL; HANDLE ghAccel;  HMENU  hMenu, hChildMenu,
hViewMenu; HMENU  hViewSubOne, hSubMenuOne, hSubMenuThree; HMENU
hPrinterMenu;  CHAR   gszFile[20]; CHAR   gszMapName[20]; char
gtext[256];  BOOL   gFloa t = TRUE; LONG   gStep = 1; LONG gIteration =
500; BOOL   gbStretch = TRUE; INT    giStretchMode = COLORONCOLOR; INT
giDmOrient = DMORIENT_PORTRAIT; INT    giNPrinters = 0;  HPALETTE
ghPal, ghPalOld;  double xFrom, xTo, yFrom, yTo, c1, c2; LONG   lxFrom,
lxTo, lyFrom, lyTo, lc1, lc2;
/******************************Public*Routine******************************\
* * WinMain *
\**************************************************************************/
 int WINAPI WinMain (     HINSTANCE hInstance,     HINSTANCE
hPrevInstance, LPSTR lpCmdLine,     int nShowCmd) {     MSG    msg;
ghModule = GetModuleHandle(NULL);     if (!InitializeApp(&giPen))  {
OutputDebugString("memory: InitializeApp failure!"); return 0;     }
if (!(ghAccel = LoadAccelerators (ghModule, MAKEINTRESOURCE(ACCEL_ID))))
OutputDebugString("memory: Load Accel failure!");       while
(GetMessage(&msg, NULL, 0, 0))  { if (!TranslateAccelerator( ghwndMain,
ghAccel, &msg) &&     !TranslateMDISysAccel( ghwndClient, &msg))  {
TranslateMessage(&ms g); DispatchMe ssage(&msg); }     }
DeleteObject(ghPal);      return 1;
UNREFERENCED_PARAMETER(lpCmdLine);     UNREFERENCED_PARAMETER(nShowCmd);
UNREFERENCED_PARAMETER(hInstance);
UNREFERENCED_PARAMETER(hPrevInstance); }
/***************************************************************************\
* InitializeApp *
\***************************************************************************/
 BOOL InitializeApp(INT *piPen) {     WNDCLASS wc;     HDC      hDC;
#ifdef THRDONE     INT      iNumClr; #end if      wc.style            =
CS_OWNDC;     wc.lpfnWndProc      = (WNDPROC)MainWndProc;
wc.cbClsExtra       = 0;     wc.cbWndExtra       = sizeof(LONG);
wc.hInstance        = ghModule;     wc.hIcon            =
LoadIcon(ghModule, MAKEINTRESOURCE(APPICON));     wc.hCursor          =
LoadCursor(NULL, IDC_ARROW);     wc.hbrBackground    =
(HBRUSH)(COLOR_APPWORKSPACE);     wc.lpszMenuName     = "MainMenu";
wc.lpszClassName    = "MandelClass";      if (!Regist erClass(&wc))
return FALSE;      wc.lpfnW ndProc      = (WNDPROC)ChildWndProc;
wc.hIcon            = LoadIcon(ghModule, MAKEINTRESOURCE(APPICON));
wc.lpszMenuName     = NULL;     wc.lpszClassName    = "ChildClass";
if (!RegisterClass(&wc)) return FALSE;      wc.lpfnWndProc      =
(WNDPROC)ViewerWndProc;     wc.hIcon            = LoadIcon(ghModule,
MAKEINTRESOURCE(VIEWICON));     wc.lpszMenuName     = NULL;
wc.lpszClassName    = "ViewerClass";      if (!RegisterClass(&wc))
return FALSE;      wc.style = CS_OWNDC | CS_HREDR AW | CS_VREDRAW;
wc.lpfnWndProc      = (WNDPROC)TextWndProc;     wc.hIcon            =
NULL;     wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
wc.hbrBackground    = (HBRUSH)(COLOR_BTNSHADOW);     wc.lpszMenuName
= NULL;     wc.lpszClassName    = "Text";      if (!RegisterClass(&wc))
return FALSE;      wc.style            = CS_OWNDC | CS_HREDRAW |
CS_VREDRAW;     wc.lpfnWndProc      = (WNDPROC)JuliaWndProc;
wc.hIcon            = NULL; //     // Nope.  Can't have this, screw up m
y Paint Can cursor     //     //wc.hCursor          = LoadCursor(NULL,
IDC_ARROW);      wc.hCursor          = NULL;     wc.hbrBackground    =
(HBRUSH)(COLOR_BACKGROUND);     wc.lpszMenuName     = NULL;
wc.lpszClassName    = "Julia";      if (!RegisterClass(&wc))     return
FALSE;      wc.style            = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
wc.lpfnWndProc      = (WNDPROC)ViewSurfWndProc;     wc.hIcon
= NULL;     wc.h Cursor          = LoadCursor(NULL, IDC_ARROW);
wc.hbrBackground    = (HBRUSH)(COLOR_BACKGROUND);     wc.lpszMenuName
= NULL;     wc.lpszClassName    = "View";      if (!RegisterClass(&wc))
return FALSE;       //     // Notice, submenu is zero-based     //
hMenu       = LoadMenu(ghModule, "MainMenu");     hChildMenu  =
LoadMenu(ghModule, "ChildMenu");     hViewMenu   = LoadMenu(ghModule,
"ViewMenu");     hViewSubOne = GetSubMenu(hViewMenu, 1);     hSubMenuOne
= GetSubMenu(hMenu, 1);     hSubMenuThree = GetSubMenu(hChildMenu, 8);
hPrinterM enu = GetSubMenu(hChi ldMenu, 7);      //     // Disable
color-cycling for display devices that does not support     // palette
like the VGA.  As as 29-May-1992, the MIPS display driver     // is the
only one that supports palette     //     hDC = GetDC(NULL);     if
(!((GetDeviceCaps(hDC, RASTERCAPS)) & RC_PALETTE)) {
EnableMenuItem(hChildMenu, MM_CYCLE, MF_GRAYED);     }  #ifdef THRDONE
if ((iNumClr = iCreatePenFrPal(hDC, NULL, 0, &ghPal)) != 0)  { sprintf(
gtext,"iNumClr = %d\n", iNumClr); OutputDebugString( gtext);  if
((pInfo->prghPen = (PVOID*) GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT,
sizeof(HPEN)*iNumClr)) == NULL)      OutputDebugString ("Failed in
Memory Allocation for pInfo->prghPen!"); else  if ((*piPen =
iCreatePenFrPal(hDC, pInfo->prghPen, 0, &ghPal)) == 0)
OutputDebugString("Failed in creating pen!");     }  #endif
ReleaseDC(NULL, hDC);      ghwndMain = CreateWindowEx(0L, "MandelClass",
GetStringRes (IDS_MANDEL_DREAM),      WS_OVERLAPPED   | WS_CAPTION     |
WS_BORDER       |     WS_THICKFRAME   | WS_MAXI MIZEBOX | WS_MINIMIZEBOX
|     WS_CLIPCHILDREN | WS_VISIBLE     | WS_SYSMENU,     80, 70, 550,
400 /* 550 */,     NULL, hMenu, ghModule, NULL);      if (ghwndMain ==
NULL) return FALSE;      bInitPrinter(ghwndMain);
SetWindowLong(ghwndMain, GWL_USERDATA, 0L);      SetFocus(ghwndMain);
/* set initial focus */      PostMessage(ghwndMain, WM_COMMAND,
MM_MANDEL, 0L);     PostMessage(ghwndMain, WM_COMMAND,
MM_CREATE_MANDEL_THREAD, 0L);      return TRUE; }
/******************************Public*Routine*
*****************************\ * * MainWndProc *
\**************************************************************************/
 long APIENTRY MainWndProc(     HWND hwnd,     UINT message,     DWORD
wParam,     LONG lParam) {     static int         iJuliaCount=1;
static int         iMandelCount=1;     static int
iViewerCount=1;     CLIENTCREATESTRUCT clientcreate;     HWND
hwndChildWindow;     static FARPROC     lpfnSuspendThrd, lpfnResum
eThrd;       switch (message) {        case WM _CREATE:
SetWindowLong(hwnd, 0, (LONG)NULL);  clientcreate.hWindowMenu  =
hSubMenuOne; clientcreate.idFirstChild = 1;  ghwndClient =
CreateWindow("MDICLIENT", NULL,     WS_CHILD | WS_CLIPCHILDREN |
WS_VISIBLE,     0,0,0,0,     hwnd, NULL, ghModule,
(LPVOID)&clientcreate); lpfnSuspendThrd = (FARPROC)MakeProcInstance
(SuspendDrawThrd, ghModule); lpfnResumeThrd  = (FARPROC)MakeProcInstance
(ResumeDrawThrd, ghModule);  return 0L;        case WM_DESTROY: {
bCleanupPr inter(); PostQuitMessage(0); return 0L; }        //       //
Wait! User is going to zero out our app's visible region.  This       //
is going to mess up our drawing (we are not keeping any shadow       //
bitmap in this version yet.) So, let's suspend our drawing thread
// first before user does that.  We will resume after user is done.
//       case WM_SYSCOMMAND: { LONG        lResult;  // // We'll
enumerate our children and suspend their drawing thread. //
EnumChildWindows(ghwndCli ent, (WNDENUMPROC)lpfnSuspendThrd, lParam);
// // Now, let user does it supposed to do // lResult =
DefFrameProc(hwnd,  ghwndClient, message, wParam, lParam);  // // User's
done, we'll resume the suspended threads in our children //
EnumChildWindows(ghwndClient, (WNDENUMPROC)lpfnResumeThrd, lParam);
return lResult; break;       } #if 0       //       // Our window's size
is going to change, we'll make sure the new       // window is a square.
//       case WM_WINDOWPOSCHANGING: {     PWINDOWPOS pWndPos;     RECT
rect;     LONG       lcx, lcy; GetWindowRect(hwnd, &rect);     lcx =
rect.right-rect.left;     lcy = rect.bottom-rect.top;     pWndPos =
(PWINDOWPOS)lParam;     if ((pWndPos->cy > lcy) || (pWndPos->cx > lcx))
pWndPos->cx =  pWndPos->cy =    ((pWndPos->cx > pWndPos->cy) ?
pWndPos->cy : pWndPos->cx);     else if ((pWndPos->cy < lcy) ||
(pWndPos->cx < lcx))      pWndPos->cx =  pWndPos->cy = ((pWndPos->cx >
pWndPos->cy) ? pWndPos->cy : pWndPos->cx);     break;       } #endif
case WM_COMMAND:  switch (LOWORD(wParam)) {     case I DM_TILE:
SendMessage(ghwndClient, WM_MDITILE, 0L, 0L); return 0L;     case
IDM_CASCADE: SendMessage(ghwndClient, WM_MDICASCADE, 0L, 0L); return 0L;
case IDM_ARRANGE: SendMessage(ghwndClient, WM_MDIICONARRANGE, 0L, 0L);
return 0L;      //     // Create Julia or Mandelbrot set     //     case
MM_JULIA:     case MM_MANDEL: { HANDLE hInfo; PINFO  pInfo;
MDICREATESTRUCT mdicreate;  hInfo = LocalAlloc(LHND, (WORD)
sizeof(INFO)); if (hInfo == NULL)  {     O utputDebugString("Failed to
Allocate Info!");     retu rn 0L; } if ((pInfo =
(PINFO)LocalLock(hInfo)) == NULL)  {     OutputDebugString("Failed in
LocalLock, hInfo");     return 0L; }  bInitInfo(pInfo); wsprintf((LPSTR)
&(pInfo->CaptionBarText),  (LOWORD(wParam) == MM_JULIA) ?   GetStringRes
(IDS_JULIA):   GetStringRes (IDS_MANDELBROT),  (LOWORD(wParam) ==
MM_JULIA) ? iJuliaCount : iMandelCount); if (LOWORD(wParam) == MM_JULIA)
{     c1 = 0.360284;     c2 = 0.100376;     lc1 = 738;
//.3603515     lc2 = 206;          //.1005859 pInfo->bMandel = FALS E; }
else {     pInfo->bMandel = TRUE; }  // // Fill in the MDICREATE
structure for MDI child creation // mdicreate.szClass = "ChildClass";
mdicreate.szTitle = (LPTSTR)&(pInfo->CaptionBarText); mdicreate.hOwner
= ghModule; mdicreate.x       = mdicreate.y       = CW_USEDEFAULT;
mdicreate.cx      = 300; mdicreate.cy      = 300; mdicreate.style   =
0L; mdicreate.lParam  = (LONG) hInfo;  /*Create Child Window*/
hwndChildWindow =     (HANDLE) Se ndMessage(ghwndClient, WM_MDICREATE,
0L, (LONG)(LPMDICREATESTRUCT)& mdicreate);  if (hwndChildWindow == NULL)
{     OutputDebugString ("Failed in Creating Child Window");     return
0L; }  (LOWORD(wParam) == MM_JULIA) ? iJuliaCount++ : iMandelCount++ ;
LocalUnlock(hInfo); return ((LONG)hwndChildWindow);     }      case
MM_RLEVIEWER: { HANDLE hInfo; PINFO  pInfo; MDICREATESTRUCT mdicreate;
hInfo = LocalAlloc(LHND, (WORD) sizeof(INFO)); if (hInfo == NULL)  {
OutputDebugString ("Failed to Allocate Info!");     return 0L; } if
((pInfo = (PINF O)LocalLock(hInfo)) == NULL) {     OutputDebugString
("Failed in LocalLock, hInfo");     return 0L; }  bInitInfo(pInfo);
wsprintf((LPSTR) &(pInfo->CaptionBarText), GetStringRes (IDS_VIEWER),
iViewerCount );  // // Fill in the MDICREATE structure for MDI child
creation // mdicreate.szClass = "ViewerClass"; mdicreate.szTitle =
(LPTSTR)&(pInfo->CaptionBarText); mdicreate.hOwner  = ghModule;
mdicreate.x       = mdicreate.y       = CW_USEDEFAULT; mdicreate.cx
= 300; mdicreate.cy      = 300; m dicreate.style   = 0L;
mdicreate.lParam  = ( LONG) hInfo;  /*Create Child Window*/
hwndChildWindow =     (HANDLE) SendMessage(ghwndClient, WM_MDICREATE,
0L, (LONG)(LPMDICREATESTRUCT)&mdicreate);  if (hwndChildWindow == NULL)
{     OutputDebugString ("Failed in Creating Child Window");     return
0L; }  iViewerCount++ ; LocalUnlock(hInfo); return
((LONG)hwndChildWindow);      }      case MM_ABOUT: if
(DialogBox(ghModule, "AboutBox", ghwndMain, (DLGPROC)About) == -1)
OutputDebugString ("DEMO: About Dialog Creation Error!"); retu rn 0L;
//     // Only my children know how to deal with these messages, so
// pass these to children for processing     //     case
MM_CREATE_JULIA_THREAD:     case MM_SET_XFORM_ATTR:     case
MM_CREATE_MANDEL_THREAD:     case MM_OPT_4:              // currently
not used     case MM_DRAW_SET:     case MM_SETDIB2DEVICE:     case
MM_BW:     case MM_SHIFT:     case MM_CUSTOM:     case MM_CYCLE:
case MM_TP_IDLE:     case MM_TP_LOW:     case MM_TP_BELOW_NORMAL:
case MM_TP_NORMAL:     case MM_TP_ABOVE_NORMAL :     cas e MM_TP_HIGH:
case MM_TP_TIME_CRITICAL:     case MM_FLOAT:     case MM_FIX:     case
MM_ITERATION_100:     case MM_ITERATION_500:     case MM_ITERATION_1000:
case MM_ITERATION_5000:     case MM_ITERATION_DOUBLE:     case
MM_STEP_ONE:     case MM_STEP_TWO:     case MM_STEP_THREE:     case
MM_SAVE:     case MM_SAVE_MONO:     case MM_LOAD:     case
MM_STRETCHBLT:     case MM_BITBLT:     case MM_BLACKONWHITE:     case
MM_COLORONCOLOR:     case MM_W HITEONBLACK:     case MM_HALFTONE:
case MM_CLIP: case MM_RM_CLIP:     case MM_SELCLIPRGN:     case
MM_ERASE:     case MM_PORTRAIT:     case MM_LANDSCAPE:     case
MM_PRINTER:     case MM_PRINTER + 1:     case MM_PRINTER + 2:     case
MM_PRINTER + 3:     case MM_PRINTER + 4:     case MM_PRINTER + 5:
case MM_PRINTER + 6:     case MM_PRINTER + 7:     case MM_PRINTER + 8:
case MM_PRINTER + 9:     case MM_RLELOAD_DEMO:     case MM_RLEPLAYCONT:
case MM_RLELOAD:     case MM_RLESAVE:     case MM_CLEAR:     case
MM_RLEPLAY:     { HWND hAc tiveChild;  h ActiveChild = (HANDLE)
SendMessage(ghwndClient, WM_MDIGETACTIVE, 0L, 0L); if (hActiveChild)
SendMessage(hActiveChild, WM_COMMAND, wParam, lParam); return 0L;     }
default: return DefFrameProc(hwnd,  ghwndClient, message, wParam,
lParam); }     default:  return DefFrameProc(hwnd,  ghwndClient,
message, wParam, lParam);     } }
/***************************************************************************\
* ChildWndProc * \*************************************
**************************************/ long APIENTRY ChildWndProc(
HWND hwnd,     UINT message,     DWORD wParam,     LONG lParam) {
static FARPROC     lpfnSuspendThrd, lpfnResumeThrd;     static BOOL
bDIB2Device = FALSE;      sprintf( gtext,"message = %lx\n", message);
OutputDebugString( gtext);      switch (message) { case WM_COMMAND: {
PINFO       pInfo;   HWND        hTextWnd;    switch (LOWORD(wParam)) {
//     // Create a Julia drawing thread     //     case
MM_CREATE_JULIA_T HREAD: { if ((pInfo = pGetInfoData(hwnd )) == NULL) {
return 0L; }  hTextWnd = pInfo->hTextWnd; sprintf( gtext,"(%g, %g) <->
(%g, %g)", pInfo->xFrom, pInfo->yFrom, pInfo->xTo, pInfo->yTo);
SetWindowText(hTextWnd, gtext); sprintf( gtext,"(c1 = %g, c2 = %g)\n\n",
pInfo->c1, pInfo->c2); OutputDebugString( gtext );  if (pInfo->hThrd)
CloseHandle(pInfo->hThrd);  pInfo->hThrd = CreateThread(NULL, 0,
(gFloat ? (LPTHREAD_START_ROUTINE)StartDraw :
(LPTHREAD_START_ROUTINE)StartDrawFix),  pIn fo,
STANDARD_RIGHTS_REQUIRED,  &pInfo->dwThreadId ); if (pInfo->hThrd &&
pInfo->bDrawing)     {    if (!SetThreadPriority(pInfo->hThrd,
pInfo->iPriority))      OutputDebugString ("Can't set Priority!");
}         bReleaseInfoData(hwnd);        return 0L;     }      //     //
Reset pInfo reflecting new transformation     //     case
MM_SET_XFORM_ATTR: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L; }  hTextWnd = pInfo->hTextWnd; SetWindowText(hTextWnd,
GetStringRes (IDS_CLICK_HERE_VIEW)); pInfo->xFrom      = xFrom;
pInfo->xTo        = xTo; pInfo->yFrom      = yFrom; pInfo->yTo        =
yTo; pInfo->c1         = c1; pInfo->c2         = c2; pInfo->lxFrom
= lxFrom; pInfo->lxTo        = lxTo; pInfo->lyFrom      = lyFrom;
pInfo->lyTo        = lyTo; pInfo->lc1         = lc1; pInfo->lc2
= lc2;         bReleaseInfoData(hwnd);        return 0L;     }      //
// Create a Mandelbrot drawing thread     //     case
MM_CREATE_MANDEL_THREAD: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L; }  hTextWnd = p Info->hTextWnd; spr intf( gtext,"(%g, %g) <->
(%g, %g)", pInfo->xFrom, pInfo->yFrom, pInfo->xTo, pInfo->yTo);
SetWindowText(hTextWnd, gtext); sprintf( gtext,"(c1 = %g, c2 = %g)\n\n",
pInfo->c1, pInfo->c2); OutputDebugString( gtext );  if (pInfo->hThrd)
CloseHandle(pInfo->hThrd);  pInfo->hThrd = CreateThread(NULL, 0,
(gFloat ? (LPTHREAD_START_ROUTINE)StartMandelbrot :
(LPTHREAD_START_ROUTINE)StartMandelbrotFix),  pInfo,
STANDARD_RIGHTS_REQUIRED,  &pInfo->dwThreadId ); if (pInfo->hThrd &&
pInfo->bDrawing)     {   i f (!SetThreadPriority(pInfo->hThrd,
pInfo->iPriority))      OutputDebugString ("Can't set Priority!");
}          bReleaseInfoData(hwnd);        return 0L;     }      //
// Create a Julia drawing thread using algorithm StartDraw2     //
Currently not used     //     case MM_OPT_4: { if ((pInfo =
pGetInfoData(hwnd)) == NULL) {     return 0L; }  hTextWnd =
pInfo->hTextWnd; SetWindowText(hTextWnd, "MM_OPT_4");  sprintf(
gtext,"xFrom = %g, xTo = %g, yFrom = %g, yTo = %g\n", pInfo->xFrom,
pInfo->xTo, pInfo->yFrom, pInfo->yTo); OutputDebugString( gtext );  if
(pInfo->hThrd)     CloseHandle(pInfo->hThrd);  pInfo->hThrd =
CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE)StartDraw2,  pInfo,
STANDARD_RIGHTS_REQUIRED,  &pInfo->dwThreadId );         if
(pInfo->hThrd && pInfo->bDrawing)     {       if
(!SetThreadPriority(pInfo->hThrd, pInfo->iPriority))
OutputDebugString ("Can't set Priority!");        }
bReleaseInfoData(hwnd);        return 0L;     }      case MM_DRAW_SET: {
if ((pInfo = pGetInfoDa ta(hwnd)) == NULL) {     return 0L; }
PostMessage(hwnd, WM_COMMAND,     pInfo->bMandel ?
(DWORD)((WORD)MM_CREATE_MANDEL_THREAD) :
(DWORD)((WORD)MM_CREATE_JULIA_THREAD),     (LONG)0L);
bReleaseInfoData(hwnd);        return 0L;     }      {     int
iPriority;  case MM_TP_IDLE:     if ((pInfo = pGetInfoData(hwnd)) ==
NULL) { return 0L;     }      iPriority = THREAD_PRIORITY_IDLE;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_IDLE);
DrawMenuBar(GetParent(GetParent(hwnd ))) ;     goto CWP_SET_ PRIORITY;
case MM_TP_LOW:     if ((pInfo = pGetInfoData(hwnd)) == NULL) { return
0L;     }      iPriority = THREAD_PRIORITY_LOWEST;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_LOW);
DrawMenuBar(GetParent(GetParent(hwnd))) ;     goto CWP_SET_PRIORITY;
case MM_TP_BELOW_NORMAL:     if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L;     }      iPriority = THREAD_PRIORITY_BELOW_NORMAL;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_BELO W_NORMAL);
DrawMenuBar(GetParent(GetParent(hwnd))) ; goto CWP_SET_PRIORITY;  case
MM_TP_NORMAL:     if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;
}      iPriority = THREAD_PRIORITY_NORMAL;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_NORMAL);
DrawMenuBar(GetParent(GetParent(hwnd))) ;     goto CWP_SET_PRIORITY;
case MM_TP_ABOVE_NORMAL:     if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L;     }      iPriority = THREAD_PRIORITY_ABOVE_NORMAL;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_ABO VE_NORMAL);
DrawMenuBar(GetParent(Get Parent(hwnd))) ;     goto CWP_SET_PRIORITY;
case MM_TP_HIGH:     if ((pInfo = pGetInfoData(hwnd)) == NULL) { return
0L;     }      iPriority = THREAD_PRIORITY_HIGHEST;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_HIGH);
DrawMenuBar(GetParent(GetParent(hwnd))) ;     goto CWP_SET_PRIORITY;
case MM_TP_TIME_CRITICAL:     if ((pInfo = pGetInfoData(hwnd)) == NULL)
{ return 0L;     }      iPriority = THREAD_PRIORITY_TIME_CRITICAL;
bCheckMutexMenuItem(pInfo, hChildMenu, MM_TP_ TIME_CRITICAL);     DrawM
enuBar(GetParent(GetParent(hwnd))) ;  CWP_SET_PRIORITY:     {
HANDLE       hThrd;         hThrd = pInfo->hThrd;
pInfo->iPriority = iPriority;         if (hThrd && pInfo->bDrawing)
{       if (!SetThreadPriority(hThrd, iPriority))      OutputDebugString
("Can't set Priority!");        }      }     bReleaseInfoData(hwnd);
return 0L;      }      case MM_FLOAT: { if ((pInfo = pGetInfoData(hwnd))
== NULL) {     return 0L ; } bCheckMutexMenuItem(pInfo, hChildMenu,
MM_FLOAT); DrawMenuBar(GetPa rent(GetParent(hwnd))) ; gFloat = TRUE;
bReleaseInfoData(hwnd); return 0L;     }     case MM_FIX: { if ((pInfo =
pGetInfoData(hwnd)) == NULL) {     return 0L; }
bCheckMutexMenuItem(pInfo, hChildMenu, MM_FIX);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gFloat = FALSE;
bReleaseInfoData(hwnd); return 0L;     }     case MM_ITERATION_100: { if
((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }
bCheckMutexMenuItem(pInfo, hChildMenu, MM_ITERATION_100);
DrawMenuBar(GetParent(GetPar ent(hwnd))) ; gIteration = 1 00;
pInfo->iIteration = 100; SetWindowText(pInfo->hTextWnd, GetStringRes
(IDS_ITER_100)); bReleaseInfoData(hwnd); return 0L;     }     case
MM_ITERATION_500: { if ((pInfo = pGetInfoData(hwnd)) == NULL){
return 0L; } bCheckMutexMenuItem(pInfo, hChildMenu, MM_ITERATION_500);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gIteration = 500;
pInfo->iIteration = 500; SetWindowText(pInfo->hTextWnd, GetStringRes
(IDS_ITER_500)); bReleaseInfoData(hwnd); return 0L;     }     case
MM_ITERATION_1000: { if ((pInfo = pGe tInfoData(hwnd)) == NULL) {
return 0L; } bCheckMutexMenuItem(pInfo, hChildMenu, MM_ITERATION_1000);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gIteration = 1000;
pInfo->iIteration = 1000; SetWindowText(pInfo->hTextWnd, GetStringRes
(IDS_ITER_1000)); bReleaseInfoData(hwnd); return 0L;     }     case
MM_ITERATION_5000: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L; } bCheckMutexMenuItem(pInfo, hChildMenu, MM_ITERATION_5000);
DrawMenuBar(GetParent(GetParent (hwnd))) ; gIteration = 5000; pInfo->
iIteration = 5000; SetWindowText(pInfo->hTextWnd, GetStringRes
(IDS_ITER_5000)); bReleaseInfoData(hwnd); return 0L;     }     case
MM_ITERATION_DOUBLE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L; } bCheckMutexMenuItem(pInfo, hChildMenu,
MM_ITERATION_DOUBLE); DrawMenuBar(GetParent(GetParent(hwnd))) ;
gIteration *= 2; pInfo->iIteration = gIteration; sprintf( gtext,
GetStringRes (IDS_ITERATION), pInfo->iIteration); SetWindowT
ext(pInfo->hTextWnd, gtext); bReleaseInfoData(hwnd); return 0L;     }
case MM_STEP_ONE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {
return 0L; } bCheckMutexMenuItem(pInfo, hChildMenu, MM_STEP_ONE);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gStep = 1; pInfo->iStep = 1;
bReleaseInfoData(hwnd); return 0L;     }     case MM_STEP_TWO:  { if
((pInfo = pGetInfoData(hwnd)) == NULL){     return 0L; }
bCheckMutexMenuItem(pInfo, hChildMenu, MM_STEP_TWO);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gStep = 2; pInfo->iStep = 2;
bReleaseInfoData(hwnd); return 0L;     }     case MM_STEP_THREE: { if
((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }
bCheckMutexMenuItem(pInfo, hChildMenu, MM_STEP_THREE);
DrawMenuBar(GetParent(GetParent(hwnd))) ; gStep = 3; pInfo->iStep = 3;
bReleaseInfoData(hwnd); return 0L;     }      case MM_LOAD: { HDC hDC;
OPENFILENAME ofn; char szDirName[256]; char szFile[256],
szFileTitle[256]; static char *szFilter; RECT    rc;  if ((pInfo =
pGetInfoData(hwnd)) == NULL){     return 0L; }  szFilter = GetS tringRes
(IDS_FILE_LIST1);  GetSystemDirectory((L PSTR) szDirName, 256);
strcpy(szFile, "*.bmp\0");

ofn.lStructSize = sizeof(OPENFILENAME); ofn.hwndOwner = pInfo->hwnd; ofn.lpstrFilter = szFilter; ofn.lpstrCustomFilter = (LPSTR) NULL; ofn.nMaxCustFilter = 0L; ofn.nFilterIndex = 1; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFileTitle = szFileTitle; ofn.nMaxFileTitle = sizeof(szFileTitle); ofn.lpstrInitialDir = szDirName; ofn.lpstrTitle = (LPSTR) NULL; ofn.Flags = 0L; ofn.nFileOffset = 0; ofn.nFileExtension = 0; ofn.lpstrDefExt = "BMP";  if (!GetOpenFileName(&ofn))     return 0L;  GetCl
ientRect(pInfo->hwnd, &rc); hDC = GetDC(pInfo->hwnd); if (LoadBitmapFile(hDC, pInfo, szFile))   bDrawDIB(hDC, pInfo, 0, 0, rc.right, rc.bottom); ReleaseDC(hwnd, hDC);  bReleaseInfoData(hwnd);  return 0L;     }      case MM_SAVE: { HDC hDC; OPENFILENAME ofn; char szDirName[256]; char szFile[256], szFileTitle[256]; static char *szFilter;  szFilter = GetStringRes (IDS_FILE_LIST2);  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd); #if 0 { HPALETTE hPalTmp;  hPalTmp = CopyPa
lette(pInfo->hPal); DeleteObject(pInfo->hPal); pInfo->hPal = hPalTmp; } #endif // // saving special effects user might have created in window // if (pInfo->hBmpSaved)     DeleteObject(pInfo->hBmpSaved); pInfo->hBmpSaved = SaveBitmap(pInfo->hwnd, pInfo->hPal); pInfo->bUseDIB = FALSE;  GetSystemDirectory((LPSTR) szDirName, 256); strcpy(szFile, "*.bmp\0"); ofn.lStructSize = sizeof(OPENFILENAME); ofn.hwndOwner = pInfo->hwnd; ofn.lpstrFilter = szFilter; ofn.lpstrCustomFilter = (LPSTR) NULL; ofn.nMaxCustFilter = 
0L; ofn.nFilterIndex = 0L; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFileTitle = szFileTitle; ofn.nMaxFileTitle = sizeof(szFileTitle); ofn.lpstrInitialDir = szDirName; ofn.lpstrTitle = (LPSTR) NULL; ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT; ofn.nFileOffset = 0; ofn.nFileExtension = 0; ofn.lpstrDefExt = (LPSTR)NULL;  if (!GetSaveFileName(&ofn)) {     ReleaseDC(pInfo->hwnd, hDC);     bReleaseInfoData(hwnd);     return 0L; }  SelectPalette(hDC,    ((pInfo->iStretchMode == HALFTONE) 
? pInfo->hHTPal : pInfo->hPal),    FALSE); RealizePalette(hDC); UpdateColors(hDC);  // // test // ghPal = pInfo->hPal;  SaveBitmapFile(hDC, pInfo->hBmpSaved, szFile);  ReleaseDC(pInfo->hwnd, hDC);  bReleaseInfoData(hwnd); return 0L;     }     case MM_SAVE_MONO: { HDC hDC; OPENFILENAME ofn; char szDirName[256]; char szFile[256], szFileTitle[256]; static char *szFilter;  if ((pInfo = pGetInfoData(hwnd)) == NULL){     return 0L; }  szFilter = GetStringRes (IDS_FILE_LIST2);  GetSystemDirectory((LPSTR) szDirName
, 256); strcpy(szFile, "*.bmp\0"); ofn.lStructSize = sizeof(OPENFILENAME); ofn.hwndOwner = pInfo->hwnd; ofn.lpstrFilter = szFilter; ofn.lpstrCustomFilter = (LPSTR) NULL; ofn.nMaxCustFilter = 0L; ofn.nFilterIndex = 0L; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFileTitle = szFileTitle; ofn.nMaxFileTitle = sizeof(szFileTitle); ofn.lpstrInitialDir = szDirName; ofn.lpstrTitle = GetStringRes (IDS_SAVING_MONO_BITMAP); ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT; ofn.nFileOffset = 0; ofn.nF
ileExtension = 0; ofn.lpstrDefExt = (LPSTR)NULL;  if (!GetSaveFileName(&ofn))     return 0L;  hDC = GetDC(pInfo->hwnd);  SaveBitmapFile(hDC, pInfo->hBmpMono, szFile); ReleaseDC(pInfo->hwnd, hDC);  bReleaseInfoData(hwnd); return 0L;     }     case MM_STRETCHBLT: { if ((pInfo = pGetInfoData(hwnd)) == NULL){     return 0L; } gbStretch = TRUE; bCheckMutexMenuItem(pInfo, hChildMenu, MM_STRETCHBLT); DrawMenuBar(GetParent(GetParent(hwnd))) ; pInfo->bStretch = gbStretch; InvalidateRect(pInfo->hwnd, NULL, FALSE);  b
ReleaseInfoData(hwnd); return 0L;      }     case MM_BITBLT: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } gbStretch = FALSE; bCheckMutexMenuItem(pInfo, hChildMenu, MM_BITBLT); DrawMenuBar(GetParent(GetParent(hwnd))) ; pInfo->bStretch = gbStretch; InvalidateRect(pInfo->hwnd, NULL, FALSE);  bReleaseInfoData(hwnd); return 0L;      }     case MM_BLACKONWHITE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } giStretchMode = BLACKONWHITE; bCheckMutexMenuItem(pInfo, hChildMenu, MM_B
LACKONWHITE); DrawMenuBar(GetParent(GetParent(hwnd))) ; pInfo->iStretchMode = giStretchMode; InvalidateRect(pInfo->hwnd, NULL, FALSE);  bReleaseInfoData(hwnd); return 0L;      }     case MM_COLORONCOLOR: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } giStretchMode = COLORONCOLOR; bCheckMutexMenuItem(pInfo, hChildMenu, MM_COLORONCOLOR); DrawMenuBar(GetParent(GetParent(hwnd))) ; pInfo->iStretchMode = giStretchMode; InvalidateRect(pInfo->hwnd, NULL, FALSE);  bReleaseInfoData(hwnd); return 0L;  
    }     case MM_WHITEONBLACK: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } giStretchMode = WHITEONBLACK; bCheckMutexMenuItem(pInfo, hChildMenu, MM_WHITEONBLACK); DrawMenuBar(GetParent(GetParent(hwnd))) ; pInfo->iStretchMode = giStretchMode; InvalidateRect(pInfo->hwnd, NULL, FALSE);  bReleaseInfoData(hwnd); return 0L;      }     case MM_HALFTONE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } giStretchMode = HALFTONE; bCheckMutexMenuItem(pInfo, hChildMenu, MM_HALFTONE); Dr
awMenuBar(GetParent(GetParent(hwnd))) ; pInfo->iStretchMode = giStretchMode; InvalidateRect(pInfo->hwnd, NULL, FALSE);  bReleaseInfoData(hwnd); return 0L;     }     case MM_SETDIB2DEVICE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } bDIB2Device = (bDIB2Device ? FALSE : TRUE); pInfo->bSetDIBsToDevice = bDIB2Device; CheckMenuItem(hChildMenu, MM_SETDIB2DEVICE, (bDIB2Device ? MF_CHECKED : MF_UNCHECKED)); bReleaseInfoData(hwnd); return 0L;     }     case MM_BW: { HDC hDC;  if ((pInfo = pGetInfo
Data(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd); bChangeDIBColor(hDC, pInfo, MM_BW); ReleaseDC(hwnd, hDC); bReleaseInfoData(hwnd); return 0L;     }     case MM_SHIFT: { HDC hDC;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd); bChangeDIBColor(hDC, pInfo, MM_SHIFT); ReleaseDC(hwnd, hDC); bReleaseInfoData(hwnd); return 0L;     }     case MM_CUSTOM: { static DWORD argbCust[16] = {     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255
, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255),     RGB(255, 255, 255), RGB(255, 255, 255) }; CHOOSECOLOR cc; BOOL bResult; DWORD rgbOld; HBRUSH hBrush; HDC hDC;  rgbOld = RGB(255, 255, 255); cc.lStructSize = sizeof(CHOOSECOLOR); cc.hwndOwner = ghwndMain; cc.hInstance = ghModule; cc.rgbResult = rgbOld; cc.lpCustColors = argbC
ust; cc.Flags = CC_RGBINIT | CC_SHOWHELP; cc.lCustData = 0; cc.lpfnHook = NULL; cc.lpTemplateName = NULL;  bResult = ChooseColor(&cc); if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } if (bResult) {     hDC = GetDC(pInfo->hwnd);     hBrush = hBrCreateBrush(hDC, cc.rgbResult);     ReleaseDC(pInfo->hwnd, hDC);     if (pInfo->hBrush) DeleteObject(pInfo->hBrush);     pInfo->hBrush = hBrush;     pInfo->bFill = TRUE; } bReleaseInfoData(hwnd); return 0L; }  #ifndef CYCLETHRD     case MM_CYCLE: { HDC hD
C;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd);  if (pInfo->bClrCycle) {     CheckMenuItem(hChildMenu, MM_CYCLE, MF_UNCHECKED);     pInfo->bClrCycle = FALSE; } else {     CheckMenuItem(hChildMenu, MM_CYCLE, MF_CHECKED);     pInfo->bClrCycle = TRUE;     bChangeDIBColor(hDC, pInfo, MM_CYCLE); }  ReleaseDC(hwnd, hDC); bReleaseInfoData(hwnd); return 0L;     } #else      case MM_CYCLE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if (pInfo->bFirstTime
)  {     if (!SetEvent(pInfo->hQuitEvent))  { OutputDebugString ("Can't set Quit Event!"); return 0L;     }      if (pInfo->hCycleThrd) CloseHandle(pInfo->hCycleThrd);      pInfo->hCycleThrd = CreateThread(NULL, 0,      (LPTHREAD_START_ROUTINE)bCycle,      (LPVOID)hwnd,      STANDARD_RIGHTS_REQUIRED,      &pInfo->dwCycleThrdID );     pInfo->bClrCycle = TRUE;     pInfo->bFirstTime = FALSE;     CheckMenuItem(hChildMenu, MM_CYCLE, MF_CHECKED); } else {     if (pInfo->bClrCycle) { CheckMenuItem(hChildMenu, MM_C
YCLE, MF_UNCHECKED); pInfo->bClrCycle = FALSE; pInfo->dwSuspend = SuspendThread(pInfo->hCycleThrd);     } else { CheckMenuItem(hChildMenu, MM_CYCLE, MF_CHECKED); pInfo->bClrCycle = TRUE; pInfo->dwSuspend = ResumeThread(pInfo->hCycleThrd);     }     if (pInfo->dwSuspend == -1) { (pInfo->bClrCycle ?  sprintf( gtext,"Error in resuming thread\n") :  sprintf( gtext,"Error in suspending thread\n")  ); OutputDebugString( gtext );     } }  bReleaseInfoData(hwnd); return 0L;     } #endif     case MM_CLIP: { if ((pIn
fo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  hTextWnd = pInfo->hTextWnd; sprintf( gtext,"(%g, %g) <-> (%g, %g)", pInfo->xFrom, pInfo->yFrom, pInfo->xTo, pInfo->yTo); SetWindowText(hTextWnd, gtext); sprintf( gtext,"(c1 = %g, c2 = %g)\n\n", pInfo->c1, pInfo->c2); OutputDebugString( gtext ); if (!pInfo->bMandel)  {     MessageBox(ghwndMain,     GetStringRes (IDS_BOUNDARY),    NULL, MB_OK);     return 0L; }  if (pInfo->hThrd)     CloseHandle(pInfo->hThrd);  pInfo->hThrd = CreateThread(NULL, 0,  (gFloat
 ? (LPTHREAD_START_ROUTINE)bBoundaryScanFix : (LPTHREAD_START_ROUTINE)bBoundaryScanFix),  pInfo,  STANDARD_RIGHTS_REQUIRED,  &pInfo->dwThreadId );         bReleaseInfoData(hwnd);        return 0L;     }     case MM_RM_CLIP: { HDC     hDC;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  hDC = GetDC(pInfo->hwnd); SelectClipRgn(hDC, (HRGN) NULL); ReleaseDC(pInfo->hwnd, hDC); bReleaseInfoData(hwnd); InvalidateRect(pInfo->hwnd, NULL, FALSE); return 0L;      }     case MM_SELCLIPRGN: { HDC     hDC;
  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd);  if (pInfo->hRgnPath != (HRGN) NULL) {     SelectClipRgn(hDC, pInfo->hRgnPath); }  ReleaseDC(pInfo->hwnd, hDC); bReleaseInfoData(hwnd); return 0L;     }     case MM_ERASE: { HDC     hDC; RECT    rc;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; } hDC = GetDC(pInfo->hwnd); if (pInfo->hRgnPath != (HRGN) NULL) {     SelectClipRgn(hDC, pInfo->hRgnPath); } SelectObject(hDC, GetStockObject(WHITE_BRUSH)); GetClie
ntRect(pInfo->hwnd, &rc); PatBlt(hDC, 0, 0, rc.right, rc.bottom, PATCOPY); ReleaseDC(pInfo->hwnd, hDC); bReleaseInfoData(hwnd); return 0L;     }     case MM_PORTRAIT: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  giDmOrient = DMORIENT_PORTRAIT; bCheckMutexMenuItem(pInfo, hChildMenu, MM_PORTRAIT); DrawMenuBar(GetParent(GetParent(hwnd))) ; bReleaseInfoData(hwnd); return 0L;     }     case MM_LANDSCAPE: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  giDmOrient = DMORIENT_LAND
SCAPE; bCheckMutexMenuItem(pInfo, hChildMenu, MM_LANDSCAPE); DrawMenuBar(GetParent(GetParent(hwnd))) ; bReleaseInfoData(hwnd); return 0L;     }     case MM_PRINTER:     case MM_PRINTER + 1:     case MM_PRINTER + 2:     case MM_PRINTER + 3:     case MM_PRINTER + 4:     case MM_PRINTER + 5:     case MM_PRINTER + 6:     case MM_PRINTER + 7:     case MM_PRINTER + 8:  #ifdef PRTTHRD     case MM_PRINTER + 9: { PINFO       pInfo; PRTDATA     PrtData, *pPrtData; ULONG       sizINFO; PBYTE       pjTmpInfo, pjTmp;  i
f ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if (pInfo->hBmpSaved == NULL)  {     MessageBox(ghwndMain,         GetStringRes (IDS_NO_SAVED_BITMAP),    NULL, MB_OK);     return 0L; }  // // Copy the info structure to PrtData // pPrtData = &PrtData; pjTmp    = (PBYTE)&(pPrtData->info); pjTmpInfo = (PBYTE)pInfo; sizINFO = sizeof(INFO);  while(sizINFO--) {     *(((PBYTE)pjTmp)++) = *((pjTmpInfo)++); }  PrtData.index = LOWORD(wParam) - MM_PRINTER;  if (giDmOrient == DMORIENT_PORTRAIT) {     PrtDa
ta.bUseDefault = TRUE; } else {     PrtData.bUseDefault = FALSE;     PrtData.DevMode.dmSize = sizeof(DEVMODE);     PrtData.DevMode.dmDriverExtra = 0;     PrtData.DevMode.dmOrientation = DMORIENT_LANDSCAPE;     PrtData.DevMode.dmFields = DM_ORIENTATION; }  if (pInfo->hPrtThrd)     CloseHandle(pInfo->hPrtThrd);  pInfo->hPrtThrd = CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE)bPrintBmp,  &PrtData,  STANDARD_RIGHTS_REQUIRED,  &pInfo->dwPrtThrdID );  bReleaseInfoData(hwnd); return 0L;     }  #else     case MM_P
RINTER + 9: { HDC         hdcPrinter, hDC; int         index; DEVMODE     devmode; DEVMODE     *pdevmode; PINFO       pInfo; int         iWidth, iHeight;   index = LOWORD(wParam) - MM_PRINTER;  if (giDmOrient == DMORIENT_PORTRAIT)     pdevmode = NULL; else {     pdevmode = &devmode;     devmode.dmSize = sizeof(DEVMODE);     devmode.dmDriverExtra = 0;     devmode.dmOrientation = DMORIENT_LANDSCAPE;     devmode.dmFields = DM_ORIENTATION; }  if (!(hdcPrinter = CreateDC( "", gpszPrinterNames[index],      "", pd
evmode))) {     return(0L); }  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  iWidth = GetDeviceCaps(hdcPrinter, HORZRES); iHeight = GetDeviceCaps(hdcPrinter, VERTRES);  // !!! Why is it necessary to save the image over again?  May not want to //     do this because user may want to print the nice HT bitmap. So, //     use the DIB src. #if 0 if (pInfo->hBmpSaved)     DeleteObject(pInfo->hBmpSaved);  pInfo->hBmpSaved = SaveBitmap(pInfo->hwnd, pInfo->hPal); #endif Escape(hdcPrinter, STARTDOC, 2
0, "Mandelbrot", NULL); bDrawDIB(hdcPrinter, pInfo, 0, 0, iWidth, iHeight); Escape(hdcPrinter, NEWFRAME, 0, NULL, NULL); Escape(hdcPrinter, ENDDOC, 0, NULL, NULL); ReleaseDC(pInfo->hwnd, hDC); bReleaseInfoData(hwnd); DeleteDC(hdcPrinter); return 0L;     } #endif     default:        return 0L;    }  } case WM_SETFOCUS:     break;  case WM_MDIACTIVATE: {     PINFO       pInfo;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      if ((HWND) lParam == hwnd) {                // being activated 
SendMessage(GetParent(hwnd), WM_MDISETMENU,     (DWORD)  hChildMenu,     (LONG)   hSubMenuThree) ;  (pInfo->bClrCycle ?     CheckMenuItem(hChildMenu, MM_CYCLE, MF_CHECKED) :     CheckMenuItem(hChildMenu, MM_CYCLE, MF_UNCHECKED) );  vChkMenuItem(pInfo, hChildMenu, MF_CHECKED); DrawMenuBar(GetParent(GetParent(hwnd))) ; goto MDI_ACT_EXIT;     }      if ((HWND) wParam == hwnd) {                // being deactivated  vChkMenuItem(pInfo, hChildMenu, MF_UNCHECKED); DrawMenuBar(GetParent(GetParent(hwnd))) ;     } MD
I_ACT_EXIT:     bReleaseInfoData(hwnd);     return 0L; }  case WM_QUERYNEWPALETTE: case WM_PALETTECHANGED: {     PINFO       pInfo;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      SendMessage(pInfo->hwnd, message, wParam, lParam);     bReleaseInfoData(hwnd);     return 0L; }  #if 0 case WM_WINDOWPOSCHANGING: {     PWINDOWPOS  pWndPos;     PINFO       pInfo;     HWND        hTextWnd;     int         iCyText, iCxBorder, iCyBorder, iCyCaption;     RECT        rect, rcl;     LONG        l
cx, lcy;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { break;     }      hTextWnd = pInfo->hTextWnd;      bReleaseInfoData(hwnd);      iCyText = GetWindowLong(hTextWnd, GWL_USERDATA);     iCxBorder = GetSystemMetrics(SM_CXBORDER);     iCyBorder = GetSystemMetrics(SM_CYBORDER);     iCyCaption = GetSystemMetrics(SM_CYCAPTION) - iCyBorder;     GetClientRect(GetParent(hwnd), &rcl);     GetWindowRect(hwnd, &rect);     lcx = rect.right-rect.left;     lcy = rect.bottom-rect.top;     pWndPos = (PWINDOWPOS)lParam
;     if ((pWndPos->cy > lcy) || (pWndPos->cx > lcx)) { pWndPos->cx =    ((pWndPos->cx > pWndPos->cy) ? pWndPos->cy-iCyText : pWndPos->cx); pWndPos->cy = pWndPos->cx + iCyText;     } else { if ((pWndPos->cy < lcy) || (pWndPos->cx < lcx)) {      pWndPos->cx = ((pWndPos->cx > pWndPos->cy) ? pWndPos->cy-iCyText : pWndPos->cx);      pWndPos->cy = pWndPos->cx + iCyText;    }     }     break; } #endif case WM_SIZE: {     HANDLE      hThrd;     PINFO       pInfo;     HWND        hTextWnd, hJulia;     BOOL        b
Mandel;     WORD        wCx;     int         iCyText;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { break;     }      hTextWnd = pInfo->hTextWnd;     hJulia   = pInfo->hwnd;     hThrd    = pInfo->hThrd;     bMandel  = pInfo->bMandel;     bReleaseInfoData(hwnd);     iCyText = GetWindowLong(hTextWnd, GWL_USERDATA);     wCx = (WORD) (HIWORD(lParam) - iCyText);      MoveWindow(hJulia, 0, 0,    LOWORD(lParam),    wCx,    TRUE);      MoveWindow(hTextWnd,        0,        wCx,        LOWORD(lParam),        iCyT
ext,        TRUE);      if (hThrd) { TerminateThread(hThrd, (DWORD)0L); /* PostMessage(hwnd, WM_COMMAND,     bMandel ? (DWORD)((WORD)MM_CREATE_MANDEL_THREAD) : (DWORD)((WORD)MM_CREATE_JULIA_THREAD),     (LONG)0L);   */     }      break; }  // // display info in the status window // case WM_USER+0xa: {     PINFO       pInfo;     static ULONG ulClick = 0;     HWND        hTextWnd;      ulClick++;     if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      hTextWnd = pInfo->hTextWnd;     switch (ulCl
ick % 6) { case 0: sprintf( gtext,"%g <= x <= %g, %g <= y <= %g", pInfo->xFrom, pInfo->xTo, pInfo->yTo, pInfo->yFrom); break; case 1: sprintf( gtext,"c1 = %g, c2 = %g", pInfo->c1, pInfo->c2); break; case 2: sprintf( gtext,      GetStringRes (IDS_ELAPSED_TIME),  (ULONG) pInfo->dwElapsed); break; case 3: sprintf( gtext,      GetStringRes (IDS_ITERATION),  pInfo->iIteration); break; case 4:sprintf( gtext,  GetStringRes (IDS_STEP),  pInfo->iStep); break; case 5:  (gFloat ? sprintf( gtext, GetStringRes (IDS_FLOA
TING_PT))         : sprintf( gtext, GetStringRes (IDS_FIXED_PT))) ; break;  default: break;     }     SetWindowText(hTextWnd, gtext);     bReleaseInfoData(hwnd);     return 0L; }  case WM_SYSCOMMAND: {     LONG        lResult;      EnumChildWindows(ghwndClient, (WNDENUMPROC)lpfnSuspendThrd, lParam);      lResult = DefMDIChildProc(hwnd, message, wParam, lParam);      EnumChildWindows(ghwndClient, (WNDENUMPROC)lpfnResumeThrd, lParam);      return lResult;     break; }  case WM_CREATE: {     PINFO           pI
nfo;     HANDLE          hInfo;     HWND            hTextWnd, hJulia;     RECT            rcl;      //     // CR! MakeProcInstance is noop!     //     lpfnSuspendThrd = (FARPROC)MakeProcInstance (SuspendDrawThrd, ghModule);     lpfnResumeThrd  = (FARPROC)MakeProcInstance (ResumeDrawThrd, ghModule);      hTextWnd = CreateWindow("Text", NULL,     WS_BORDER | SS_LEFT | WS_CHILD | WS_VISIBLE,     0, 0, 0, 0,     hwnd,     (HMENU) 2,     ghModule,     NULL);      GetClientRect(hwnd, &rcl);     hJulia = CreateWin
dow("Julia", (LPSTR) NULL,   WS_CHILD      | WS_VISIBLE     |   WS_BORDER,   0,0, rcl.right-rcl.left,   rcl.bottom-rcl.top-GetWindowLong(hTextWnd, GWL_USERDATA),   hwnd, (HMENU)1, ghModule, (LPVOID)NULL);      SetWindowText(hTextWnd, GetStringRes (IDS_SELECT_DRAW_SET));      hInfo = (HANDLE) ((LPMDICREATESTRUCT) ((LPCREATESTRUCT) lParam)->lpCreateParams)->lParam ;     if (hInfo) { if ((pInfo = (PINFO)LocalLock(hInfo)) == NULL)  {     OutputDebugString ("Failed in LocalLock, hNode");     break; }  else  {   
  HDC hDC;      if (!GetClientRect(hwnd, &pInfo->rcClient)) OutputDebugString ("Failed in GetClientRect!");      pInfo->hTextWnd = hTextWnd;     pInfo->hwnd = hJulia;     hDC = GetDC(hJulia);     pInfo->hHTPal = CreateHalftonePalette(hDC);     ReleaseDC(hJulia, hDC); #ifdef CYCLETHRD     //     // Creating a signal quit color cycling event     //     if ((pInfo->hQuitEvent = CreateEvent(NULL, TRUE, TRUE, NULL)) == NULL) OutputDebugString ("Failed in creating Quit Event!"); #endif     SetWindowLong(hwnd, 0, 
(LONG) hInfo);     LocalUnlock(hInfo); }     }  else  { OutputDebugString ("Can't allocate hInfo!");     }  #if 0     //     // Initialize printers here will detect printers availiability     // more often, but kind of overkill.     //     bInitPrinter(hwnd); #endif     break; }  case WM_CLOSE: {     SendMessage(GetParent(hwnd), WM_MDISETMENU,     (DWORD) hMenu,     (LONG)  hSubMenuOne) ;     DrawMenuBar(GetParent(GetParent(hwnd))) ;     break; }  case WM_DESTROY: {     PINFO            pInfo;     if ((pInf
o = pGetInfoData(hwnd)) == NULL) { break;     }      if (pInfo->hThrd) { TerminateThread(pInfo->hThrd, (DWORD)0L); CloseHandle(pInfo->hThrd);     }     if (pInfo->hPrtThrd) { TerminateThread(pInfo->hPrtThrd, (DWORD)0L); CloseHandle(pInfo->hPrtThrd);     }     if (pInfo->hCycleThrd) { TerminateThread(pInfo->hCycleThrd, (DWORD)0L); CloseHandle(pInfo->hCycleThrd);     }   #ifdef CYCLETHRD     //     // Cleanup color cycling     //     if (!ResetEvent(pInfo->hQuitEvent)) OutputDebugString ("Failed in reseting q
uit event!"); #endif     if (pInfo->hBmpMono) DeleteObject(pInfo->hBmpMono);      if (pInfo->hBmpSaved) DeleteObject(pInfo->hBmpSaved);      if (pInfo->hBrush) DeleteObject(pInfo->hBrush);      bReleaseInfoData(hwnd);     LocalFree((HANDLE) GetWindowLong(hwnd, 0));     break; }  default:     return DefMDIChildProc(hwnd, message, wParam, lParam);      } //switch     return DefMDIChildProc(hwnd, message, wParam, lParam); }  /******************************Public*Routine******************************\ * * Viewe
rWndProc * * Effects: * * Warnings: * \**************************************************************************/  LONG APIENTRY ViewerWndProc(     HWND hwnd,     UINT message,     DWORD wParam,     LONG lParam) {     static FARPROC     lpfnSuspendThrd, lpfnResumeThrd;      switch (message) { case WM_COMMAND: {   PINFO       pInfo;   HWND        hTextWnd;    switch (LOWORD(wParam)) {     case MM_RLELOAD_DEMO: { HDC             hDC; char            szDirName[256]; char            szFile[400]; HWND          
  hViewSurf;  GetCurrentDirectory(256, (LPTSTR) szDirName);  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if ((hViewSurf = pInfo->hwnd) == NULL)     return 0L;  hTextWnd = pInfo->hTextWnd; sprintf( gtext,"Loading bitmap(s) into memory"); SetWindowText(hTextWnd, gtext);  hDC = GetDC(hViewSurf); strcpy(szFile, szDirName); strcat(szFile, "\\rsc\\julia.rle");  if (bStoreRleFile(hDC, pInfo, szFile)) {     pInfo->RleData.ulFiles++;     PostMessage(ghwndMain, WM_COMMAND, MM_RLEPLAYCONT, 0L); }  Re
leaseDC(hViewSurf, hDC);  bReleaseInfoData(hwnd); return 0L;      }       case MM_RLELOAD: { HDC             hDC; OPENFILENAME    ofn; char            szDirName[256]; char            szFile[256], szFileTitle[256]; static char     *szFilter; RECT            rc; HWND            hViewSurf;  szFilter = GetStringRes (IDS_FILE_LIST1);  GetSystemDirectory((LPSTR) szDirName, 256); strcpy(szFile, "*.bmp\0"); ofn.lStructSize = sizeof(OPENFILENAME); ofn.hwndOwner = hwnd; ofn.lpstrFilter = szFilter; ofn.lpstrCustomFilt
er = (LPSTR) NULL; ofn.nMaxCustFilter = 0L; ofn.nFilterIndex = 1; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFileTitle = szFileTitle; ofn.nMaxFileTitle = sizeof(szFileTitle); ofn.lpstrInitialDir = szDirName; ofn.lpstrTitle = (LPSTR) NULL; ofn.Flags = 0L; ofn.nFileOffset = 0; ofn.nFileExtension = 0; ofn.lpstrDefExt = "BMP";  if (!GetOpenFileName(&ofn))     return 0L;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if ((hViewSurf = pInfo->hwnd) == NULL)     return 0L;  hTex
tWnd = pInfo->hTextWnd; sprintf( gtext, GetStringRes (IDS_LOADING_BITMAPS)); SetWindowText(hTextWnd, gtext);  GetClientRect(hwnd, &rc); hDC = GetDC(hViewSurf); if (bStoreRleFile(hDC, pInfo, szFile))     pInfo->RleData.ulFiles++; ReleaseDC(hViewSurf, hDC);  bReleaseInfoData(hwnd); return 0L;      }      case MM_RLESAVE: { HDC             hDC; OPENFILENAME    ofn; char            szDirName[256]; char            szFile[256], szFileTitle[256]; static char     *szFilter; HWND            hViewSurf;  szFilter = Ge
tStringRes (IDS_FILE_LIST3);  GetSystemDirectory((LPSTR) szDirName, 256); strcpy(szFile, "*.rle\0"); ofn.lStructSize = sizeof(OPENFILENAME); ofn.hwndOwner = hwnd; ofn.lpstrFilter = szFilter; ofn.lpstrCustomFilter = (LPSTR) NULL; ofn.nMaxCustFilter = 0L; ofn.nFilterIndex = 0L; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFileTitle = szFileTitle; ofn.nMaxFileTitle = sizeof(szFileTitle); ofn.lpstrInitialDir = szDirName; ofn.lpstrTitle = GetStringRes(IDS_SAVING_MEMORY_RLE);  ofn.Flags = OFN_S
HOWHELP | OFN_OVERWRITEPROMPT; ofn.nFileOffset = 0; ofn.nFileExtension = 0; ofn.lpstrDefExt = (LPSTR)NULL;  if (!GetSaveFileName(&ofn))     return 0L;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if ((hViewSurf = pInfo->hwnd) == NULL)     return 0L;  hTextWnd = pInfo->hTextWnd; sprintf( gtext, GetStringRes (IDS_SAVING_LOADED_BMP));  SetWindowText(hTextWnd, gtext);  hDC = GetDC(hViewSurf); bSaveRleFile(hDC, pInfo, szFile); ReleaseDC(hViewSurf, hDC);  bReleaseInfoData(hwnd); return 0L;     }
      case MM_CLEAR: { if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  hTextWnd = pInfo->hTextWnd; sprintf( gtext,GetStringRes (IDS_DISCARD_LOADED_BMP)); SetWindowText(hTextWnd, gtext);  bFreeRleFile(pInfo);  bReleaseInfoData(hwnd); return 0L;     }      case MM_RLEPLAY: { HDC     hDC; HWND    hViewSurf;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if ((hViewSurf = pInfo->hwnd) == NULL)     return 0L;  hTextWnd = pInfo->hTextWnd; sprintf( gtext, GetStringRes (IDS_PLAY_LOADED_
BMP));  SetWindowText(hTextWnd, gtext);  hDC = GetDC(hViewSurf); EnableMenuItem(hViewSubOne, MM_CLEAR, MF_GRAYED);  if (pInfo->hThrd0)     CloseHandle(pInfo->hThrd0);  pInfo->hThrd0 = CreateThread(NULL, 0,      (LPTHREAD_START_ROUTINE)bPlayRle,      pInfo,      STANDARD_RIGHTS_REQUIRED,      &pInfo->dwThreadId );  if (pInfo->hThrd0)  {     if (!SetThreadPriority(pInfo->hThrd0, THREAD_PRIORITY_BELOW_NORMAL)) 

OutputDebugString ("Can't set Priority!"); }  //bPlayRle(hDC, pInfo); EnableMenuItem(hViewSubOne, MM_CLEAR, MF_ENABLED); ReleaseDC(hViewSurf, hDC);  bReleaseInfoData(hwnd); return 0L;     }      case MM_RLEPLAYCONT: { HWND    hViewSurf;  if ((pInfo = pGetInfoData(hwnd)) == NULL) {     return 0L; }  if ((hViewSurf = pInfo->hwnd) == NULL)     return 0L;  hTextWnd = pInfo->hTextWnd; sprintf( gtext, GetStringRes (IDS_PLAY_BMP_CONT)); SetWindowText(hTextWnd, gtext);  if (pInfo->bFirstTime)  {     if (!SetEvent(p
Info->hQuitEvent))  { OutputDebugString ("Can't set Quit Event!");     return 0L;     }      EnableMenuItem(hViewSubOne, MM_CLEAR, MF_GRAYED);      if (pInfo->hThrd) CloseHandle(pInfo->hThrd);      pInfo->hThrd = CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE)bPlayRleCont2,  pInfo,  STANDARD_RIGHTS_REQUIRED,  &pInfo->dwThreadId );      if (pInfo->hThrd) { if (!SetThreadPriority(pInfo->hThrd, THREAD_PRIORITY_BELOW_NORMAL))      OutputDebugString ("Can't set Priority!");     }      pInfo->bPlayRleCont = TRUE;
     pInfo->bFirstTime = FALSE;     CheckMenuItem(hViewMenu, MM_RLEPLAYCONT, MF_CHECKED); }  else  {     if (pInfo->bPlayRleCont) { EnableMenuItem(hViewSubOne, MM_CLEAR, MF_ENABLED); CheckMenuItem(hViewMenu, MM_RLEPLAYCONT, MF_UNCHECKED); pInfo->bPlayRleCont = FALSE; pInfo->dwSuspend = SuspendThread(pInfo->hThrd);     } else { EnableMenuItem(hViewSubOne, MM_CLEAR, MF_GRAYED); CheckMenuItem(hViewMenu, MM_RLEPLAYCONT, MF_CHECKED); pInfo->bPlayRleCont = TRUE; pInfo->dwSuspend = ResumeThread(pInfo->hThrd);     
}     if (pInfo->dwSuspend == -1) { (pInfo->bPlayRleCont ?  sprintf( gtext,"Error in resuming thread\n") :  sprintf( gtext,"Error in suspending thread\n")  ); OutputDebugString( gtext );     } }  bReleaseInfoData(hwnd); return 0L;     }      default:        return 0L;    } //switch  } // WM_COMMAND case WM_SETFOCUS:     break;  case WM_MDIACTIVATE: {     PINFO       pInfo;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      if ((HWND) lParam == hwnd) { SendMessage(GetParent(hwnd), WM_MDIS
ETMENU,     (WPARAM)  hViewMenu,     (LPARAM)  NULL) ; DrawMenuBar(GetParent(GetParent(hwnd))) ;     }      bReleaseInfoData(hwnd);     return 0L; } case WM_QUERYNEWPALETTE: case WM_PALETTECHANGED: {     PINFO       pInfo;      if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      SendMessage(pInfo->hwnd, message, wParam, lParam);     bReleaseInfoData(hwnd);     return 0L; }  case WM_SIZE: {     PINFO       pInfo;     HWND        hTextWnd, hView;     WORD        wCx;     int         iCyText;    
  if ((pInfo = pGetInfoData(hwnd)) == NULL) { break;     }      hTextWnd = pInfo->hTextWnd;     hView    = pInfo->hwnd;     bReleaseInfoData(hwnd);     iCyText = GetWindowLong(hTextWnd, GWL_USERDATA);     wCx = (WORD) (HIWORD(lParam) - iCyText);      MoveWindow(hView, 0, 0,    LOWORD(lParam),    wCx,    TRUE);      MoveWindow(hTextWnd,        0,        wCx,        LOWORD(lParam),        iCyText,        TRUE);      break; }  // // display info in the status window // case WM_USER+0xa: {     PINFO       pInfo
;     static ULONG ulClick = 0;     HWND        hTextWnd;      ulClick++;     if ((pInfo = pGetInfoData(hwnd)) == NULL) { return 0L;     }      hTextWnd = pInfo->hTextWnd;     switch (ulClick % 4) { case 0:  sprintf( gtext, GetStringRes (IDS_FRAMES));  break; case 1:  sprintf( gtext, GetStringRes (IDS_FRAMES)); break; case 2: sprintf( gtext, (pInfo->bPlayRleCont ?                 GetStringRes (IDS_CONT_PLAY) :                        GetStringRes (IDS_SINGLE_PLAY))); break; case 3: sprintf( gtext,""); break;
 default: break;     }     SetWindowText(hTextWnd, gtext);     bReleaseInfoData(hwnd);     return 0L; }  case WM_SYSCOMMAND: {     LONG        lResult;      EnumChildWindows(ghwndClient, (WNDENUMPROC)lpfnSuspendThrd, lParam);      lResult = DefMDIChildProc(hwnd, message, wParam, lParam);      EnumChildWindows(ghwndClient, (WNDENUMPROC)lpfnResumeThrd, lParam);      return lResult;     break; }  case WM_CREATE: {     PINFO           pInfo;     HANDLE          hInfo;     HWND            hTextWnd, hView;     RE
CT            rcl;      //     // CR! MakeProcInstance is noop!     //     lpfnSuspendThrd = (FARPROC)MakeProcInstance (SuspendDrawThrd, ghModule);     lpfnResumeThrd  = (FARPROC)MakeProcInstance (ResumeDrawThrd, ghModule);      hTextWnd = CreateWindow("Text", NULL,     WS_BORDER | SS_LEFT | WS_CHILD | WS_VISIBLE,     0, 0, 0, 0,     hwnd,     (HMENU) 2,     ghModule,     NULL);      GetClientRect(hwnd, &rcl);     hView = CreateWindow("View", (LPSTR) NULL,   WS_CHILD      | WS_VISIBLE     |   WS_BORDER,   0
,0, rcl.right-rcl.left,   rcl.bottom-rcl.top-GetWindowLong(hTextWnd, GWL_USERDATA),   hwnd, (HMENU)1, ghModule, (LPVOID)NULL);      SetWindowText(hTextWnd, GetStringRes (IDS_SELECT_DRAW_SET));     hInfo = (HANDLE) ((LPMDICREATESTRUCT) ((LPCREATESTRUCT) lParam)->lpCreateParams)->lParam ;     if (hInfo) { if ((pInfo = (PINFO)LocalLock(hInfo)) == NULL)  {     OutputDebugString ("Failed in LocalLock, hNode");     break; }  else  {     HDC hDC;      if (!GetClientRect(hwnd, &pInfo->rcClient)) OutputDebugString (
"Failed in GetClientRect!");      pInfo->hTextWnd = hTextWnd;     pInfo->hwnd = hView;      hDC = GetDC(hView);     pInfo->hHTPal = CreateHalftonePalette(hDC);     ReleaseDC(hView, hDC);      //     // Creating a signal quit play continuous event     //     if ((pInfo->hQuitEvent = CreateEvent(NULL, TRUE, TRUE, NULL)) == NULL) OutputDebugString ("Failed in creating Quit Event!");      SetWindowLong(hwnd, 0, (LONG) hInfo);     LocalUnlock(hInfo); }     }  else  { OutputDebugString ("Can't allocate hInfo!"); 
    }      break; }  case WM_CLOSE: {     SendMessage(GetParent(hwnd), WM_MDISETMENU,     (DWORD) hMenu,     (LONG)  hSubMenuOne) ;     DrawMenuBar(GetParent(GetParent(hwnd))) ;     break; }  case WM_DESTROY: {     PINFO            pInfo;     if ((pInfo = pGetInfoData(hwnd)) == NULL) { break;     }      TerminateThread(pInfo->hThrd, (DWORD)0L);     CloseHandle(pInfo->hThrd);      //     // Cleanup continuous play     //     if (!ResetEvent(pInfo->hQuitEvent)) OutputDebugString ("Failed in reseting quit even
t!");      bFreeRleFile(pInfo);     bReleaseInfoData(hwnd);     LocalFree((HANDLE) GetWindowLong(hwnd, 0));     break; }  default:     return DefMDIChildProc(hwnd, message, wParam, lParam);      } //switch     return DefMDIChildProc(hwnd, message, wParam, lParam); }  /******************************Public*Routine******************************\ * * ViewSurfWndProc * * Effects: * * Warnings: * \**************************************************************************/  LONG APIENTRY ViewSurfWndProc (HWND hwnd
, UINT message, DWORD wParam, LONG lParam) {     switch (message)     {        case WM_CREATE: {     break;        }         case WM_DESTROY: {     break;        }         case WM_QUERYNEWPALETTE: {      HWND        hParent;      PINFO       pInfo;      HDC         hDC;      UINT        i;      HPALETTE    hOldPal;       if ((hParent=GetParent(hwnd)) == NULL)   { OutputDebugString ("Can't get hParent!"); return 0L;      }      if ((pInfo = pGetInfoData(hParent)) == NULL) {   return 0L;      }       // If pa
lette realization causes a palette change,      // we need to do a full redraw.       hDC = GetDC (hwnd);       hOldPal = SelectPalette (hDC,        pInfo->RleData.hPal,        0);       i = RealizePalette(hDC);       SelectPalette (hDC, hOldPal, 0);      ReleaseDC (hwnd, hDC);      bReleaseInfoData(hParent);       if (i) {  InvalidateRect (hwnd, (LPRECT) (NULL), TRUE);  return TRUE;      } else  return FALSE;         }        case WM_PALETTECHANGED: {      HWND        hParent;      PINFO       pInfo;      
HDC         hDC;      UINT        i;      HPALETTE    hOldPal;       if ((hParent=GetParent(hwnd)) == NULL) {  MessageBox(ghwndMain, "Can't get hParent!", "Error", MB_OK);  return 0L;      }      if ((pInfo = pGetInfoData(hParent)) == NULL) {   return 0L;      }       // if we were not responsible for palette change and if      // palette realization causes a palette change, do a redraw.       if ((HWND)wParam != hwnd){ hDC = GetDC (hwnd);  hOldPal = SelectPalette (hDC, pInfo->RleData.hPal, 0);  i = Realize
Palette (hDC);  if (i){     UpdateColors (hDC); } SelectPalette (hDC, hOldPal, 0); ReleaseDC (hwnd, hDC);     }     bReleaseInfoData(hParent);     break;        }         case WM_PAINT:  {      PAINTSTRUCT ps;      HDC         hDC;      RECT        rc;       GetClientRect(hwnd,&rc);      hDC = BeginPaint(hwnd, &ps);      EndPaint(hwnd, &ps);      return 0L;  }      } // switch     return DefWindowProc(hwnd, message, wParam, lParam); }  /***********************************************************************
****\ * About * * About dialog proc. * \***************************************************************************/  BOOL CALLBACK About(     HWND hDlg,     UINT message,     DWORD wParam,     LONG lParam) {     switch (message) {     case WM_INITDIALOG: return TRUE;      case WM_COMMAND: if (wParam == IDOK)     EndDialog(hDlg, wParam); break;     }      return FALSE;      UNREFERENCED_PARAMETER(lParam);     UNREFERENCED_PARAMETER(hDlg); }  /*****************************************************************
******** * * TextWndProc * * Text Window proc. * \***************************************************************************/  LONG APIENTRY TextWndProc (HWND hwnd, UINT message, DWORD wParam, LONG lParam) {     static HFONT hFont = (HFONT) NULL;      switch (message)     {     case WM_CREATE: {     LOGFONT    lf;     HDC        hDC;     HFONT      hOldFont;     TEXTMETRIC tm;     RECT       rect;     LONG       lHeight;      SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(lf), (PVOID) &lf, FALSE);   
   hDC = GetDC(hwnd);     // this is the height for 8 point size font in pixels     lf.lfHeight = 8 * GetDeviceCaps(hDC, LOGPIXELSY) / 72;      hFont = CreateFontIndirect(&lf);     hOldFont = SelectObject(hDC, hFont);     GetTextMetrics(hDC, &tm);     GetClientRect(GetParent(hwnd), &rect);      // base the height of the window on size of text     lHeight = tm.tmHeight+6*GetSystemMetrics(SM_CYBORDER)+2;     // saved the height for later reference     SetWindowLong(hwnd, GWL_USERDATA, lHeight);     SetWindowP
os(hwnd, NULL,     0,     rect.bottom-lHeight,     rect.right-rect.left,     lHeight,     SWP_NOZORDER | SWP_NOMOVE);      ReleaseDC(hwnd, hDC);     break; }      case WM_LBUTTONDOWN: { PostMessage(GetParent(hwnd), WM_USER+0xa, (DWORD)0L, (LONG)0L); break;     }      case WM_DESTROY:     if (hFont) DeleteObject(hFont);     break;      case WM_SETTEXT:     DefWindowProc(hwnd, message, wParam, lParam);     InvalidateRect(hwnd,NULL,TRUE);     UpdateWindow(hwnd);     return 0L;      case WM_PAINT: {     PAINTST
RUCT ps;     RECT   rc;     char   ach[128];     int    len, nxBorder, nyBorder;     HFONT  hOldFont = NULL;      BeginPaint(hwnd, &ps);      GetClientRect(hwnd,&rc);      nxBorder = GetSystemMetrics(SM_CXBORDER);     rc.left  += 9*nxBorder;     rc.right -= 9*nxBorder;      nyBorder = GetSystemMetrics(SM_CYBORDER);     rc.top    += 3*nyBorder;     rc.bottom -= 3*nyBorder;      // 3D Text     len = GetWindowText(hwnd, ach, sizeof(ach));     SetBkColor(ps.hdc, GetSysColor(COLOR_BTNFACE));      SetBkMode(ps.hd
c, TRANSPARENT);     SetTextColor(ps.hdc, RGB(64,96,96));     if (hFont) hOldFont = SelectObject(ps.hdc, hFont);     ExtTextOut(ps.hdc, rc.left+2*nxBorder+2, rc.top+2, ETO_OPAQUE | ETO_CLIPPED, &rc, ach, len, NULL);      SetTextColor(ps.hdc, RGB(128,128,128));     if (hFont) hOldFont = SelectObject(ps.hdc, hFont);     ExtTextOut(ps.hdc, rc.left+2*nxBorder+1, rc.top+1, ETO_CLIPPED, &rc, ach, len, NULL);      SetTextColor(ps.hdc, RGB(255,255,255));     if (hFont) hOldFont = SelectObject(ps.hdc, hFont);     Ex
tTextOut(ps.hdc, rc.left+2*nxBorde
