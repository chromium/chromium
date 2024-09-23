// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_MSG_UTIL_H_
#define UI_GFX_WIN_MSG_UTIL_H_

#include <ostream>

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

// Based on WTL version 8.0 atlcrack.h

// This differs from the original atlcrack.h by removing usage of CPoint,
// CSize, etc.

///////////////////////////////////////////////////////////////////////////////
// Message map macro for cracked handlers

// Note about message maps with cracked handlers:
// For ATL 3.0, a message map using cracked handlers MUST use
// CR_BEGIN_MSG_MAP_EX. For ATL 7.0 or higher, you can use CR_BEGIN_MSG_MAP for
// CWindowImpl/CDialogImpl derived classes, but must use CR_BEGIN_MSG_MAP_EX for
// classes that don't derive from CWindowImpl/CDialogImpl.
// Classes using the CR_BEGIN_MSG_MAP_EX/CR_END_MSG_MAP set of macros must
// also include a CR_MSG_MAP_CLASS_DECLARATIONS macro after all members in
// the class definition since the macros add a
// base::WeakPtrFactory which is only allowed if last in the class.

#define CR_BEGIN_MSG_MAP_EX(theClass)                                       \
 public:                                                                    \
  /* "handled" management for cracked handlers */                           \
  void SetMsgHandled(BOOL handled) { msg_handled_ = handled; }              \
  BOOL ProcessWindowMessage(HWND hwnd, UINT msg, WPARAM w_param,            \
                            LPARAM l_param, LRESULT& l_result,              \
                            DWORD msg_map_id = 0) override {                \
    auto ref(theClass::msg_handler_weak_factory_.GetWeakPtr());             \
    BOOL old_msg_handled = msg_handled_;                                    \
    BOOL ret = _ProcessWindowMessage(hwnd, msg, w_param, l_param, l_result, \
                                     msg_map_id);                           \
    if (ref.get())                                                          \
      msg_handled_ = old_msg_handled;                                       \
    return ret;                                                             \
  }                                                                         \
  BOOL _ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam,           \
                             LPARAM lParam, LRESULT& lResult,               \
                             DWORD dwMsgMapID) {                            \
    auto ref(theClass::msg_handler_weak_factory_.GetWeakPtr());             \
    BOOL bHandled = TRUE;                                                   \
    hWnd;                                                                   \
    uMsg;                                                                   \
    wParam;                                                                 \
    lParam;                                                                 \
    lResult;                                                                \
    bHandled;                                                               \
    switch (dwMsgMapID) {                                                   \
      case 0:

// Replacement for atlwin.h's END_MSG_MAP for removing ATL usage.
#define CR_END_MSG_MAP()                                                   \
  break;                                                                   \
  default:                                                                 \
    NOTREACHED_IN_MIGRATION() << "Invalid message map ID: " << dwMsgMapID; \
    break;                                                                 \
    }                                                                      \
    return FALSE;                                                          \
    }

// This macro must be last in the class since it contains a
// base::WeakPtrFactory which must be last in the class.
#define CR_MSG_MAP_CLASS_DECLARATIONS(theClass) \
 private:                                       \
  BOOL msg_handled_{false};                     \
  base::WeakPtrFactory<theClass> msg_handler_weak_factory_{this};

#define CR_GET_X_LPARAM(lParam) ((int)(short)LOWORD(lParam))
#define CR_GET_Y_LPARAM(lParam) ((int)(short)HIWORD(lParam))

///////////////////////////////////////////////////////////////////////////////
// Standard Windows message macros

// int OnCreate(LPCREATESTRUCT lpCreateStruct)
#define CR_MSG_WM_CREATE(func)                       \
  if (uMsg == WM_CREATE) {                           \
    SetMsgHandled(TRUE);                             \
    lResult = (LRESULT)func((LPCREATESTRUCT)lParam); \
    if (!ref.get() || msg_handled_)                  \
      return TRUE;                                   \
  }

// BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
#define CR_MSG_WM_INITDIALOG(func)                 \
  if (uMsg == WM_INITDIALOG) {                     \
    SetMsgHandled(TRUE);                           \
    lResult = (LRESULT)func((HWND)wParam, lParam); \
    if (!ref.get() || msg_handled_)                \
      return TRUE;                                 \
  }

