#include "rar.hpp"

const char *NullToEmpty(const char *Str)
{
  return Str==nullptr ? "":Str;
}


const wchar *NullToEmpty(const wchar *Str)
{
  return Str==nullptr ? L"":Str;
}


void IntToExt(const std::string &Src,std::string &Dest)
{
#ifdef _WIN_ALL
  if (std::addressof(Src)!=std::addressof(Dest))
    Dest=Src;
  // OemToCharA use seems to be discouraged. So we use OemToCharBuffA,
  // which doesn't stop at 0 and converts the entire passed length.
  OemToCharBuffA(&Dest[0],&Dest[0],(DWORD)Dest.size());

  std::string::size_type Pos=Dest.find('\0'); // Avoid zeroes inside of Dest.
  if (Pos!=std::string::npos)
    Dest.erase(Pos);
  
#else
  if (std::addressof(Src)!=std::addressof(Dest))
    Dest=Src;
#endif
}


// Convert archived names and comments to Unicode.
// Allows user to select a code page in GUI.
void ArcCharToWide(const char *Src,std::wstring &Dest,ACTW_ENCODING Encoding)
{
#if defined(_WIN_ALL) // Console Windows RAR.
  if (Encoding==ACTW_UTF8)
    UtfToWide(Src,Dest);
  else
  {
#if defined(CHROMIUM_UNRAR)
    if (Encoding == ACTW_OEM) {
      // OemToCharBuffA, called by IntToExt, is implemented by user32.dll which
      // is not available in win32k lockdown sandbox. We can map from the OEM
      // codepage using CP_OEMCP and MultiByteToWideChar from kernel32.dll
      // instead, as we're also attempting to map to wide chars.
      const size_t SrcLength = strlen(Src) + 1;
      const int size =
          MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED | MB_USEGLYPHCHARS, Src,
                              SrcLength, nullptr, 0);
      if (size <= 0) {
        Dest.clear();
      } else {
        Dest.resize(size);
        (void)::MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED | MB_USEGLYPHCHARS,
                                    Src, SrcLength, &Dest[0], size);
      }
    } else {
      CharToWide(Src, Dest);
    }
#else
    std::string NameA;
    if (Encoding==ACTW_OEM)
    {
      IntToExt(Src,NameA);
      Src=NameA.data();
    }
    CharToWide(Src,Dest);
#endif  // defined(CHROMIUM_UNRAR)
  }
#else // RAR for Unix.
  if (Encoding==ACTW_UTF8)
    UtfToWide(Src,Dest);
  else
    CharToWide(Src,Dest);
#endif
  TruncateAtZero(Dest); // Ensure there are no zeroes inside of string.
}






int stricomp(const char *s1,const char *s2)
{
#ifdef _WIN_ALL
  return CompareStringA(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,-1,s2,-1)-2;
#else
  while (toupper(*s1)==toupper(*s2))
  {
    if (*s1==0)
      return 0;
    s1++;
    s2++;
  }
  return s1 < s2 ? -1 : 1;
#endif
}


int strnicomp(const char *s1,const char *s2,size_t n)
{
#ifdef _WIN_ALL
  // If we specify 'n' exceeding the actual string length, CompareString goes
  // beyond the trailing zero and compares garbage. So we need to limit 'n'
  // to real string length.
  // It is important to use strnlen (or memchr(...,0)) instead of strlen,
  // because data can be not zero terminated.
  size_t l1=Min(strnlen(s1,n),n);
  size_t l2=Min(strnlen(s2,n),n);
  return CompareStringA(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,(int)l1,s2,(int)l2)-2;
#else
  if (n==0)
    return 0;
  while (toupper(*s1)==toupper(*s2))
  {
    if (*s1==0 || --n==0)
      return 0;
    s1++;
    s2++;
  }
  return s1 < s2 ? -1 : 1;
#endif
}


