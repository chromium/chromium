#ifndef _RAR_STRFN_
#define _RAR_STRFN_

const char* NullToEmpty(const char *Str);
const wchar* NullToEmpty(const wchar *Str);
void IntToExt(const std::string &Src,std::string &Dest);

enum ACTW_ENCODING { ACTW_DEFAULT, ACTW_OEM, ACTW_UTF8};
void ArcCharToWide(const char *Src,std::wstring &Dest,ACTW_ENCODING Encoding);


int stricomp(const char *s1,const char *s2);
int strnicomp(const char *s1,const char *s2,size_t n);
wchar* RemoveEOL(wchar *Str);
void RemoveEOL(std::wstring &Str);
wchar* RemoveLF(wchar *Str);
void RemoveLF(std::wstring &Str);

void strncpyz(char *dest, const char *src, size_t maxlen);
void wcsncpyz(wchar *dest, const wchar *src, size_t maxlen);
void strncatz(char* dest, const char* src, size_t maxlen);
void wcsncatz(wchar* dest, const wchar* src, size_t maxlen);

#if defined(SFX_MODULE)
unsigned char etoupper(unsigned char c);
#endif
wchar etoupperw(wchar c);

bool IsDigit(int ch);
bool IsSpace(int ch);
bool IsAlpha(int ch);

void BinToHex(const byte *Bin,size_t BinSize,std::wstring &Hex);

#ifndef SFX_MODULE
uint GetDigits(uint Number);
#endif

bool LowAscii(const std::string &Str);
bool LowAscii(const std::wstring &Str);

int wcsicompc(const wchar *s1,const wchar *s2);
int wcsicompc(const std::wstring &s1,const std::wstring &s2);
int wcsnicompc(const wchar *s1,const wchar *s2,size_t n);
int wcsnicompc(const std::wstring &s1,const std::wstring &s2,size_t n);

void itoa(int64 n,char *Str,size_t MaxSize);
void itoa(int64 n,wchar *Str,size_t MaxSize);
void fmtitoa(int64 n,wchar *Str,size_t MaxSize);
std::wstring GetWide(const char *Src);
bool GetCmdParam(const std::wstring &CmdLine,std::wstring::size_type &Pos,std::wstring &Param);
#ifndef RARDLL
void PrintfPrepareFmt(const wchar *Org,std::wstring &Cvt);
std::wstring wstrprintf(const wchar *fmt,...);
std::wstring vwstrprintf(const wchar *fmt,va_list arglist);
#endif

#ifdef _WIN_ALL
bool ExpandEnvironmentStr(std::wstring &Str);
#endif

void TruncateAtZero(std::wstring &Str);
void ReplaceEsc(std::wstring &Str);

#endif
