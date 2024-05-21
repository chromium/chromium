#include "rar.hpp"



void InitLogOptions(const std::wstring &LogFileName,RAR_CHARSET CSet)
{
}


void CloseLogOptions()
{
}


#ifndef SILENT
void Log(const wchar *ArcName,const wchar *fmt,...)
{
  // Preserve the error code for possible following system error message.
  int Code=ErrHandler.GetSystemErrorCode();

  uiAlarm(UIALARM_ERROR);

  va_list arglist;
  va_start(arglist,fmt);

  std::wstring s=vwstrprintf(fmt,arglist);

  ReplaceEsc(s);
  
  va_end(arglist);
  eprintf(L"%ls",s.c_str());
  ErrHandler.SetSystemErrorCode(Code);
}
#endif