// BOOL OnCopyData(CWindow wnd, PCOPYDATASTRUCT pCopyDataStruct)
#define CR_MSG_WM_COPYDATA(func)                                    \
  if (uMsg == WM_COPYDATA) {                                        \
    SetMsgHandled(TRUE);                                            \
    lResult = (LRESULT)func((HWND)wParam, (PCOPYDATASTRUCT)lParam); \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnDestroy()
#define CR_MSG_WM_DESTROY(func)     \
  if (uMsg == WM_DESTROY) {         \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnMove(CPoint ptPos)
#define CR_MSG_WM_MOVE(func)                                            \
  if (uMsg == WM_MOVE) {                                                \
    SetMsgHandled(TRUE);                                                \
    func(gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnSize(UINT nType, gfx::Size size)
#define CR_MSG_WM_SIZE(func)                                           \
  if (uMsg == WM_SIZE) {                                               \
    SetMsgHandled(TRUE);                                               \
    func((UINT)wParam,                                                 \
         gfx::Size(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                       \
    if (!ref.get() || msg_handled_)                                    \
      return TRUE;                                                     \
  }

// void OnActivate(UINT nState, BOOL bMinimized, CWindow wndOther)
#define CR_MSG_WM_ACTIVATE(func)                                    \
  if (uMsg == WM_ACTIVATE) {                                        \
    SetMsgHandled(TRUE);                                            \
    func((UINT)LOWORD(wParam), (BOOL)HIWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnSetFocus(CWindow wndOld)
#define CR_MSG_WM_SETFOCUS(func)    \
  if (uMsg == WM_SETFOCUS) {        \
    SetMsgHandled(TRUE);            \
    func((HWND)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnKillFocus(CWindow wndFocus)
#define CR_MSG_WM_KILLFOCUS(func)   \
  if (uMsg == WM_KILLFOCUS) {       \
    SetMsgHandled(TRUE);            \
    func((HWND)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnEnable(BOOL bEnable)
#define CR_MSG_WM_ENABLE(func)      \
  if (uMsg == WM_ENABLE) {          \
    SetMsgHandled(TRUE);            \
    func((BOOL)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnPaint(CDCHandle dc)
#define CR_MSG_WM_PAINT(func)       \
  if (uMsg == WM_PAINT) {           \
    SetMsgHandled(TRUE);            \
    func((HDC)wParam);              \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnClose()
#define CR_MSG_WM_CLOSE(func)       \
  if (uMsg == WM_CLOSE) {           \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// BOOL OnQueryEndSession(UINT nSource, UINT uLogOff)
#define CR_MSG_WM_QUERYENDSESSION(func)                  \
  if (uMsg == WM_QUERYENDSESSION) {                      \
    SetMsgHandled(TRUE);                                 \
    lResult = (LRESULT)func((UINT)wParam, (UINT)lParam); \
    if (!ref.get() || msg_handled_)                      \
      return TRUE;                                       \
  }

// BOOL OnQueryOpen()
#define CR_MSG_WM_QUERYOPEN(func)   \
  if (uMsg == WM_QUERYOPEN) {       \
    SetMsgHandled(TRUE);            \
    lResult = (LRESULT)func();      \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// BOOL OnEraseBkgnd(CDCHandle dc)
#define CR_MSG_WM_ERASEBKGND(func)        \
  if (uMsg == WM_ERASEBKGND) {            \
    SetMsgHandled(TRUE);                  \
    lResult = (LRESULT)func((HDC)wParam); \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// void OnSysColorChange()
#define CR_MSG_WM_SYSCOLORCHANGE(func) \
  if (uMsg == WM_SYSCOLORCHANGE) {     \
    SetMsgHandled(TRUE);               \
    func();                            \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// void OnEndSession(BOOL bEnding, UINT uLogOff)
#define CR_MSG_WM_ENDSESSION(func)    \
  if (uMsg == WM_ENDSESSION) {        \
    SetMsgHandled(TRUE);              \
    func((BOOL)wParam, (UINT)lParam); \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnShowWindow(BOOL bShow, UINT nStatus)
#define CR_MSG_WM_SHOWWINDOW(func)   \
  if (uMsg == WM_SHOWWINDOW) {       \
    SetMsgHandled(TRUE);             \
    func((BOOL)wParam, (int)lParam); \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// HBRUSH OnCtlColorEdit(CDCHandle dc, CEdit edit)
#define CR_MSG_WM_CTLCOLOREDIT(func)                    \
  if (uMsg == WM_CTLCOLOREDIT) {                        \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnCtlColorListBox(CDCHandle dc, CListBox listBox)
#define CR_MSG_WM_CTLCOLORLISTBOX(func)                 \
  if (uMsg == WM_CTLCOLORLISTBOX) {                     \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnCtlColorBtn(CDCHandle dc, CButton button)
#define CR_MSG_WM_CTLCOLORBTN(func)                     \
  if (uMsg == WM_CTLCOLORBTN) {                         \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnCtlColorDlg(CDCHandle dc, CWindow wnd)
#define CR_MSG_WM_CTLCOLORDLG(func)                     \
  if (uMsg == WM_CTLCOLORDLG) {                         \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnCtlColorScrollBar(CDCHandle dc, CScrollBar scrollBar)
#define CR_MSG_WM_CTLCOLORSCROLLBAR(func)               \
  if (uMsg == WM_CTLCOLORSCROLLBAR) {                   \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnCtlColorStatic(CDCHandle dc, CStatic wndStatic)
#define CR_MSG_WM_CTLCOLORSTATIC(func)                  \
  if (uMsg == WM_CTLCOLORSTATIC) {                      \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// void OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
#define CR_MSG_WM_SETTINGCHANGE(func)    \
  if (uMsg == WM_SETTINGCHANGE) {        \
    SetMsgHandled(TRUE);                 \
    func((UINT)wParam, (LPCTSTR)lParam); \
    lResult = 0;                         \
    if (!ref.get() || msg_handled_)      \
      return TRUE;                       \
  }

// void OnDevModeChange(LPCTSTR lpDeviceName)
#define CR_MSG_WM_DEVMODECHANGE(func) \
  if (uMsg == WM_DEVMODECHANGE) {     \
    SetMsgHandled(TRUE);              \
    func((LPCTSTR)lParam);            \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnActivateApp(BOOL bActive, DWORD dwThreadID)
#define CR_MSG_WM_ACTIVATEAPP(func)    \
  if (uMsg == WM_ACTIVATEAPP) {        \
    SetMsgHandled(TRUE);               \
    func((BOOL)wParam, (DWORD)lParam); \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// void OnFontChange()
#define CR_MSG_WM_FONTCHANGE(func)  \
  if (uMsg == WM_FONTCHANGE) {      \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnTimeChange()
#define CR_MSG_WM_TIMECHANGE(func)  \
  if (uMsg == WM_TIMECHANGE) {      \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnCancelMode()
#define CR_MSG_WM_CANCELMODE(func)  \
  if (uMsg == WM_CANCELMODE) {      \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// BOOL OnSetCursor(CWindow wnd, UINT nHitTest, UINT message)
#define CR_MSG_WM_SETCURSOR(func)                               \
  if (uMsg == WM_SETCURSOR) {                                   \
    SetMsgHandled(TRUE);                                        \
    lResult = (LRESULT)func((HWND)wParam, (UINT)LOWORD(lParam), \
                            (UINT)HIWORD(lParam));              \
    if (!ref.get() || msg_handled_)                             \
      return TRUE;                                              \
  }

// int OnMouseActivate(CWindow wndTopLevel, UINT nHitTest, UINT message)
#define CR_MSG_WM_MOUSEACTIVATE(func)                           \
  if (uMsg == WM_MOUSEACTIVATE) {                               \
    SetMsgHandled(TRUE);                                        \
    lResult = (LRESULT)func((HWND)wParam, (UINT)LOWORD(lParam), \
                            (UINT)HIWORD(lParam));              \
    if (!ref.get() || msg_handled_)                             \
      return TRUE;                                              \
  }

// void OnChildActivate()
#define CR_MSG_WM_CHILDACTIVATE(func) \
  if (uMsg == WM_CHILDACTIVATE) {     \
    SetMsgHandled(TRUE);              \
    func();                           \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnGetMinMaxInfo(LPMINMAXINFO lpMMI)
#define CR_MSG_WM_GETMINMAXINFO(func) \
  if (uMsg == WM_GETMINMAXINFO) {     \
    SetMsgHandled(TRUE);              \
    func((LPMINMAXINFO)lParam);       \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnIconEraseBkgnd(CDCHandle dc)
#define CR_MSG_WM_ICONERASEBKGND(func) \
  if (uMsg == WM_ICONERASEBKGND) {     \
    SetMsgHandled(TRUE);               \
    func((HDC)wParam);                 \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// void OnSpoolerStatus(UINT nStatus, UINT nJobs)
#define CR_MSG_WM_SPOOLERSTATUS(func)         \
  if (uMsg == WM_SPOOLERSTATUS) {             \
    SetMsgHandled(TRUE);                      \
    func((UINT)wParam, (UINT)LOWORD(lParam)); \
    lResult = 0;                              \
    if (!ref.get() || msg_handled_)           \
      return TRUE;                            \
  }

// void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
#define CR_MSG_WM_DRAWITEM(func)                  \
  if (uMsg == WM_DRAWITEM) {                      \
    SetMsgHandled(TRUE);                          \
    func((UINT)wParam, (LPDRAWITEMSTRUCT)lParam); \
    lResult = TRUE;                               \
    if (!ref.get() || msg_handled_)               \
      return TRUE;                                \
  }

// void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
#define CR_MSG_WM_MEASUREITEM(func)                  \
  if (uMsg == WM_MEASUREITEM) {                      \
    SetMsgHandled(TRUE);                             \
    func((UINT)wParam, (LPMEASUREITEMSTRUCT)lParam); \
    lResult = TRUE;                                  \
    if (!ref.get() || msg_handled_)                  \
      return TRUE;                                   \
  }

// void OnDeleteItem(int nIDCtl, LPDELETEITEMSTRUCT lpDeleteItemStruct)
#define CR_MSG_WM_DELETEITEM(func)                  \
  if (uMsg == WM_DELETEITEM) {                      \
    SetMsgHandled(TRUE);                            \
    func((UINT)wParam, (LPDELETEITEMSTRUCT)lParam); \
    lResult = TRUE;                                 \
    if (!ref.get() || msg_handled_)                 \
      return TRUE;                                  \
  }

// int OnCharToItem(UINT nChar, UINT nIndex, CListBox listBox)
#define CR_MSG_WM_CHARTOITEM(func)                                      \
  if (uMsg == WM_CHARTOITEM) {                                          \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), \
                            (HWND)lParam);                              \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// int OnVKeyToItem(UINT nKey, UINT nIndex, CListBox listBox)
#define CR_MSG_WM_VKEYTOITEM(func)                                      \
  if (uMsg == WM_VKEYTOITEM) {                                          \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), \
                            (HWND)lParam);                              \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// HCURSOR OnQueryDragIcon()
#define CR_MSG_WM_QUERYDRAGICON(func) \
  if (uMsg == WM_QUERYDRAGICON) {     \
    SetMsgHandled(TRUE);              \
    lResult = (LRESULT)func();        \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// int OnCompareItem(int nIDCtl, LPCOMPAREITEMSTRUCT lpCompareItemStruct)
#define CR_MSG_WM_COMPAREITEM(func)                                     \
  if (uMsg == WM_COMPAREITEM) {                                         \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)wParam, (LPCOMPAREITEMSTRUCT)lParam); \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnCompacting(UINT nCpuTime)
#define CR_MSG_WM_COMPACTING(func)  \
  if (uMsg == WM_COMPACTING) {      \
    SetMsgHandled(TRUE);            \
    func((UINT)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// BOOL OnNcCreate(LPCREATESTRUCT lpCreateStruct)
#define CR_MSG_WM_NCCREATE(func)                     \
  if (uMsg == WM_NCCREATE) {                         \
    SetMsgHandled(TRUE);                             \
    lResult = (LRESULT)func((LPCREATESTRUCT)lParam); \
    if (!ref.get() || msg_handled_)                  \
      return TRUE;                                   \
  }

// void OnNcDestroy()
#define CR_MSG_WM_NCDESTROY(func)   \
  if (uMsg == WM_NCDESTROY) {       \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// LRESULT OnNcCalcSize(BOOL bCalcValidRects, LPARAM lParam)
#define CR_MSG_WM_NCCALCSIZE(func)        \
  if (uMsg == WM_NCCALCSIZE) {            \
    SetMsgHandled(TRUE);                  \
    lResult = func((BOOL)wParam, lParam); \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// UINT OnNcHitTest(gfx::Point point)
#define CR_MSG_WM_NCHITTEST(func)                                      \
  if (uMsg == WM_NCHITTEST) {                                          \
    SetMsgHandled(TRUE);                                               \
    lResult = (LRESULT)func(                                           \
        gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    if (!ref.get() || msg_handled_)                                    \
      return TRUE;                                                     \
  }

// void OnNcPaint(CRgn rgn)
#define CR_MSG_WM_NCPAINT(func)     \
  if (uMsg == WM_NCPAINT) {         \
    SetMsgHandled(TRUE);            \
    func((HRGN)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// BOOL OnNcActivate(BOOL bActive)
#define CR_MSG_WM_NCACTIVATE(func)         \
  if (uMsg == WM_NCACTIVATE) {             \
    SetMsgHandled(TRUE);                   \
    lResult = (LRESULT)func((BOOL)wParam); \
    if (!ref.get() || msg_handled_)        \
      return TRUE;                         \
  }

// UINT OnGetDlgCode(LPMSG lpMsg)
#define CR_MSG_WM_GETDLGCODE(func)          \
  if (uMsg == WM_GETDLGCODE) {              \
    SetMsgHandled(TRUE);                    \
    lResult = (LRESULT)func((LPMSG)lParam); \
    if (!ref.get() || msg_handled_)         \
      return TRUE;                          \
  }

// void OnNcMouseMove(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCMOUSEMOVE(func)                                     \
  if (uMsg == WM_NCMOUSEMOVE) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcLButtonDown(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCLBUTTONDOWN(func)                                   \
  if (uMsg == WM_NCLBUTTONDOWN) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcLButtonUp(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCLBUTTONUP(func)                                     \
  if (uMsg == WM_NCLBUTTONUP) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcLButtonDblClk(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCLBUTTONDBLCLK(func)                                 \
  if (uMsg == WM_NCLBUTTONDBLCLK) {                                     \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcRButtonDown(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCRBUTTONDOWN(func)                                   \
  if (uMsg == WM_NCRBUTTONDOWN) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcRButtonUp(UINT nHitTest, gfx::Point point)
#define CR_MSG_WM_NCRBUTTONUP(func)                                     \
  if (uMsg == WM_NCRBUTTONUP) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcRButtonDblClk(UINT nHitTest, CPoint point)
#define CR_MSG_WM_NCRBUTTONDBLCLK(func)                                 \
  if (uMsg == WM_NCRBUTTONDBLCLK) {                                     \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcMButtonDown(UINT nHitTest, CPoint point)
#define CR_MSG_WM_NCMBUTTONDOWN(func)                                   \
  if (uMsg == WM_NCMBUTTONDOWN) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcMButtonUp(UINT nHitTest, CPoint point)
#define CR_MSG_WM_NCMBUTTONUP(func)                                     \
  if (uMsg == WM_NCMBUTTONUP) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNcMButtonDblClk(UINT nHitTest, CPoint point)
#define CR_MSG_WM_NCMBUTTONDBLCLK(func)                                 \
  if (uMsg == WM_NCMBUTTONDBLCLK) {                                     \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_KEYDOWN(func)                \
  if (uMsg == WM_KEYDOWN) {                    \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_KEYUP(func)                  \
  if (uMsg == WM_KEYUP) {                      \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_CHAR(func)                   \
  if (uMsg == WM_CHAR) {                       \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnDeadChar(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_DEADCHAR(func)               \
  if (uMsg == WM_DEADCHAR) {                   \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_SYSKEYDOWN(func)             \
  if (uMsg == WM_SYSKEYDOWN) {                 \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_SYSKEYUP(func)               \
  if (uMsg == WM_SYSKEYUP) {                   \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSysChar(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_SYSCHAR(func)                \
  if (uMsg == WM_SYSCHAR) {                    \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSysDeadChar(UINT nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_SYSDEADCHAR(func)            \
  if (uMsg == WM_SYSDEADCHAR) {                \
    SetMsgHandled(TRUE);                       \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF, \
         (UINT)((lParam & 0xFFFF0000) >> 16)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSysCommand(UINT nID, LPARAM lParam)
#define CR_MSG_WM_SYSCOMMAND(func)                                      \
  if (uMsg == WM_SYSCOMMAND) {                                          \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnTCard(UINT idAction, DWORD dwActionData)
#define CR_MSG_WM_TCARD(func)          \
  if (uMsg == WM_TCARD) {              \
    SetMsgHandled(TRUE);               \
    func((UINT)wParam, (DWORD)lParam); \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// void OnTimer(UINT_PTR nIDEvent)
#define CR_MSG_WM_TIMER(func)       \
  if (uMsg == WM_TIMER) {           \
    SetMsgHandled(TRUE);            \
    func((UINT_PTR)wParam);         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
#define CR_MSG_WM_HSCROLL(func)                                     \
  if (uMsg == WM_HSCROLL) {                                         \
    SetMsgHandled(TRUE);                                            \
    func((int)LOWORD(wParam), (short)HIWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
#define CR_MSG_WM_VSCROLL(func)                                     \
  if (uMsg == WM_VSCROLL) {                                         \
    SetMsgHandled(TRUE);                                            \
    func((int)LOWORD(wParam), (short)HIWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnInitMenu(CMenu menu)
#define CR_MSG_WM_INITMENU(func)    \
  if (uMsg == WM_INITMENU) {        \
    SetMsgHandled(TRUE);            \
    func((HMENU)wParam);            \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnInitMenuPopup(CMenu menuPopup, UINT nIndex, BOOL bSysMenu)
#define CR_MSG_WM_INITMENUPOPUP(func)                                \
  if (uMsg == WM_INITMENUPOPUP) {                                    \
    SetMsgHandled(TRUE);                                             \
    func((HMENU)wParam, (UINT)LOWORD(lParam), (BOOL)HIWORD(lParam)); \
    lResult = 0;                                                     \
    if (!ref.get() || msg_handled_)                                  \
      return TRUE;                                                   \
  }

// void OnMenuSelect(UINT nItemID, UINT nFlags, CMenu menu)
#define CR_MSG_WM_MENUSELECT(func)                                   \
  if (uMsg == WM_MENUSELECT) {                                       \
    SetMsgHandled(TRUE);                                             \
    func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), (HMENU)lParam); \
    lResult = 0;                                                     \
    if (!ref.get() || msg_handled_)                                  \
      return TRUE;                                                   \
  }

// LRESULT OnMenuChar(UINT nChar, UINT nFlags, CMenu menu)
#define CR_MSG_WM_MENUCHAR(func)                                          \
  if (uMsg == WM_MENUCHAR) {                                              \
    SetMsgHandled(TRUE);                                                  \
    lResult =                                                             \
        func((TCHAR)LOWORD(wParam), (UINT)HIWORD(wParam), (HMENU)lParam); \
    if (!ref.get() || msg_handled_)                                       \
      return TRUE;                                                        \
  }

// LRESULT OnNotify(int idCtrl, LPNMHDR pnmh)
#define CR_MSG_WM_NOTIFY(func)                    \
  if (uMsg == WM_NOTIFY) {                        \
    SetMsgHandled(TRUE);                          \
    lResult = func((int)wParam, (LPNMHDR)lParam); \
    if (!ref.get() || msg_handled_)               \
      return TRUE;                                \
  }

// void OnEnterIdle(UINT nWhy, CWindow wndWho)
#define CR_MSG_WM_ENTERIDLE(func)     \
  if (uMsg == WM_ENTERIDLE) {         \
    SetMsgHandled(TRUE);              \
    func((UINT)wParam, (HWND)lParam); \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnMouseMove(UINT nFlags, CPoint point)
#define CR_MSG_WM_MOUSEMOVE(func)                                       \
  if (uMsg == WM_MOUSEMOVE) {                                           \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
#define CR_MSG_WM_MOUSEWHEEL(func)                                     \
  if (uMsg == WM_MOUSEWHEEL) {                                         \
    SetMsgHandled(TRUE);                                               \
    lResult = (LRESULT)func(                                           \
        (UINT)LOWORD(wParam), (short)HIWORD(wParam),                   \
        gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    if (!ref.get() || msg_handled_)                                    \
      return TRUE;                                                     \
  }

// void OnLButtonDown(UINT nFlags, CPoint point)
#define CR_MSG_WM_LBUTTONDOWN(func)                                     \
  if (uMsg == WM_LBUTTONDOWN) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnLButtonUp(UINT nFlags, CPoint point)
#define CR_MSG_WM_LBUTTONUP(func)                                       \
  if (uMsg == WM_LBUTTONUP) {                                           \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnLButtonDblClk(UINT nFlags, CPoint point)
#define CR_MSG_WM_LBUTTONDBLCLK(func)                                   \
  if (uMsg == WM_LBUTTONDBLCLK) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnRButtonDown(UINT nFlags, CPoint point)
#define CR_MSG_WM_RBUTTONDOWN(func)                                     \
  if (uMsg == WM_RBUTTONDOWN) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnRButtonUp(UINT nFlags, CPoint point)
#define CR_MSG_WM_RBUTTONUP(func)                                       \
  if (uMsg == WM_RBUTTONUP) {                                           \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnRButtonDblClk(UINT nFlags, CPoint point)
#define CR_MSG_WM_RBUTTONDBLCLK(func)                                   \
  if (uMsg == WM_RBUTTONDBLCLK) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnMButtonDown(UINT nFlags, CPoint point)
#define CR_MSG_WM_MBUTTONDOWN(func)                                     \
  if (uMsg == WM_MBUTTONDOWN) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnMButtonUp(UINT nFlags, CPoint point)
#define CR_MSG_WM_MBUTTONUP(func)                                       \
  if (uMsg == WM_MBUTTONUP) {                                           \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnMButtonDblClk(UINT nFlags, CPoint point)
#define CR_MSG_WM_MBUTTONDBLCLK(func)                                   \
  if (uMsg == WM_MBUTTONDBLCLK) {                                       \
    SetMsgHandled(TRUE);                                                \
    func((UINT)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnParentNotify(UINT message, UINT nChildID, LPARAM lParam)
#define CR_MSG_WM_PARENTNOTIFY(func)                          \
  if (uMsg == WM_PARENTNOTIFY) {                              \
    SetMsgHandled(TRUE);                                      \
    func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), lParam); \
    lResult = 0;                                              \
    if (!ref.get() || msg_handled_)                           \
      return TRUE;                                            \
  }

// void OnMDIActivate(CWindow wndActivate, CWindow wndDeactivate)
#define CR_MSG_WM_MDIACTIVATE(func)   \
  if (uMsg == WM_MDIACTIVATE) {       \
    SetMsgHandled(TRUE);              \
    func((HWND)wParam, (HWND)lParam); \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnRenderFormat(UINT nFormat)
#define CR_MSG_WM_RENDERFORMAT(func) \
  if (uMsg == WM_RENDERFORMAT) {     \
    SetMsgHandled(TRUE);             \
    func((UINT)wParam);              \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// void OnRenderAllFormats()
#define CR_MSG_WM_RENDERALLFORMATS(func) \
  if (uMsg == WM_RENDERALLFORMATS) {     \
    SetMsgHandled(TRUE);                 \
    func();                              \
    lResult = 0;                         \
    if (!ref.get() || msg_handled_)      \
      return TRUE;                       \
  }

// void OnDestroyClipboard()
#define CR_MSG_WM_DESTROYCLIPBOARD(func) \
  if (uMsg == WM_DESTROYCLIPBOARD) {     \
    SetMsgHandled(TRUE);                 \
    func();                              \
    lResult = 0;                         \
    if (!ref.get() || msg_handled_)      \
      return TRUE;                       \
  }

// void OnDrawClipboard()
#define CR_MSG_WM_DRAWCLIPBOARD(func) \
  if (uMsg == WM_DRAWCLIPBOARD) {     \
    SetMsgHandled(TRUE);              \
    func();                           \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnPaintClipboard(CWindow wndViewer, const LPPAINTSTRUCT lpPaintStruct)
#define CR_MSG_WM_PAINTCLIPBOARD(func)                                      \
  if (uMsg == WM_PAINTCLIPBOARD) {                                          \
    SetMsgHandled(TRUE);                                                    \
    func((HWND)wParam, (const LPPAINTSTRUCT)::GlobalLock((HGLOBAL)lParam)); \
    ::GlobalUnlock((HGLOBAL)lParam);                                        \
    lResult = 0;                                                            \
    if (!ref.get() || msg_handled_)                                         \
      return TRUE;                                                          \
  }

// void OnVScrollClipboard(CWindow wndViewer, UINT nSBCode, UINT nPos)
#define CR_MSG_WM_VSCROLLCLIPBOARD(func)                            \
  if (uMsg == WM_VSCROLLCLIPBOARD) {                                \
    SetMsgHandled(TRUE);                                            \
    func((HWND)wParam, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam)); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnContextMenu(CWindow wnd, CPoint point)
#define CR_MSG_WM_CONTEXTMENU(func)                                     \
  if (uMsg == WM_CONTEXTMENU) {                                         \
    SetMsgHandled(TRUE);                                                \
    func((HWND)wParam,                                                  \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnSizeClipboard(CWindow wndViewer, const LPRECT lpRect)
#define CR_MSG_WM_SIZECLIPBOARD(func)                                \
  if (uMsg == WM_SIZECLIPBOARD) {                                    \
    SetMsgHandled(TRUE);                                             \
    func((HWND)wParam, (const LPRECT)::GlobalLock((HGLOBAL)lParam)); \
    ::GlobalUnlock((HGLOBAL)lParam);                                 \
    lResult = 0;                                                     \
    if (!ref.get() || msg_handled_)                                  \
      return TRUE;                                                   \
  }

// void OnAskCbFormatName(UINT nMaxCount, LPTSTR lpszString)
#define CR_MSG_WM_ASKCBFORMATNAME(func)  \
  if (uMsg == WM_ASKCBFORMATNAME) {      \
    SetMsgHandled(TRUE);                 \
    func((DWORD)wParam, (LPTSTR)lParam); \
    lResult = 0;                         \
    if (!ref.get() || msg_handled_)      \
      return TRUE;                       \
  }

// void OnChangeCbChain(CWindow wndRemove, CWindow wndAfter)
#define CR_MSG_WM_CHANGECBCHAIN(func) \
  if (uMsg == WM_CHANGECBCHAIN) {     \
    SetMsgHandled(TRUE);              \
    func((HWND)wParam, (HWND)lParam); \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnHScrollClipboard(CWindow wndViewer, UINT nSBCode, UINT nPos)
#define CR_MSG_WM_HSCROLLCLIPBOARD(func)                            \
  if (uMsg == WM_HSCROLLCLIPBOARD) {                                \
    SetMsgHandled(TRUE);                                            \
    func((HWND)wParam, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam)); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// BOOL OnQueryNewPalette()
#define CR_MSG_WM_QUERYNEWPALETTE(func) \
  if (uMsg == WM_QUERYNEWPALETTE) {     \
    SetMsgHandled(TRUE);                \
    lResult = (LRESULT)func();          \
    if (!ref.get() || msg_handled_)     \
      return TRUE;                      \
  }

// void OnPaletteChanged(CWindow wndFocus)
#define CR_MSG_WM_PALETTECHANGED(func) \
  if (uMsg == WM_PALETTECHANGED) {     \
    SetMsgHandled(TRUE);               \
    func((HWND)wParam);                \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// void OnPaletteIsChanging(CWindow wndPalChg)
#define CR_MSG_WM_PALETTEISCHANGING(func) \
  if (uMsg == WM_PALETTEISCHANGING) {     \
    SetMsgHandled(TRUE);                  \
    func((HWND)wParam);                   \
    lResult = 0;                          \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// void OnDropFiles(HDROP hDropInfo)
#define CR_MSG_WM_DROPFILES(func)   \
  if (uMsg == WM_DROPFILES) {       \
    SetMsgHandled(TRUE);            \
    func((HDROP)wParam);            \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnWindowPosChanging(LPWINDOWPOS lpWndPos)
#define CR_MSG_WM_WINDOWPOSCHANGING(func) \
  if (uMsg == WM_WINDOWPOSCHANGING) {     \
    SetMsgHandled(TRUE);                  \
    func((LPWINDOWPOS)lParam);            \
    lResult = 0;                          \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// void OnWindowPosChanged(LPWINDOWPOS lpWndPos)
#define CR_MSG_WM_WINDOWPOSCHANGED(func) \
  if (uMsg == WM_WINDOWPOSCHANGED) {     \
    SetMsgHandled(TRUE);                 \
    func((LPWINDOWPOS)lParam);           \
    lResult = 0;                         \
    if (!ref.get() || msg_handled_)      \
      return TRUE;                       \
  }

// void OnExitMenuLoop(BOOL fIsTrackPopupMenu)
#define CR_MSG_WM_EXITMENULOOP(func) \
  if (uMsg == WM_EXITMENULOOP) {     \
    SetMsgHandled(TRUE);             \
    func((BOOL)wParam);              \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// void OnEnterMenuLoop(BOOL fIsTrackPopupMenu)
#define CR_MSG_WM_ENTERMENULOOP(func) \
  if (uMsg == WM_ENTERMENULOOP) {     \
    SetMsgHandled(TRUE);              \
    func((BOOL)wParam);               \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnStyleChanged(int nStyleType, LPSTYLESTRUCT lpStyleStruct)
#define CR_MSG_WM_STYLECHANGED(func)           \
  if (uMsg == WM_STYLECHANGED) {               \
    SetMsgHandled(TRUE);                       \
    func((UINT)wParam, (LPSTYLESTRUCT)lParam); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnStyleChanging(int nStyleType, LPSTYLESTRUCT lpStyleStruct)
#define CR_MSG_WM_STYLECHANGING(func)          \
  if (uMsg == WM_STYLECHANGING) {              \
    SetMsgHandled(TRUE);                       \
    func((UINT)wParam, (LPSTYLESTRUCT)lParam); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSizing(UINT fwSide, LPRECT pRect)
#define CR_MSG_WM_SIZING(func)          \
  if (uMsg == WM_SIZING) {              \
    SetMsgHandled(TRUE);                \
    func((UINT)wParam, (LPRECT)lParam); \
    lResult = TRUE;                     \
    if (!ref.get() || msg_handled_)     \
      return TRUE;                      \
  }

// void OnMoving(UINT fwSide, LPRECT pRect)
#define CR_MSG_WM_MOVING(func)          \
  if (uMsg == WM_MOVING) {              \
    SetMsgHandled(TRUE);                \
    func((UINT)wParam, (LPRECT)lParam); \
    lResult = TRUE;                     \
    if (!ref.get() || msg_handled_)     \
      return TRUE;                      \
  }

// void OnCaptureChanged(CWindow wnd)
#define CR_MSG_WM_CAPTURECHANGED(func) \
  if (uMsg == WM_CAPTURECHANGED) {     \
    SetMsgHandled(TRUE);               \
    func((HWND)lParam);                \
    lResult = 0;                       \
    if (!ref.get() || msg_handled_)    \
      return TRUE;                     \
  }

// BOOL OnDeviceChange(UINT nEventType, DWORD dwData)
#define CR_MSG_WM_DEVICECHANGE(func)                      \
  if (uMsg == WM_DEVICECHANGE) {                          \
    SetMsgHandled(TRUE);                                  \
    lResult = (LRESULT)func((UINT)wParam, (DWORD)lParam); \
    if (!ref.get() || msg_handled_)                       \
      return TRUE;                                        \
  }

// void OnCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_MSG_WM_COMMAND(func)                                    \
  if (uMsg == WM_COMMAND) {                                        \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// void OnDisplayChange(UINT uBitsPerPixel, gfx::Size sizeScreen)
#define CR_MSG_WM_DISPLAYCHANGE(func)                                  \
  if (uMsg == WM_DISPLAYCHANGE) {                                      \
    SetMsgHandled(TRUE);                                               \
    func((UINT)wParam,                                                 \
         gfx::Size(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                       \
    if (!ref.get() || msg_handled_)                                    \
      return TRUE;                                                     \
  }

// void OnEnterSizeMove()
#define CR_MSG_WM_ENTERSIZEMOVE(func) \
  if (uMsg == WM_ENTERSIZEMOVE) {     \
    SetMsgHandled(TRUE);              \
    func();                           \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnExitSizeMove()
#define CR_MSG_WM_EXITSIZEMOVE(func) \
  if (uMsg == WM_EXITSIZEMOVE) {     \
    SetMsgHandled(TRUE);             \
    func();                          \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// HFONT OnGetFont()
#define CR_MSG_WM_GETFONT(func)     \
  if (uMsg == WM_GETFONT) {         \
    SetMsgHandled(TRUE);            \
    lResult = (LRESULT)func();      \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// LRESULT OnGetHotKey()
#define CR_MSG_WM_GETHOTKEY(func)   \
  if (uMsg == WM_GETHOTKEY) {       \
    SetMsgHandled(TRUE);            \
    lResult = func();               \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// HICON OnGetIcon()
#define CR_MSG_WM_GETICON(func)            \
  if (uMsg == WM_GETICON) {                \
    SetMsgHandled(TRUE);                   \
    lResult = (LRESULT)func((UINT)wParam); \
    if (!ref.get() || msg_handled_)        \
      return TRUE;                         \
  }

// int OnGetText(int cchTextMax, LPTSTR lpszText)
#define CR_MSG_WM_GETTEXT(func)                           \
  if (uMsg == WM_GETTEXT) {                               \
    SetMsgHandled(TRUE);                                  \
    lResult = (LRESULT)func((int)wParam, (LPTSTR)lParam); \
    if (!ref.get() || msg_handled_)                       \
      return TRUE;                                        \
  }

// int OnGetTextLength()
#define CR_MSG_WM_GETTEXTLENGTH(func) \
  if (uMsg == WM_GETTEXTLENGTH) {     \
    SetMsgHandled(TRUE);              \
    lResult = (LRESULT)func();        \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// void OnHelp(LPHELPINFO lpHelpInfo)
#define CR_MSG_WM_HELP(func)        \
  if (uMsg == WM_HELP) {            \
    SetMsgHandled(TRUE);            \
    func((LPHELPINFO)lParam);       \
    lResult = TRUE;                 \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnHotKey(int nHotKeyID, UINT uModifiers, UINT uVirtKey)
#define CR_MSG_WM_HOTKEY(func)                                     \
  if (uMsg == WM_HOTKEY) {                                         \
    SetMsgHandled(TRUE);                                           \
    func((int)wParam, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam)); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// void OnInputLangChange(DWORD dwCharSet, HKL hKbdLayout)
#define CR_MSG_WM_INPUTLANGCHANGE(func) \
  if (uMsg == WM_INPUTLANGCHANGE) {     \
    SetMsgHandled(TRUE);                \
    func((DWORD)wParam, (HKL)lParam);   \
    lResult = TRUE;                     \
    if (!ref.get() || msg_handled_)     \
      return TRUE;                      \
  }

// void OnInputLangChangeRequest(BOOL bSysCharSet, HKL hKbdLayout)
#define CR_MSG_WM_INPUTLANGCHANGEREQUEST(func) \
  if (uMsg == WM_INPUTLANGCHANGEREQUEST) {     \
    SetMsgHandled(TRUE);                       \
    func((BOOL)wParam, (HKL)lParam);           \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnNextDlgCtl(BOOL bHandle, WPARAM wCtlFocus)
#define CR_MSG_WM_NEXTDLGCTL(func)      \
  if (uMsg == WM_NEXTDLGCTL) {          \
    SetMsgHandled(TRUE);                \
    func((BOOL)LOWORD(lParam), wParam); \
    lResult = 0;                        \
    if (!ref.get() || msg_handled_)     \
      return TRUE;                      \
  }

// void OnNextMenu(int nVirtKey, LPMDINEXTMENU lpMdiNextMenu)
#define CR_MSG_WM_NEXTMENU(func)              \
  if (uMsg == WM_NEXTMENU) {                  \
    SetMsgHandled(TRUE);                      \
    func((int)wParam, (LPMDINEXTMENU)lParam); \
    lResult = 0;                              \
    if (!ref.get() || msg_handled_)           \
      return TRUE;                            \
  }

// int OnNotifyFormat(CWindow wndFrom, int nCommand)
#define CR_MSG_WM_NOTIFYFORMAT(func)                    \
  if (uMsg == WM_NOTIFYFORMAT) {                        \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HWND)wParam, (int)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// BOOL OnPowerBroadcast(DWORD dwPowerEvent, DWORD dwData)
#define CR_MSG_WM_POWERBROADCAST(func)                     \
  if (uMsg == WM_POWERBROADCAST) {                         \
    SetMsgHandled(TRUE);                                   \
    lResult = (LRESULT)func((DWORD)wParam, (DWORD)lParam); \
    if (!ref.get() || msg_handled_)                        \
      return TRUE;                                         \
  }

// void OnPrint(CDCHandle dc, UINT uFlags)
#define CR_MSG_WM_PRINT(func)        \
  if (uMsg == WM_PRINT) {            \
    SetMsgHandled(TRUE);             \
    func((HDC)wParam, (UINT)lParam); \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// void OnPrintClient(CDCHandle dc, UINT uFlags)
#define CR_MSG_WM_PRINTCLIENT(func)  \
  if (uMsg == WM_PRINTCLIENT) {      \
    SetMsgHandled(TRUE);             \
    func((HDC)wParam, (UINT)lParam); \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

// void OnRasDialEvent(RASCONNSTATE rasconnstate, DWORD dwError)
#define CR_MSG_WM_RASDIALEVENT(func)           \
  if (uMsg == WM_RASDIALEVENT) {               \
    SetMsgHandled(TRUE);                       \
    func((RASCONNSTATE)wParam, (DWORD)lParam); \
    lResult = TRUE;                            \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnSetFont(CFont font, BOOL bRedraw)
#define CR_MSG_WM_SETFONT(func)                \
  if (uMsg == WM_SETFONT) {                    \
    SetMsgHandled(TRUE);                       \
    func((HFONT)wParam, (BOOL)LOWORD(lParam)); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// int OnSetHotKey(int nVirtKey, UINT uFlags)
#define CR_MSG_WM_SETHOTKEY(func)                          \
  if (uMsg == WM_SETHOTKEY) {                              \
    SetMsgHandled(TRUE);                                   \
    lResult = (LRESULT)func((int)LOBYTE(LOWORD(wParam)),   \
                            (UINT)HIBYTE(LOWORD(wParam))); \
    if (!ref.get() || msg_handled_)                        \
      return TRUE;                                         \
  }

// HICON OnSetIcon(UINT uType, HICON hIcon)
#define CR_MSG_WM_SETICON(func)                           \
  if (uMsg == WM_SETICON) {                               \
    SetMsgHandled(TRUE);                                  \
    lResult = (LRESULT)func((UINT)wParam, (HICON)lParam); \
    if (!ref.get() || msg_handled_)                       \
      return TRUE;                                        \
  }

// void OnSetRedraw(BOOL bRedraw)
#define CR_MSG_WM_SETREDRAW(func)   \
  if (uMsg == WM_SETREDRAW) {       \
    SetMsgHandled(TRUE);            \
    func((BOOL)wParam);             \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// int OnSetText(LPCTSTR lpstrText)
#define CR_MSG_WM_SETTEXT(func)               \
  if (uMsg == WM_SETTEXT) {                   \
    SetMsgHandled(TRUE);                      \
    lResult = (LRESULT)func((LPCTSTR)lParam); \
    if (!ref.get() || msg_handled_)           \
      return TRUE;                            \
  }

// void OnUserChanged()
#define CR_MSG_WM_USERCHANGED(func) \
  if (uMsg == WM_USERCHANGED) {     \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

///////////////////////////////////////////////////////////////////////////////
// New NT4 & NT5 messages

#if (_WIN32_WINNT >= 0x0400)

// void OnMouseHover(WPARAM wParam, CPoint ptPos)
#define CR_MSG_WM_MOUSEHOVER(func)                                      \
  if (uMsg == WM_MOUSEHOVER) {                                          \
    SetMsgHandled(TRUE);                                                \
    func(wParam,                                                        \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnMouseLeave()
#define CR_MSG_WM_MOUSELEAVE(func)  \
  if (uMsg == WM_MOUSELEAVE) {      \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

#endif /* _WIN32_WINNT >= 0x0400 */

#if (WINVER >= 0x0500)

// void OnMenuRButtonUp(WPARAM wParam, CMenu menu)
#define CR_MSG_WM_MENURBUTTONUP(func) \
  if (uMsg == WM_MENURBUTTONUP) {     \
    SetMsgHandled(TRUE);              \
    func(wParam, (HMENU)lParam);      \
    lResult = 0;                      \
    if (!ref.get() || msg_handled_)   \
      return TRUE;                    \
  }

// LRESULT OnMenuDrag(WPARAM wParam, CMenu menu)
#define CR_MSG_WM_MENUDRAG(func)           \
  if (uMsg == WM_MENUDRAG) {               \
    SetMsgHandled(TRUE);                   \
    lResult = func(wParam, (HMENU)lParam); \
    if (!ref.get() || msg_handled_)        \
      return TRUE;                         \
  }

// LRESULT OnMenuGetObject(PMENUGETOBJECTINFO info)
#define CR_MSG_WM_MENUGETOBJECT(func)           \
  if (uMsg == WM_MENUGETOBJECT) {               \
    SetMsgHandled(TRUE);                        \
    lResult = func((PMENUGETOBJECTINFO)lParam); \
    if (!ref.get() || msg_handled_)             \
      return TRUE;                              \
  }

// void OnUnInitMenuPopup(UINT nID, CMenu menu)
#define CR_MSG_WM_UNINITMENUPOPUP(func)        \
  if (uMsg == WM_UNINITMENUPOPUP) {            \
    SetMsgHandled(TRUE);                       \
    func((UINT)HIWORD(lParam), (HMENU)wParam); \
    lResult = 0;                               \
    if (!ref.get() || msg_handled_)            \
      return TRUE;                             \
  }

// void OnMenuCommand(WPARAM nIndex, CMenu menu)
#define CR_MSG_WM_MENUCOMMAND(func) \
  if (uMsg == WM_MENUCOMMAND) {     \
    SetMsgHandled(TRUE);            \
    func(wParam, (HMENU)lParam);    \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

#endif /* WINVER >= 0x0500 */

#if (_WIN32_WINNT >= 0x0500)

// BOOL OnAppCommand(CWindow wndFocus, short cmd, WORD uDevice, int dwKeys)
#define CR_MSG_WM_APPCOMMAND(func)                                             \
  if (uMsg == WM_APPCOMMAND) {                                                 \
    SetMsgHandled(TRUE);                                                       \
    lResult =                                                                  \
        (LRESULT)func((HWND)wParam, GET_APPCOMMAND_LPARAM(lParam),             \
                      GET_DEVICE_LPARAM(lParam), GET_KEYSTATE_LPARAM(lParam)); \
    if (!ref.get() || msg_handled_)                                            \
      return TRUE;                                                             \
  }

// void OnNCXButtonDown(int fwButton, short nHittest, CPoint ptPos)
#define CR_MSG_WM_NCXBUTTONDOWN(func)                                   \
  if (uMsg == WM_NCXBUTTONDOWN) {                                       \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_NCHITTEST_WPARAM(wParam),      \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNCXButtonUp(int fwButton, short nHittest, CPoint ptPos)
#define CR_MSG_WM_NCXBUTTONUP(func)                                     \
  if (uMsg == WM_NCXBUTTONUP) {                                         \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_NCHITTEST_WPARAM(wParam),      \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnNCXButtonDblClk(int fwButton, short nHittest, CPoint ptPos)
#define CR_MSG_WM_NCXBUTTONDBLCLK(func)                                 \
  if (uMsg == WM_NCXBUTTONDBLCLK) {                                     \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_NCHITTEST_WPARAM(wParam),      \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnXButtonDown(int fwButton, int dwKeys, CPoint ptPos)
#define CR_MSG_WM_XBUTTONDOWN(func)                                     \
  if (uMsg == WM_XBUTTONDOWN) {                                         \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam),       \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnXButtonUp(int fwButton, int dwKeys, CPoint ptPos)
#define CR_MSG_WM_XBUTTONUP(func)                                       \
  if (uMsg == WM_XBUTTONUP) {                                           \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam),       \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnXButtonDblClk(int fwButton, int dwKeys, CPoint ptPos)
#define CR_MSG_WM_XBUTTONDBLCLK(func)                                   \
  if (uMsg == WM_XBUTTONDBLCLK) {                                       \
    SetMsgHandled(TRUE);                                                \
    func(GET_XBUTTON_WPARAM(wParam), GET_KEYSTATE_WPARAM(wParam),       \
         gfx::Point(CR_GET_X_LPARAM(lParam), CR_GET_Y_LPARAM(lParam))); \
    lResult = 0;                                                        \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnChangeUIState(WORD nAction, WORD nState)
#define CR_MSG_WM_CHANGEUISTATE(func)     \
  if (uMsg == WM_CHANGEUISTATE) {         \
    SetMsgHandled(TRUE);                  \
    func(LOWORD(wParam), HIWORD(wParam)); \
    lResult = 0;                          \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// void OnUpdateUIState(WORD nAction, WORD nState)
#define CR_MSG_WM_UPDATEUISTATE(func)     \
  if (uMsg == WM_UPDATEUISTATE) {         \
    SetMsgHandled(TRUE);                  \
    func(LOWORD(wParam), HIWORD(wParam)); \
    lResult = 0;                          \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// LRESULT OnQueryUIState()
#define CR_MSG_WM_QUERYUISTATE(func) \
  if (uMsg == WM_QUERYUISTATE) {     \
    SetMsgHandled(TRUE);             \
    lResult = func();                \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

#endif  // (_WIN32_WINNT >= 0x0500)

#if (_WIN32_WINNT >= 0x0501)

// void OnInput(WPARAM RawInputCode, HRAWINPUT hRawInput)
#define CR_MSG_WM_INPUT(func)                                  \
  if (uMsg == WM_INPUT) {                                      \
    SetMsgHandled(TRUE);                                       \
    func(GET_RAWINPUT_CODE_WPARAM(wParam), (HRAWINPUT)lParam); \
    lResult = 0;                                               \
    if (!ref.get() || msg_handled_)                            \
      return TRUE;                                             \
  }

// void OnUniChar(TCHAR nChar, UINT nRepCnt, UINT nFlags)
#define CR_MSG_WM_UNICHAR(func)                            \
  if (uMsg == WM_UNICHAR) {                                \
    SetMsgHandled(TRUE);                                   \
    func((TCHAR)wParam, (UINT)lParam & 0xFFFF,             \
         (UINT)((lParam & 0xFFFF0000) >> 16));             \
    if (!ref.get() || msg_handled_) {                      \
      lResult = (wParam == UNICODE_NOCHAR) ? TRUE : FALSE; \
      return TRUE;                                         \
    }                                                      \
  }

// OnThemeChanged()
#define CR_MSG_WM_THEMECHANGED(func) \
  if (uMsg == WM_THEMECHANGED) {     \
    SetMsgHandled(TRUE);             \
    func();                          \
    lResult = 0;                     \
    if (!ref.get() || msg_handled_)  \
      return TRUE;                   \
  }

#endif /* _WIN32_WINNT >= 0x0501 */

///////////////////////////////////////////////////////////////////////////////
// ATL defined messages

// BOOL OnForwardMsg(LPMSG Msg, DWORD nUserData)
#define CR_MSG_WM_FORWARDMSG(func)                         \
  if (uMsg == WM_FORWARDMSG) {                             \
    SetMsgHandled(TRUE);                                   \
    lResult = (LRESULT)func((LPMSG)lParam, (DWORD)wParam); \
    if (!ref.get() || msg_handled_)                        \
      return TRUE;                                         \
  }

///////////////////////////////////////////////////////////////////////////////
// Dialog specific messages

// LRESULT OnDMGetDefID()
#define MSG_DM_GETDEFID(func)       \
  if (uMsg == DM_GETDEFID) {        \
    SetMsgHandled(TRUE);            \
    lResult = func();               \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnDMSetDefID(UINT DefID)
#define MSG_DM_SETDEFID(func)       \
  if (uMsg == DM_SETDEFID) {        \
    SetMsgHandled(TRUE);            \
    func((UINT)wParam);             \
    lResult = TRUE;                 \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnDMReposition()
#define MSG_DM_REPOSITION(func)     \
  if (uMsg == DM_REPOSITION) {      \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

///////////////////////////////////////////////////////////////////////////////
// Reflected messages

// void OnReflectedCommand(UINT uNotifyCode, int nID, CWindow wndCtl)
#define MSG_OCM_COMMAND(func)                                      \
  if (uMsg == OCM_COMMAND) {                                       \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnReflectedNotify(int idCtrl, LPNMHDR pnmh)
#define MSG_OCM_NOTIFY(func)                      \
  if (uMsg == OCM_NOTIFY) {                       \
    SetMsgHandled(TRUE);                          \
    lResult = func((int)wParam, (LPNMHDR)lParam); \
    if (!ref.get() || msg_handled_)               \
      return TRUE;                                \
  }

// void OnReflectedParentNotify(UINT message, UINT nChildID, LPARAM lParam)
#define MSG_OCM_PARENTNOTIFY(func)                            \
  if (uMsg == OCM_PARENTNOTIFY) {                             \
    SetMsgHandled(TRUE);                                      \
    func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), lParam); \
    lResult = 0;                                              \
    if (!ref.get() || msg_handled_)                           \
      return TRUE;                                            \
  }

// void OnReflectedDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
#define MSG_OCM_DRAWITEM(func)                    \
  if (uMsg == OCM_DRAWITEM) {                     \
    SetMsgHandled(TRUE);                          \
    func((UINT)wParam, (LPDRAWITEMSTRUCT)lParam); \
    lResult = TRUE;                               \
    if (!ref.get() || msg_handled_)               \
      return TRUE;                                \
  }

// void OnReflectedMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT
// lpMeasureItemStruct)
#define MSG_OCM_MEASUREITEM(func)                    \
  if (uMsg == OCM_MEASUREITEM) {                     \
    SetMsgHandled(TRUE);                             \
    func((UINT)wParam, (LPMEASUREITEMSTRUCT)lParam); \
    lResult = TRUE;                                  \
    if (!ref.get() || msg_handled_)                  \
      return TRUE;                                   \
  }

// int OnReflectedCompareItem(int nIDCtl, LPCOMPAREITEMSTRUCT
// lpCompareItemStruct)
#define MSG_OCM_COMPAREITEM(func)                                       \
  if (uMsg == OCM_COMPAREITEM) {                                        \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)wParam, (LPCOMPAREITEMSTRUCT)lParam); \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnReflectedDeleteItem(int nIDCtl, LPDELETEITEMSTRUCT lpDeleteItemStruct)
#define MSG_OCM_DELETEITEM(func)                    \
  if (uMsg == OCM_DELETEITEM) {                     \
    SetMsgHandled(TRUE);                            \
    func((UINT)wParam, (LPDELETEITEMSTRUCT)lParam); \
    lResult = TRUE;                                 \
    if (!ref.get() || msg_handled_)                 \
      return TRUE;                                  \
  }

// int OnReflectedVKeyToItem(UINT nKey, UINT nIndex, CListBox listBox)
#define MSG_OCM_VKEYTOITEM(func)                                        \
  if (uMsg == OCM_VKEYTOITEM) {                                         \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), \
                            (HWND)lParam);                              \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// int OnReflectedCharToItem(UINT nChar, UINT nIndex, CListBox listBox)
#define MSG_OCM_CHARTOITEM(func)                                        \
  if (uMsg == OCM_CHARTOITEM) {                                         \
    SetMsgHandled(TRUE);                                                \
    lResult = (LRESULT)func((UINT)LOWORD(wParam), (UINT)HIWORD(wParam), \
                            (HWND)lParam);                              \
    if (!ref.get() || msg_handled_)                                     \
      return TRUE;                                                      \
  }

// void OnReflectedHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
#define MSG_OCM_HSCROLL(func)                                       \
  if (uMsg == OCM_HSCROLL) {                                        \
    SetMsgHandled(TRUE);                                            \
    func((int)LOWORD(wParam), (short)HIWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// void OnReflectedVScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
#define MSG_OCM_VSCROLL(func)                                       \
  if (uMsg == OCM_VSCROLL) {                                        \
    SetMsgHandled(TRUE);                                            \
    func((int)LOWORD(wParam), (short)HIWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                    \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// HBRUSH OnReflectedCtlColorEdit(CDCHandle dc, CEdit edit)
#define MSG_OCM_CTLCOLOREDIT(func)                      \
  if (uMsg == OCM_CTLCOLOREDIT) {                       \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnReflectedCtlColorListBox(CDCHandle dc, CListBox listBox)
#define MSG_OCM_CTLCOLORLISTBOX(func)                   \
  if (uMsg == OCM_CTLCOLORLISTBOX) {                    \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnReflectedCtlColorBtn(CDCHandle dc, CButton button)
#define MSG_OCM_CTLCOLORBTN(func)                       \
  if (uMsg == OCM_CTLCOLORBTN) {                        \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnReflectedCtlColorDlg(CDCHandle dc, CWindow wnd)
#define MSG_OCM_CTLCOLORDLG(func)                       \
  if (uMsg == OCM_CTLCOLORDLG) {                        \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnReflectedCtlColorScrollBar(CDCHandle dc, CScrollBar scrollBar)
#define MSG_OCM_CTLCOLORSCROLLBAR(func)                 \
  if (uMsg == OCM_CTLCOLORSCROLLBAR) {                  \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

// HBRUSH OnReflectedCtlColorStatic(CDCHandle dc, CStatic wndStatic)
#define MSG_OCM_CTLCOLORSTATIC(func)                    \
  if (uMsg == OCM_CTLCOLORSTATIC) {                     \
    SetMsgHandled(TRUE);                                \
    lResult = (LRESULT)func((HDC)wParam, (HWND)lParam); \
    if (!ref.get() || msg_handled_)                     \
      return TRUE;                                      \
  }

///////////////////////////////////////////////////////////////////////////////
// Edit specific messages

// void OnClear()
#define CR_MSG_WM_CLEAR(func)       \
  if (uMsg == WM_CLEAR) {           \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnCopy()
#define CR_MSG_WM_COPY(func)        \
  if (uMsg == WM_COPY) {            \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnCut()
#define CR_MSG_WM_CUT(func)         \
  if (uMsg == WM_CUT) {             \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnPaste()
#define CR_MSG_WM_PASTE(func)       \
  if (uMsg == WM_PASTE) {           \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

// void OnUndo()
#define CR_MSG_WM_UNDO(func)        \
  if (uMsg == WM_UNDO) {            \
    SetMsgHandled(TRUE);            \
    func();                         \
    lResult = 0;                    \
    if (!ref.get() || msg_handled_) \
      return TRUE;                  \
  }

///////////////////////////////////////////////////////////////////////////////
// Generic message handlers

// LRESULT OnMessageHandlerEX(UINT uMsg, WPARAM wParam, LPARAM lParam)
#define CR_MESSAGE_HANDLER_EX(msg, func)  \
  if (uMsg == msg) {                      \
    SetMsgHandled(TRUE);                  \
    lResult = func(uMsg, wParam, lParam); \
    if (!ref.get() || msg_handled_)       \
      return TRUE;                        \
  }

// LRESULT OnMessageRangeHandlerEX(UINT uMsg, WPARAM wParam, LPARAM lParam)
#define CR_MESSAGE_RANGE_HANDLER_EX(msgFirst, msgLast, func) \
  if (uMsg >= msgFirst && uMsg <= msgLast) {                 \
    SetMsgHandled(TRUE);                                     \
    lResult = func(uMsg, wParam, lParam);                    \
    if (!ref.get() || msg_handled_)                          \
      return TRUE;                                           \
  }

///////////////////////////////////////////////////////////////////////////////
// Commands and notifications

// void OnCommandHandlerEX(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_COMMAND_HANDLER_EX(id, code, func)                                 \
  if (uMsg == WM_COMMAND && code == HIWORD(wParam) && id == LOWORD(wParam)) { \
    SetMsgHandled(TRUE);                                                      \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam);            \
    lResult = 0;                                                              \
    if (!ref.get() || msg_handled_)                                           \
      return TRUE;                                                            \
  }

// void OnCommandIDHandlerEX(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_COMMAND_ID_HANDLER_EX(id, func)                         \
  if (uMsg == WM_COMMAND && id == LOWORD(wParam)) {                \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// void OnCommandCodeHandlerEX(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_COMMAND_CODE_HANDLER_EX(code, func)                     \
  if (uMsg == WM_COMMAND && code == HIWORD(wParam)) {              \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnNotifyHandlerEX(LPNMHDR pnmh)
#define CR_NOTIFY_HANDLER_EX(id, cd, func)                  \
  if (uMsg == WM_NOTIFY && cd == ((LPNMHDR)lParam)->code && \
      id == ((LPNMHDR)lParam)->idFrom) {                    \
    SetMsgHandled(TRUE);                                    \
    lResult = func((LPNMHDR)lParam);                        \
    if (!ref.get() || msg_handled_)                         \
      return TRUE;                                          \
  }

// LRESULT OnNotifyIDHandlerEX(LPNMHDR pnmh)
#define CR_NOTIFY_ID_HANDLER_EX(id, func)                     \
  if (uMsg == WM_NOTIFY && id == ((LPNMHDR)lParam)->idFrom) { \
    SetMsgHandled(TRUE);                                      \
    lResult = func((LPNMHDR)lParam);                          \
    if (!ref.get() || msg_handled_)                           \
      return TRUE;                                            \
  }

// LRESULT OnNotifyCodeHandlerEX(LPNMHDR pnmh)
#define CR_NOTIFY_CODE_HANDLER_EX(cd, func)                 \
  if (uMsg == WM_NOTIFY && cd == ((LPNMHDR)lParam)->code) { \
    SetMsgHandled(TRUE);                                    \
    lResult = func((LPNMHDR)lParam);                        \
    if (!ref.get() || msg_handled_)                         \
      return TRUE;                                          \
  }

// void OnCommandRangeHandlerEX(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_COMMAND_RANGE_HANDLER_EX(idFirst, idLast, func)         \
  if (uMsg == WM_COMMAND && LOWORD(wParam) >= idFirst &&           \
      LOWORD(wParam) <= idLast) {                                  \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// void OnCommandRangeCodeHandlerEX(UINT uNotifyCode, int nID, CWindow wndCtl)
#define CR_COMMAND_RANGE_CODE_HANDLER_EX(idFirst, idLast, code, func) \
  if (uMsg == WM_COMMAND && code == HIWORD(wParam) &&                 \
      LOWORD(wParam) >= idFirst && LOWORD(wParam) <= idLast) {        \
    SetMsgHandled(TRUE);                                              \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam);    \
    lResult = 0;                                                      \
    if (!ref.get() || msg_handled_)                                   \
      return TRUE;                                                    \
  }

// LRESULT OnNotifyRangeHandlerEX(LPNMHDR pnmh)
#define CR_NOTIFY_RANGE_HANDLER_EX(idFirst, idLast, func)          \
  if (uMsg == WM_NOTIFY && ((LPNMHDR)lParam)->idFrom >= idFirst && \
      ((LPNMHDR)lParam)->idFrom <= idLast) {                       \
    SetMsgHandled(TRUE);                                           \
    lResult = func((LPNMHDR)lParam);                               \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnNotifyRangeCodeHandlerEX(LPNMHDR pnmh)
#define CR_NOTIFY_RANGE_CODE_HANDLER_EX(idFirst, idLast, cd, func) \
  if (uMsg == WM_NOTIFY && cd == ((LPNMHDR)lParam)->code &&        \
      ((LPNMHDR)lParam)->idFrom >= idFirst &&                      \
      ((LPNMHDR)lParam)->idFrom <= idLast) {                       \
    SetMsgHandled(TRUE);                                           \
    lResult = func((LPNMHDR)lParam);                               \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnReflectedCommandHandlerEX(UINT uNotifyCode, int nID, CWindow
// wndCtl)
#define CR_REFLECTED_COMMAND_HANDLER_EX(id, code, func)                        \
  if (uMsg == OCM_COMMAND && code == HIWORD(wParam) && id == LOWORD(wParam)) { \
    SetMsgHandled(TRUE);                                                       \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam);             \
    lResult = 0;                                                               \
    if (!ref.get() || msg_handled_)                                            \
      return TRUE;                                                             \
  }

// LRESULT OnReflectedCommandIDHandlerEX(UINT uNotifyCode, int nID, CWindow
// wndCtl)
#define CR_REFLECTED_COMMAND_ID_HANDLER_EX(id, func)               \
  if (uMsg == OCM_COMMAND && id == LOWORD(wParam)) {               \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnReflectedCommandCodeHandlerEX(UINT uNotifyCode, int nID, CWindow
// wndCtl)
#define CR_REFLECTED_COMMAND_CODE_HANDLER_EX(code, func)           \
  if (uMsg == OCM_COMMAND && code == HIWORD(wParam)) {             \
    SetMsgHandled(TRUE);                                           \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam); \
    lResult = 0;                                                   \
    if (!ref.get() || msg_handled_)                                \
      return TRUE;                                                 \
  }

// LRESULT OnReflectedNotifyHandlerEX(LPNMHDR pnmh)
#define CR_REFLECTED_NOTIFY_HANDLER_EX(id, cd, func)         \
  if (uMsg == OCM_NOTIFY && cd == ((LPNMHDR)lParam)->code && \
      id == ((LPNMHDR)lParam)->idFrom) {                     \
    SetMsgHandled(TRUE);                                     \
    lResult = func((LPNMHDR)lParam);                         \
    if (!ref.get() || msg_handled_)                          \
      return TRUE;                                           \
  }

// LRESULT OnReflectedNotifyIDHandlerEX(LPNMHDR pnmh)
#define CR_REFLECTED_NOTIFY_ID_HANDLER_EX(id, func)            \
  if (uMsg == OCM_NOTIFY && id == ((LPNMHDR)lParam)->idFrom) { \
    SetMsgHandled(TRUE);                                       \
    lResult = func((LPNMHDR)lParam);                           \
    if (!ref.get() || msg_handled_)                            \
      return TRUE;                                             \
  }

// LRESULT OnReflectedNotifyCodeHandlerEX(LPNMHDR pnmh)
#define CR_REFLECTED_NOTIFY_CODE_HANDLER_EX(cd, func)        \
  if (uMsg == OCM_NOTIFY && cd == ((LPNMHDR)lParam)->code) { \
    SetMsgHandled(TRUE);                                     \
    lResult = func((LPNMHDR)lParam);                         \
    if (!ref.get() || msg_handled_)                          \
      return TRUE;                                           \
  }

// void OnReflectedCommandRangeHandlerEX(UINT uNotifyCode, int nID, CWindow
// wndCtl)
#define CR_REFLECTED_COMMAND_RANGE_HANDLER_EX(idFirst, idLast, func) \
  if (uMsg == OCM_COMMAND && LOWORD(wParam) >= idFirst &&            \
      LOWORD(wParam) <= idLast) {                                    \
    SetMsgHandled(TRUE);                                             \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam);   \
    lResult = 0;                                                     \
    if (!ref.get() || msg_handled_)                                  \
      return TRUE;                                                   \
  }

// void OnReflectedCommandRangeCodeHandlerEX(UINT uNotifyCode, int nID, CWindow
// wndCtl)
#define CR_REFLECTED_COMMAND_RANGE_CODE_HANDLER_EX(idFirst, idLast, code, \
                                                   func)                  \
  if (uMsg == OCM_COMMAND && code == HIWORD(wParam) &&                    \
      LOWORD(wParam) >= idFirst && LOWORD(wParam) <= idLast) {            \
    SetMsgHandled(TRUE);                                                  \
    func((UINT)HIWORD(wParam), (int)LOWORD(wParam), (HWND)lParam);        \
    lResult = 0;                                                          \
    if (!ref.get() || msg_handled_)                                       \
      return TRUE;                                                        \
  }

// LRESULT OnReflectedNotifyRangeHandlerEX(LPNMHDR pnmh)
#define CR_REFLECTED_NOTIFY_RANGE_HANDLER_EX(idFirst, idLast, func) \
  if (uMsg == OCM_NOTIFY && ((LPNMHDR)lParam)->idFrom >= idFirst && \
      ((LPNMHDR)lParam)->idFrom <= idLast) {                        \
    SetMsgHandled(TRUE);                                            \
    lResult = func((LPNMHDR)lParam);                                \
    if (!ref.get() || msg_handled_)                                 \
      return TRUE;                                                  \
  }

// LRESULT OnReflectedNotifyRangeCodeHandlerEX(LPNMHDR pnmh)
#define CR_REFLECTED_NOTIFY_RANGE_CODE_HANDLER_EX(idFirst, idLast, cd, func) \
  if (uMsg == OCM_NOTIFY && cd == ((LPNMHDR)lParam)->code &&                 \
      ((LPNMHDR)lParam)->idFrom >= idFirst &&                                \
      ((LPNMHDR)lParam)->idFrom <= idLast) {                                 \
    SetMsgHandled(TRUE);                                                     \
    lResult = func((LPNMHDR)lParam);                                         \
    if (!ref.get() || msg_handled_)                                          \
      return TRUE;                                                           \
  }

#define CR_DEFLATE_RECT(rect, by)   \
  {                                 \
    (rect)->left += (by)->left;     \
    (rect)->top += (by)->top;       \
    (rect)->right -= (by)->right;   \
    (rect)->bottom -= (by)->bottom; \
  }

#define CR_POINT_INITIALIZER_FROM_LPARAM(lparam) \
  { LOWORD(lparam), HIWORD(lparam) }

#endif  // UI_GFX_WIN_MSG_UTIL_H_
