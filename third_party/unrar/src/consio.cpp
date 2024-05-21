#include "rar.hpp"
#include "log.cpp"

static MESSAGE_TYPE MsgStream=MSG_STDOUT;
static RAR_CHARSET RedirectCharset=RCH_DEFAULT;
static bool ProhibitInput=false;

static bool StdoutRedirected=false,StderrRedirected=false,StdinRedirected=false;

#ifdef _WIN_ALL
static bool IsRedirected(DWORD nStdHandle)
{
  HANDLE hStd=GetStdHandle(nStdHandle);
  DWORD Mode;
  return GetFileType(hStd)!=FILE_TYPE_CHAR || GetConsoleMode(hStd,&Mode)==0;
}
#endif


void InitConsole()
{
#ifdef _WIN_ALL
  // We want messages like file names or progress percent to be printed
  // immediately. Use only in Windows, in Unix they can cause wprintf %ls
  // to fail with non-English strings.
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Detect if output is redirected and set output mode properly.
  // We do not want to send Unicode output to files and especially to pipes
  // like '|more', which cannot handle them correctly in Windows.
  // In Unix console output is UTF-8 and it is handled correctly
  // when redirecting, so no need to perform any adjustments.
  StdoutRedirected=IsRedirected(STD_OUTPUT_HANDLE);
  StderrRedirected=IsRedirected(STD_ERROR_HANDLE);
  StdinRedirected=IsRedirected(STD_INPUT_HANDLE);
#ifdef _MSC_VER
  if (!StdoutRedirected)
    _setmode(_fileno(stdout), _O_U16TEXT);
  if (!StderrRedirected)
    _setmode(_fileno(stderr), _O_U16TEXT);
#endif
#elif defined(_UNIX)
  StdoutRedirected=!isatty(fileno(stdout));
  StderrRedirected=!isatty(fileno(stderr));
  StdinRedirected=!isatty(fileno(stdin));
#endif
}


void SetConsoleMsgStream(MESSAGE_TYPE MsgStream)
{
  ::MsgStream=MsgStream;
}


void SetConsoleRedirectCharset(RAR_CHARSET RedirectCharset)
{
  ::RedirectCharset=RedirectCharset;
}


void ProhibitConsoleInput()
{
  ProhibitInput=true;
}


#ifndef SILENT
static void cvt_wprintf(FILE *dest,const wchar *fmt,va_list arglist)
{
  // No need for PrintfPrepareFmt here, vwstrprintf calls it.
  std::wstring s=vwstrprintf(fmt,arglist);

  ReplaceEsc(s);

#ifdef _WIN_ALL
  if (dest==stdout && StdoutRedirected || dest==stderr && StderrRedirected)
  {
    HANDLE hOut=GetStdHandle(dest==stdout ? STD_OUTPUT_HANDLE:STD_ERROR_HANDLE);
    DWORD Written;
    if (RedirectCharset==RCH_UNICODE)
      WriteFile(hOut,s.data(),(DWORD)s.size()*sizeof(s[0]),&Written,NULL);
    else
    {
      // Avoid Unicode for redirect in Windows, it does not work with pipes.
      std::string MsgA;
      if (RedirectCharset==RCH_UTF8)
        WideToUtf(s,MsgA);
      else
        WideToChar(s,MsgA);
      if (RedirectCharset==RCH_DEFAULT || RedirectCharset==RCH_OEM)
        CharToOemA(&MsgA[0],&MsgA[0]); // Console tools like 'more' expect OEM encoding.

      // We already converted \n to \r\n above, so we use WriteFile instead
      // of C library to avoid unnecessary additional conversion.
      WriteFile(hOut,MsgA.data(),(DWORD)MsgA.size(),&Written,NULL);
    }
    return;
  }
  // MSVC2008 vfwprintf writes every character to console separately
  // and it is too slow. We use direct WriteConsole call instead.
  HANDLE hOut=GetStdHandle(dest==stderr ? STD_ERROR_HANDLE:STD_OUTPUT_HANDLE);
  DWORD Written;
  WriteConsole(hOut,s.data(),(DWORD)s.size(),&Written,NULL);
#else
  fputws(s.c_str(),dest);
  // We do not use setbuf(NULL) in Unix (see comments in InitConsole).
  fflush(dest);
#endif
}


void mprintf(const wchar *fmt,...)
{
  if (MsgStream==MSG_NULL || MsgStream==MSG_ERRONLY)
    return;

  fflush(stderr); // Ensure proper message order.

  va_list arglist;
  va_start(arglist,fmt);
  FILE *dest=MsgStream==MSG_STDERR ? stderr:stdout;
  cvt_wprintf(dest,fmt,arglist);
  va_end(arglist);
}
#endif


