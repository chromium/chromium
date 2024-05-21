#ifndef _RAR_UNICODE_
#define _RAR_UNICODE_

#if defined( _WIN_ALL)
#define DBCS_SUPPORTED
#endif

bool WideToChar(const wchar *Src,char *Dest,size_t DestSize);
bool CharToWide(const char *Src,wchar *Dest,size_t DestSize);
bool WideToChar(const std::wstring &Src,std::string &Dest);
bool CharToWide(const std::string &Src,std::wstring &Dest);
byte* WideToRaw(const wchar *Src,size_t SrcSize,byte *Dest,size_t DestSize);
void WideToRaw(const std::wstring &Src,std::vector<byte> &Dest);
wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize);
std::wstring RawToWide(const std::vector<byte> &Src);
void WideToUtf(const wchar *Src,char *Dest,size_t DestSize);
void WideToUtf(const std::wstring &Src,std::string &Dest);
size_t WideToUtfSize(const wchar *Src);
bool UtfToWide(const char *Src,wchar *Dest,size_t DestSize);
bool UtfToWide(const char *Src,std::wstring &Dest);
//bool UtfToWide(const std::vector<char> &Src,std::wstring &Dest);
bool IsTextUtf8(const byte *Src);
bool IsTextUtf8(const byte *Src,size_t SrcSize);

int wcsicomp(const wchar *s1,const wchar *s2);
inline int wcsicomp(const std::wstring &s1,const std::wstring &s2) {return wcsicomp(s1.c_str(),s2.c_str());}
int wcsnicomp(const wchar *s1,const wchar *s2,size_t n);
inline int wcsnicomp(const std::wstring &s1,const std::wstring &s2,size_t n) {return wcsnicomp(s1.c_str(),s2.c_str(),n);}
const wchar_t* wcscasestr(const wchar_t *str, const wchar_t *search);
std::wstring::size_type wcscasestr(const std::wstring &str, const std::wstring &search);
#ifndef SFX_MODULE
wchar* wcslower(wchar *s);
void wcslower(std::wstring &s);
wchar* wcsupper(wchar *s);
void wcsupper(std::wstring &s);
#endif
int toupperw(int ch);
int tolowerw(int ch);
int atoiw(const std::wstring &s);
int64 atoilw(const std::wstring &s);

#ifdef DBCS_SUPPORTED
class SupportDBCS
{
  public:
    SupportDBCS();
    void Init();
    char* charnext(const char *s);
  static SupportDBCS& GetInstance();

    bool IsLeadByte[256];
    bool DBCSMode;
};

inline char* charnext(const char *s) {return (char *)(SupportDBCS::GetInstance().DBCSMode ? SupportDBCS::GetInstance().charnext(s):s+1);}
inline bool IsDBCSMode() {return SupportDBCS::GetInstance().DBCSMode;}

#else
#define charnext(s) ((s)+1)
#define IsDBCSMode() (false)
#endif


#endif
