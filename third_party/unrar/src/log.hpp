#ifndef _RAR_LOG_
#define _RAR_LOG_

void InitLogOptions(const std::wstring &LogFileName,RAR_CHARSET CSet);
void CloseLogOptions();

#ifdef SILENT
inline void Log(const wchar *ArcName,const wchar *fmt,...) {}
#else
void Log(const wchar *ArcName,const wchar *fmt,...);
#endif

#endif