#ifndef SILENT
void eprintf(const wchar *fmt,...)
{
  if (MsgStream==MSG_NULL)
    return;

  fflush(stdout); // Ensure proper message order.

  va_list arglist;
  va_start(arglist,fmt);
  cvt_wprintf(stderr,fmt,arglist);
  va_end(arglist);
}
#endif


#ifndef SILENT
static void QuitIfInputProhibited()
{
  // We cannot handle user prompts if -si is used to read file or archive data
  // from stdin.
  if (ProhibitInput)
  {
    mprintf(St(MStdinNoInput));
    ErrHandler.Exit(RARX_FATAL);
  }
}


static void GetPasswordText(std::wstring &Str)
{
  QuitIfInputProhibited();
  if (StdinRedirected)
    getwstr(Str); // Read from pipe or redirected file.
  else
  {
#ifdef _WIN_ALL
    HANDLE hConIn=GetStdHandle(STD_INPUT_HANDLE);
    DWORD ConInMode;
    GetConsoleMode(hConIn,&ConInMode);
    SetConsoleMode(hConIn,ENABLE_LINE_INPUT); // Remove ENABLE_ECHO_INPUT.

    std::vector<wchar> Buf(MAXPASSWORD);
    
    // We prefer ReadConsole to ReadFile, so we can read Unicode input.
    DWORD Read=0;
    ReadConsole(hConIn,Buf.data(),(DWORD)Buf.size()-1,&Read,NULL);
    Buf[Read]=0;
    Str=Buf.data();
    cleandata(Buf.data(),Buf.size()*sizeof(Buf[0]));

    SetConsoleMode(hConIn,ConInMode);

    // 2023.03.12: Previously we checked for presence of "\n" in entered
    // passwords, supposing that truncated strings do not include it.
    // We did it to read the rest of excessively long string, so it is not
    // read later as the second password for -p switch. But this "\n" check
    // doesn't seem to work in Windows 10 anymore and "\r" is present even
    // in truncated strings. Also we increased MAXPASSWORD, so it is larger
    // than MAXPASSWORD_RAR. Thus we removed this check as not working
    // and not that necessary. Low level FlushConsoleInputBuffer doesn't help
    // for high level ReadConsole, which in line input mode seems to store
    // the rest of string in its own internal buffer.
#else
    std::vector<char> StrA(MAXPASSWORD*4); // "*4" for multibyte UTF-8 characters.
#ifdef __VMS
    fgets(StrA.data(),StrA.size()-1,stdin);
#elif defined(__sun)
    strncpyz(StrA.data(),getpassphrase(""),StrA.size());
#else
    strncpyz(StrA.data(),getpass(""),StrA.size());
#endif
    CharToWide(StrA.data(),Str);
    cleandata(StrA.data(),StrA.size()*sizeof(StrA[0]));
#endif
  }
  RemoveLF(Str);
}
#endif


#ifndef SILENT
bool GetConsolePassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,SecPassword *Password)
{
  if (!StdinRedirected)
    uiAlarm(UIALARM_QUESTION);
  
  while (true)
  {
//    if (!StdinRedirected)
      if (Type==UIPASSWORD_GLOBAL)
        eprintf(L"\n%s: ",St(MAskPsw));
      else
        eprintf(St(MAskPswFor),FileName.c_str());

    std::wstring PlainPsw;
    GetPasswordText(PlainPsw);
    if (PlainPsw.empty() && Type==UIPASSWORD_GLOBAL)
      return false;
    if (PlainPsw.size()>=MAXPASSWORD)
    {
      PlainPsw.erase(MAXPASSWORD-1);
      uiMsg(UIERROR_TRUNCPSW,MAXPASSWORD-1);
    }
    if (!StdinRedirected && Type==UIPASSWORD_GLOBAL)
    {
      eprintf(St(MReAskPsw));
      std::wstring CmpStr;
      GetPasswordText(CmpStr);
      if (CmpStr.empty() || PlainPsw!=CmpStr)
      {
        eprintf(St(MNotMatchPsw));
        cleandata(&PlainPsw[0],PlainPsw.size()*sizeof(PlainPsw[0]));
        cleandata(&CmpStr[0],CmpStr.size()*sizeof(CmpStr[0]));
        continue;
      }
      cleandata(&CmpStr[0],CmpStr.size()*sizeof(CmpStr[0]));
    }
    Password->Set(PlainPsw.c_str());
    cleandata(&PlainPsw[0],PlainPsw.size()*sizeof(PlainPsw[0]));
    break;
  }
  return true;
}
#endif