wchar* RemoveEOL(wchar *Str)
{
  for (int I=(int)wcslen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n' || Str[I]==' ' || Str[I]=='\t');I--)
    Str[I]=0;
  return Str;
}


void RemoveEOL(std::wstring &Str)
{
  while (!Str.empty())
  {
    wchar c=Str.back();
    if (c=='\r' || c=='\n' || c==' ' || c=='\t')
      Str.pop_back();
    else
      break;
  }
}


wchar* RemoveLF(wchar *Str)
{
  for (int I=(int)wcslen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n');I--)
    Str[I]=0;
  return Str;
}


void RemoveLF(std::wstring &Str)
{
  for (int I=(int)Str.size()-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n');I--)
    Str.erase(I);
}


#if defined(SFX_MODULE)
// char version of etoupperw. Used in console SFX module only.
// Fast toupper for English only input and output. Additionally to speed,
// it also avoids Turkish small i to big I with dot conversion problem.
// We do not define 'c' as 'int' to avoid necessity to cast all
// signed chars passed to this function to unsigned char.
unsigned char etoupper(unsigned char c)
{
  return c>='a' && c<='z' ? c-'a'+'A' : c;
}
#endif


// Fast toupper for English only input and output. Additionally to speed,
// it also avoids Turkish small i to big I with dot conversion problem.
// We do not define 'c' as 'int' to avoid necessity to cast all
// signed wchars passed to this function to unsigned char.
wchar etoupperw(wchar c)
{
  return c>='a' && c<='z' ? c-'a'+'A' : c;
}


// We do not want to cast every signed char to unsigned when passing to
// isdigit, so we implement the replacement. Shall work for Unicode too.
// If chars are signed, conversion from char to int could generate negative
// values, resulting in undefined behavior in standard isdigit.
bool IsDigit(int ch)
{
  return ch>='0' && ch<='9';
}


// We do not want to cast every signed char to unsigned when passing to
// isspace, so we implement the replacement. Shall work for Unicode too.
// If chars are signed, conversion from char to int could generate negative
// values, resulting in undefined behavior in standard isspace.
bool IsSpace(int ch)
{
  return ch==' ' || ch=='\t';
}


// We do not want to cast every signed char to unsigned when passing to
// isalpha, so we implement the replacement. Shall work for Unicode too.
// If chars are signed, conversion from char to int could generate negative
// values, resulting in undefined behavior in standard function.
bool IsAlpha(int ch)
{
  return ch>='A' && ch<='Z' || ch>='a' && ch<='z';
}




void BinToHex(const byte *Bin,size_t BinSize,std::wstring &Hex)
{
  Hex.clear();
  for (uint I=0;I<BinSize;I++)
  {
    uint High=Bin[I] >> 4;
    uint Low=Bin[I] & 0xf;
    uint HighHex=High>9 ? 'a'+High-10 : '0'+High;
    uint LowHex=Low>9 ? 'a'+Low-10 : '0'+Low;
    Hex+=HighHex;
    Hex+=LowHex;
  }
}


#ifndef SFX_MODULE
uint GetDigits(uint Number)
{
  uint Digits=1;
  while (Number>=10)
  {
    Number/=10;
    Digits++;
  }
  return Digits;
}
#endif


bool LowAscii(const std::string &Str)
{
  for (char Ch : Str)
  {
    // We convert char to byte in case char is signed.
    if (/*(uint)Ch<32 || */(byte)Ch>127)
      return false;
  }
  return true;
}


bool LowAscii(const std::wstring &Str)
{
  for (wchar Ch : Str)
  {
    // We convert wchar_t to uint just in case if some compiler
    // uses signed wchar_t.
    if (/*(uint)Ch<32 || */(uint)Ch>127)
      return false;
  }
  return true;
}


int wcsicompc(const wchar *s1,const wchar *s2) // For path comparison.
{
#if defined(_UNIX)
  return wcscmp(s1,s2);
#else
  return wcsicomp(s1,s2);
#endif
}


int wcsicompc(const std::wstring &s1,const std::wstring &s2)
{
  return wcsicompc(s1.c_str(),s2.c_str());
}


int wcsnicompc(const wchar *s1,const wchar *s2,size_t n)
{
#if defined(_UNIX)
  return wcsncmp(s1,s2,n);
#else
  return wcsnicomp(s1,s2,n);
#endif
}


int wcsnicompc(const std::wstring &s1,const std::wstring &s2,size_t n)
{
  return wcsnicompc(s1.c_str(),s2.c_str(),n);
}


// Safe copy: copies maxlen-1 max and for maxlen>0 returns zero terminated dest.
void strncpyz(char *dest, const char *src, size_t maxlen)
{
  if (maxlen>0)
  {
    while (--maxlen>0 && *src!=0)
      *dest++=*src++;
    *dest=0;
  }
}


// Safe copy: copies maxlen-1 max and for maxlen>0 returns zero terminated dest.
void wcsncpyz(wchar *dest, const wchar *src, size_t maxlen)
{
  if (maxlen>0)
  {
    while (--maxlen>0 && *src!=0)
      *dest++=*src++;
    *dest=0;
  }
}


// Safe append: resulting dest length cannot exceed maxlen and dest 
// is always zero terminated. 'maxlen' parameter defines the entire
// dest buffer size and is not compatible with wcsncat.
void strncatz(char* dest, const char* src, size_t maxlen)
{
  size_t length = strlen(dest);
  if (maxlen > length)
    strncpyz(dest + length, src, maxlen - length);
}


// Safe append: resulting dest length cannot exceed maxlen and dest 
// is always zero terminated. 'maxlen' parameter defines the entire
// dest buffer size and is not compatible with wcsncat.
void wcsncatz(wchar* dest, const wchar* src, size_t maxlen)
{
  size_t length = wcslen(dest);
  if (maxlen > length)
    wcsncpyz(dest + length, src, maxlen - length);
}


void itoa(int64 n,char *Str,size_t MaxSize)
{
  char NumStr[50];
  size_t Pos=0;

  int Neg=n < 0 ? 1 : 0;
  if (Neg)
    n=-n;

  do
  {
    if (Pos+1>=MaxSize-Neg)
      break;
    NumStr[Pos++]=char(n%10)+'0';
    n=n/10;
  } while (n!=0);

  if (Neg)
    NumStr[Pos++]='-';

  for (size_t I=0;I<Pos;I++)
    Str[I]=NumStr[Pos-I-1];
  Str[Pos]=0;
}


void itoa(int64 n,wchar *Str,size_t MaxSize)
{
  wchar NumStr[50];
  size_t Pos=0;

  int Neg=n < 0 ? 1 : 0;
  if (Neg)
    n=-n;

  do
  {
    if (Pos+1>=MaxSize-Neg)
      break;
    NumStr[Pos++]=wchar(n%10)+'0';
    n=n/10;
  } while (n!=0);

  if (Neg)
    NumStr[Pos++]='-';

  for (size_t I=0;I<Pos;I++)
    Str[I]=NumStr[Pos-I-1];
  Str[Pos]=0;
}


// Convert the number to string using thousand separators.
void fmtitoa(int64 n,wchar *Str,size_t MaxSize)
{
  static wchar ThSep=0; // Thousands separator.
#ifdef _WIN_ALL
  wchar Info[10];
  if ((!ThSep)!=0 && GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_STHOUSAND,Info,ASIZE(Info))>0)
    ThSep=*Info;
#elif defined(_UNIX)
  ThSep=*localeconv()->thousands_sep;
#endif
  if (ThSep==0) // If failed to detect the actual separator value.
    ThSep=' ';
  wchar RawText[30]; // 20 characters are enough for largest unsigned 64 bit int.
  itoa(n,RawText,ASIZE(RawText));
  uint S=0,D=0,L=wcslen(RawText)%3;
  while (RawText[S]!=0 && D+1<MaxSize)
  {
    if (S!=0 && (S+3-L)%3==0)
      Str[D++]=ThSep;
    Str[D++]=RawText[S++];
  }
  Str[D]=0;
}


std::wstring GetWide(const char *Src)
{
  std::wstring Str;
  CharToWide(Src,Str);
  return Str;
}


// Parse string containing parameters separated with spaces.
// Support quote marks. Accepts and updates the current position in the string.
// Returns false if there is nothing to parse.
bool GetCmdParam(const std::wstring &CmdLine,std::wstring::size_type &Pos,std::wstring &Param)
{
  Param.clear();

  while (IsSpace(CmdLine[Pos]))
    Pos++;
  if (Pos==CmdLine.size())
    return false;

  bool Quote=false;
  while (Pos<CmdLine.size() && (Quote || !IsSpace(CmdLine[Pos])))
  {
    if (CmdLine[Pos]=='\"')
    {
      if (CmdLine[Pos+1]=='\"')
      {
        // Insert the quote character instead of two adjoining quote characters.
        Param+='\"';
        Pos++;
      }
      else
        Quote=!Quote;
    }
    else
      Param+=CmdLine[Pos];
    Pos++;
  }
  return true;
}




#ifndef RARDLL
// For compatibility with existing translations we use %s to print Unicode
// strings in format strings and convert them to %ls here. %s could work
// without such conversion in Windows, but not in Unix wprintf.
void PrintfPrepareFmt(const wchar *Org,std::wstring &Cvt)
{
  size_t Src=0;
  while (Org[Src]!=0)
  {
    if (Org[Src]=='%' && (Src==0 || Org[Src-1]!='%'))
    {
      size_t SPos=Src+1;
      // Skipping a possible width specifier like %-50s.
      while (IsDigit(Org[SPos]) || Org[SPos]=='-')
        SPos++;
      if (Org[SPos]=='s')
      {
        while (Src<SPos)
          Cvt.push_back(Org[Src++]);
        Cvt.push_back('l');
      }
    }
#ifdef _WIN_ALL
    // Convert \n to \r\n in Windows. Important when writing to log,
    // so other tools like Notebook can view resulting log properly.
    if (Org[Src]=='\n' && (Src==0 || Org[Src-1]!='\r'))
      Cvt.push_back('\r');
#endif

    Cvt.push_back(Org[Src++]);
  }
}


// Print output to std::wstring.
std::wstring wstrprintf(const wchar *fmt,...)
{
  va_list arglist;
  va_start(arglist,fmt);
  std::wstring s=vwstrprintf(fmt,arglist);
  va_end(arglist);
  return s;
}


std::wstring vwstrprintf(const wchar *fmt,va_list arglist)
{
  std::wstring fmtw;
  PrintfPrepareFmt(fmt,fmtw);

  // We also tried to use _vscwprintf in MSVC to calculate the required buffer
  // size and allocate the exactly such size, but it seemed to be a little
  // slower than approach below.
  
  const size_t MaxAllocSize=0x10000; // Prevent the excessive allocation.

  // vswprintf returns only the error status without required buffer size,
  // so we try different buffer sizes. Start from reasonably small size
  // to reduce the zero initialization cost, but still large enough to fit
  // most of strings and avoid additional loop iterations.
  std::wstring Msg(256,L'\0');
  while (true)
  {
    va_list argscopy;
    va_copy(argscopy, arglist);
    int r=vswprintf(&Msg[0],Msg.size(),fmtw.c_str(),argscopy);
    va_end(argscopy);
    if (r>=0 || Msg.size()>MaxAllocSize)
      break;
    Msg.resize(Msg.size()*4);
  }
  std::wstring::size_type ZeroPos=Msg.find(L'\0');
  if (ZeroPos!=std::wstring::npos)
    Msg.resize(ZeroPos); // Remove excessive zeroes at the end.
  
  return Msg;
}
#endif


#ifdef _WIN_ALL
bool ExpandEnvironmentStr(std::wstring &Str)
{
  DWORD ExpCode=ExpandEnvironmentStrings(Str.c_str(),nullptr,0);
  if (ExpCode==0)
    return false;
  std::vector<wchar> Buf(ExpCode);
  ExpCode=ExpandEnvironmentStrings(Str.c_str(),Buf.data(),(DWORD)Buf.size());
  if (ExpCode==0 || ExpCode>Buf.size())
    return false;
  Str=Buf.data();
  return true;
}
#endif


void TruncateAtZero(std::wstring &Str)
{
  std::wstring::size_type Pos=Str.find(L'\0');
  if (Pos!=std::wstring::npos)
    Str.erase(Pos);
}


void ReplaceEsc(std::wstring &Str)
{
  std::wstring::size_type Pos=0;
  while (true)
  {
    Pos=Str.find(L'\033',Pos);
    if (Pos==std::wstring::npos)
      break;
    Str[Pos]=L'\'';
    Str.insert(Pos+1,L"\\033'");
    Pos+=6;
  }
}
