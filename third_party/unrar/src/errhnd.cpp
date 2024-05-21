// NOTE(vakh): The process.h file needs to be included first because "rar.hpp"
// defines certain macros that cause symbol redefinition errors
#if defined(UNRAR_NO_EXCEPTIONS)
#include "base/check.h"
#include "base/process/process.h"
#endif  // defined(UNRAR_NO_EXCEPTIONS)
#include "rar.hpp"

#include <ostream>

void ErrorHandler::Clean()
{
  ExitCode=RARX_SUCCESS;
  ErrCount=0;
  EnableBreak=true;
  Silent=false;
  UserBreak=false;
  MainExit=false;
  DisableShutdown=false;
  ReadErrIgnoreAll=false;
}


void ErrorHandler::MemoryError()
{
  MemoryErrorMsg();
  Exit(RARX_MEMORY);
}


void ErrorHandler::OpenError(const std::wstring &FileName)
{
#ifndef SILENT
  OpenErrorMsg(FileName);
  Exit(RARX_OPEN);
#endif
}


void ErrorHandler::CloseError(const std::wstring &FileName)
{
  if (!UserBreak)
  {
    uiMsg(UIERROR_FILECLOSE,FileName);
    SysErrMsg();
  }
  // We must not call Exit and throw an exception here, because this function
  // is called from File object destructor and can be invoked when stack
  // unwinding while handling another exception. Throwing a new exception
  // when stack unwinding is prohibited and terminates a program.
  // If necessary, we can check std::uncaught_exception() before throw.
  SetErrorCode(RARX_FATAL);
}


void ErrorHandler::ReadError(const std::wstring &FileName)
{
#ifndef SILENT
  ReadErrorMsg(FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_READ);
#endif
}


void ErrorHandler::AskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &Retry,bool &Quit)
{
  SetErrorCode(RARX_READ);
#if !defined(SILENT) && !defined(SFX_MODULE)
  if (!Silent)
  {
    uiMsg(UIERROR_FILEREAD,L"",FileName);
    SysErrMsg();
    if (ReadErrIgnoreAll)
      Ignore=true;
    else
    {
      bool All=false;
      uiAskRepeatRead(FileName,Ignore,All,Retry,Quit);
      if (All)
        ReadErrIgnoreAll=Ignore=true;
      if (Quit) // Disable shutdown if user select Quit in read error prompt.
        DisableShutdown=true;
    }
    return;
  }
#endif
  Ignore=true; // Saving the file part for -y or -inul or "Ignore all" choice.
}


void ErrorHandler::WriteError(const std::wstring &ArcName,const std::wstring &FileName)
{
#ifndef SILENT
  WriteErrorMsg(ArcName,FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_WRITE);
#endif
}


#ifdef _WIN_ALL
void ErrorHandler::WriteErrorFAT(const std::wstring &FileName)
{
  SysErrMsg();
  uiMsg(UIERROR_NTFSREQUIRED,FileName);
#if !defined(SILENT) && !defined(SFX_MODULE) || defined(RARDLL)
  Exit(RARX_WRITE);
#endif
}
#endif


bool ErrorHandler::AskRepeatWrite(const std::wstring &FileName,bool DiskFull)
{
#ifndef SILENT
  if (!Silent)
  {
    // We do not display "repeat write" prompt in Android, so we do not
    // need the matching system error message.
    SysErrMsg();
    bool Repeat=uiAskRepeatWrite(FileName,DiskFull);
    if (!Repeat) // Disable shutdown if user pressed Cancel in error dialog.
      DisableShutdown=true;
    return Repeat;
  }
#endif
  return false;
}


void ErrorHandler::SeekError(const std::wstring &FileName)
{
  if (!UserBreak)
  {
    uiMsg(UIERROR_FILESEEK,FileName);
    SysErrMsg();
  }
#if !defined(SILENT) || defined(RARDLL)
  Exit(RARX_FATAL);
#endif
}


void ErrorHandler::GeneralErrMsg(const wchar *fmt,...)
{
#ifndef RARDLL
  va_list arglist;
  va_start(arglist,fmt);

  std::wstring Msg=vwstrprintf(fmt,arglist);
  uiMsg(UIERROR_GENERALERRMSG,Msg);
  SysErrMsg();

  va_end(arglist);
#endif
}


void ErrorHandler::MemoryErrorMsg()
{
  uiMsg(UIERROR_MEMORY);
  SetErrorCode(RARX_MEMORY);
}


void ErrorHandler::OpenErrorMsg(const std::wstring &FileName)
{
  OpenErrorMsg(L"",FileName);
}


void ErrorHandler::OpenErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEOPEN,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_OPEN);

  // Keep GUI responsive if many files cannot be opened when archiving.
  // Call after SysErrMsg to avoid modifying the error code and SysErrMsg text.
  Wait();
}


void ErrorHandler::CreateErrorMsg(const std::wstring &FileName)
{
  CreateErrorMsg(L"",FileName);
}


void ErrorHandler::CreateErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILECREATE,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_CREATE);
}


void ErrorHandler::ReadErrorMsg(const std::wstring &FileName)
{
  ReadErrorMsg(L"",FileName);
}


void ErrorHandler::ReadErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEREAD,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_READ);
}


void ErrorHandler::WriteErrorMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_FILEWRITE,ArcName,FileName);
  SysErrMsg();
  SetErrorCode(RARX_WRITE);
}


void ErrorHandler::ArcBrokenMsg(const std::wstring &ArcName)
{
  uiMsg(UIERROR_ARCBROKEN,ArcName);
  SetErrorCode(RARX_CRC);
}


void ErrorHandler::ChecksumFailedMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_CHECKSUM,ArcName,FileName);
  SetErrorCode(RARX_CRC);
}


