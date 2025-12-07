#ifndef _RAR_CONSIO_
#define _RAR_CONSIO_

void InitConsole();
void SetConsoleMsgStream(MESSAGE_TYPE MsgStream);
void SetConsoleRedirectCharset(RAR_CHARSET RedirectCharset);
void ProhibitConsoleInput();
void OutComment(const std::wstring &Comment);
bool IsConsoleOutputPresent();

#ifndef SILENT
bool GetConsolePassword(UIPASSWORD_TYPE Type,const std::wstring &FileName,SecPassword *Password);
#endif

#ifdef SILENT
  inline void mprintf(const wchar *fmt,...) {}
  inline void eprintf(const wchar *fmt,...) {}
  inline void Alarm() {}
  inline int Ask(const wchar *AskStr) {return 0;}
  inline void getwstr(std::wstring &str) {}
#else
  void mprintf(const wchar *fmt,...);
  void eprintf(const wchar *fmt,...);
  void Alarm();
  int Ask(const wchar *AskStr);
  void getwstr(std::wstring &str);
#endif

#endif