#ifndef SILENT
bool getwstr(std::wstring &str)
{
  // Print buffered prompt title function before waiting for input.
  fflush(stderr);

  QuitIfInputProhibited();

  str.clear();

  const size_t MaxRead=MAXPATHSIZE; // Large enough to read a file name.

#if defined(_WIN_ALL)
  // fgetws does not work well with non-English text in Windows,
  // so we do not use it.
  if (StdinRedirected) // ReadConsole does not work if redirected.
  {
    // fgets does not work well with pipes in Windows in our test.
    // Let's use files.
    std::vector<char> StrA(MaxRead*4);  // Up to 4 UTF-8 characters per wchar_t.
    File SrcFile;
    SrcFile.SetHandleType(FILE_HANDLESTD);
    SrcFile.SetLineInputMode(true);
    int ReadSize=SrcFile.Read(&StrA[0],StrA.size()-1);
    if (ReadSize<=0)
    {
      // Looks like stdin is a null device. We can enter to infinite loop
      // calling Ask(), so let's better exit.
      ErrHandler.Exit(RARX_USERBREAK);
    }
    StrA[ReadSize]=0;

    // We expect ANSI encoding here, but "echo text|rar ..." to pipe to RAR,
    // such as send passwords, we get OEM encoding by default, unless we
    // use "chcp" in console. But we avoid OEM to ANSI conversion,
    // because we also want to handle ANSI files redirection correctly,
    // like "rar ... < ansifile.txt".
    CharToWide(&StrA[0],str);
    cleandata(&StrA[0],StrA.size()); // We can use this function to enter passwords.
  }
  else
  {
    std::vector<wchar> Buf(MaxRead);  // Up to 4 UTF-8 characters per wchar_t.
    DWORD ReadSize=0;
    if (ReadConsole(GetStdHandle(STD_INPUT_HANDLE),&Buf[0],(DWORD)Buf.size()-1,&ReadSize,NULL)==0)
      return false;
    Buf[ReadSize]=0;
    str=Buf.data();
  }
#else
  std::vector<wchar> Buf(MaxRead);  // Up to 4 UTF-8 characters per wchar_t.
  if (fgetws(&Buf[0],Buf.size(),stdin)==NULL)
    ErrHandler.Exit(RARX_USERBREAK); // Avoid infinite Ask() loop.
  str=Buf.data();
#endif
  RemoveLF(str);
  return true;
}
#endif


#ifndef SILENT
// We allow this function to return 0 in case of invalid input,
// because it might be convenient to press Enter to some not dangerous
// prompts like "insert disk with next volume". We should call this function
// again in case of 0 in dangerous prompt such as overwriting file.
int Ask(const wchar *AskStr)
{
  uiAlarm(UIALARM_QUESTION);

  const int MaxItems=10;
  wchar Item[MaxItems][40];
  int ItemKeyPos[MaxItems],NumItems=0;

  for (const wchar *NextItem=AskStr;NextItem!=NULL;NextItem=wcschr(NextItem+1,'_'))
  {
    wchar *CurItem=Item[NumItems];
    wcsncpyz(CurItem,NextItem+1,ASIZE(Item[0]));
    wchar *EndItem=wcschr(CurItem,'_');
    if (EndItem!=NULL)
      *EndItem=0;
    int KeyPos=0,CurKey;
    while ((CurKey=CurItem[KeyPos])!=0)
    {
      bool Found=false;
      for (int I=0;I<NumItems && !Found;I++)
        if (toupperw(Item[I][ItemKeyPos[I]])==toupperw(CurKey))
          Found=true;
      if (!Found && CurKey!=' ')
        break;
      KeyPos++;
    }
    ItemKeyPos[NumItems]=KeyPos;
    NumItems++;
  }

  for (int I=0;I<NumItems;I++)
  {
    eprintf(I==0 ? (NumItems>3 ? L"\n":L" "):L", ");
    int KeyPos=ItemKeyPos[I];
    for (int J=0;J<KeyPos;J++)
      eprintf(L"%c",Item[I][J]);
    eprintf(L"[%c]%ls",Item[I][KeyPos],&Item[I][KeyPos+1]);
  }
  eprintf(L" ");
  std::wstring Str;
  getwstr(Str);
  wchar Ch=toupperw(Str[0]);
  for (int I=0;I<NumItems;I++)
    if (Ch==Item[I][ItemKeyPos[I]])
      return I+1;
  return 0;
}
#endif


static bool IsCommentUnsafe(const std::wstring &Data)
{
  for (size_t I=0;I<Data.size();I++)
    if (Data[I]==27 && Data[I+1]=='[')
      for (size_t J=I+2;J<Data.size();J++)
      {
        // Return true for <ESC>[{key};"{string}"p used to redefine
        // a keyboard key on some terminals.
        if (Data[J]=='\"')
          return true;
        if (!IsDigit(Data[J]) && Data[J]!=';')
          break;
      }
  return false;
}


void OutComment(const std::wstring &Comment)
{
  if (IsCommentUnsafe(Comment))
    return;
  const size_t MaxOutSize=0x400;
  for (size_t I=0;I<Comment.size();I+=MaxOutSize)
  {
    size_t CopySize=Min(MaxOutSize,Comment.size()-I);
    mprintf(L"%s",Comment.substr(I,CopySize).c_str());
  }
  mprintf(L"\n");
}