void ErrorHandler::UnknownMethodMsg(const std::wstring &ArcName,const std::wstring &FileName)
{
  uiMsg(UIERROR_UNKNOWNMETHOD,ArcName,FileName);
  ErrHandler.SetErrorCode(RARX_FATAL);
}


void ErrorHandler::Exit(RAR_EXIT ExitCode)
{
  uiAlarm(UIALARM_ERROR);
  Throw(ExitCode);
}


void ErrorHandler::SetErrorCode(RAR_EXIT Code)
{
  switch(Code)
  {
    case RARX_WARNING:
    case RARX_USERBREAK:
      if (ExitCode==RARX_SUCCESS)
        ExitCode=Code;
      break;
    case RARX_CRC:
      if (ExitCode!=RARX_BADPWD)
        ExitCode=Code;
      break;
    case RARX_FATAL:
      if (ExitCode==RARX_SUCCESS || ExitCode==RARX_WARNING)
        ExitCode=RARX_FATAL;
      break;
    default:
      ExitCode=Code;
      break;
  }
  ErrCount++;
}


#ifdef _WIN_ALL
BOOL __stdcall ProcessSignal(DWORD SigType)
#else
#if defined(__sun)
extern "C"
#endif
void _stdfunction ProcessSignal(int SigType)
#endif
{
#ifdef _WIN_ALL
  // When a console application is run as a service, this allows the service
  // to continue running after the user logs off. 
  if (SigType==CTRL_LOGOFF_EVENT)
    return TRUE;
#endif

  ErrHandler.UserBreak=true;
  ErrHandler.SetDisableShutdown();
  mprintf(St(MBreak));

#ifdef _WIN_ALL
  // Let the main thread to handle 'throw' and destroy file objects.
  for (uint I=0;!ErrHandler.MainExit && I<50;I++)
    Sleep(100);
#if defined(USE_RC) && !defined(SFX_MODULE) && !defined(RARDLL)
  ExtRes.UnloadDLL();
#endif
  exit(RARX_USERBREAK);
#endif

#ifdef _UNIX
  static uint BreakCount=0;
  // User continues to press Ctrl+C, exit immediately without cleanup.
  if (++BreakCount>1)
    exit(RARX_USERBREAK);
  // Otherwise return from signal handler and let Wait() function to close
  // files and quit. We cannot use the same approach as in Windows,
  // because Unix signal handler can block execution of our main code.
#endif

#if defined(_WIN_ALL) && !defined(_MSC_VER)
  // Never reached, just to avoid a compiler warning
  return TRUE;
#endif
}


void ErrorHandler::SetSignalHandlers(bool Enable)
{
  EnableBreak=Enable;
#ifdef _WIN_ALL
  SetConsoleCtrlHandler(Enable ? ProcessSignal:NULL,TRUE);
#else
  signal(SIGINT,Enable ? ProcessSignal:SIG_IGN);
  signal(SIGTERM,Enable ? ProcessSignal:SIG_IGN);
#endif
}


void ErrorHandler::Throw(RAR_EXIT Code)
{
  if (Code==RARX_USERBREAK && !EnableBreak)
    return;
#if !defined(SILENT)
  if (Code!=RARX_SUCCESS)
    if (Code==RARX_USERERROR) // Do not write "aborted" when just displaying the online help.
      mprintf(L"\n"); // For consistency with other errors, which print the final "\n".
    else
      mprintf(L"\n%s\n",St(MProgAborted));
#endif
  SetErrorCode(Code);
#if defined(UNRAR_NO_EXCEPTIONS)
  CHECK(false) << "Failed with RAR_EXIT code: " << Code;
#else
  throw Code;
#endif  // defined(UNRAR_NO_EXCEPTIONS)
}


bool ErrorHandler::GetSysErrMsg(std::wstring &Msg)
{
#ifndef SILENT
#ifdef _WIN_ALL
  int ErrType=GetLastError();
  if (ErrType!=0)
  {
    wchar *Buf=nullptr;
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|
          FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_ALLOCATE_BUFFER,
          NULL,ErrType,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
          (LPTSTR)&Buf,0,NULL)!=0)
    {
      Msg=Buf;
      LocalFree(Buf);
      return true;
    }
  }
#endif

#ifdef _UNIX
  if (errno!=0)
  {
    char *err=strerror(errno);
    if (err!=NULL)
    {
      CharToWide(err,Msg);
      return true;
    }
  }
#endif
#endif
  return false;
}


void ErrorHandler::SysErrMsg()
{
#ifndef SILENT
  std::wstring Msg;
  if (!GetSysErrMsg(Msg))
    return;
#ifdef _WIN_ALL
  // Print string with \r\n as several strings to multiple lines.
  size_t Pos=0;
  while (Pos!=std::wstring::npos)
  {
    while (Msg[Pos]=='\r' || Msg[Pos]=='\n')
      Pos++;
    if (Pos==Msg.size())
      break;
    size_t EndPos=Msg.find_first_of(L"\r\n",Pos);
    std::wstring CurMsg=Msg.substr(Pos,EndPos==std::wstring::npos ? EndPos:EndPos-Pos);
    uiMsg(UIERROR_SYSERRMSG,CurMsg);
    Pos=EndPos;
  }
#endif

#ifdef _UNIX
  uiMsg(UIERROR_SYSERRMSG,Msg);
#endif

#endif
}


int ErrorHandler::GetSystemErrorCode()
{
#ifdef _WIN_ALL
  return GetLastError();
#else
  return errno;
#endif
}


void ErrorHandler::SetSystemErrorCode(int Code)
{
#ifdef _WIN_ALL
  SetLastError(Code);
#else
  errno=Code;
#endif
}
