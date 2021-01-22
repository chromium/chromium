#ifndef _RAR_UNICODE_
#define _RAR_UNICODE_

#if defined( _WIN_ALL)
#define DBCS_SUPPORTED
#endif

bool WideToChar(const wchar *Src,char *Dest,size_t DestSize);
bool CharToWide(const char *Src,wchar *Dest,size_t DestSize);
byte* WideToRaw(const wchar *Src,byte *Dest,size_t SrcSize);
wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize);
void WideToUtf(const wchar *Src,char *Dest,size_t DestSize);
size_t WideToUtfSize(const wchar *Src);
bool UtfToWide(const char *Src,wchar *Dest,size_t DestSize);
bool IsTextUtf8(const byte *Src);
bool IsTextUtf8(const byte *Src,size_t SrcSize);

int wcsicomp(const wchar *s1,const wchar *s2);
int wcsnicomp(const wchar *s1,const wchar *s2,size_t n);
const wchar_t* wcscasestr(const wchar_t *str, const wchar_t *search);
#ifndef SFX_MODULE
wchar* wcslower(wchar *s);
wchar* wcsupper(wchar *s);
#endif
int toupperw(int ch);
int tolowerw(int ch);
int atoiw(const wchar *s);
int64 atoilw(const wchar *s);

#ifdef DBCS_SUPPORTED
class SupportDBCS
{
  public:
    SupportDBCS();
    void Init();
    static SupportDBCS& GetInstance();

    char* charnext(const char *s);
    size_t strlend(const char *s);
    char *strchrd(const char *s, int c);
    char *strrchrd(const char *s, int c);
    void copychrd(char *dest,const char *src);

    bool IsLeadByte[256];
    bool DBCSMode;
};

inline char* charnext(const char *s) {return (char *)(SupportDBCS::GetInstance().DBCSMode ? SupportDBCS::GetInstance().charnext(s):s+1);}
inline size_t strlend(const char *s) {return (uint)(SupportDBCS::GetInstance().DBCSMode ? SupportDBCS::GetInstance().strlend(s):strlen(s));}
inline char* strchrd(const char *s, int c) {return (char *)(SupportDBCS::GetInstance().DBCSMode ? SupportDBCS::GetInstance().strchrd(s,c):strchr(s,c));}
inline char* strrchrd(const char *s, int c) {return (char *)(SupportDBCS::GetInstance().DBCSMode ? SupportDBCS::GetInstance().strrchrd(s,c):strrchr(s,c));}
inline void copychrd(char *dest,const char *src) {if (SupportDBCS::GetInstance().DBCSMode) SupportDBCS::GetInstance().copychrd(dest,src); else *dest=*src;}
inline bool IsDBCSMode() {return(SupportDBCS::GetInstance().DBCSMode);}
inline void InitDBCS() {SupportDBCS::GetInstance().Init();}

#else
#define charnext(s) ((s)+1)
#define strlend strlen
#define strchrd strchr
#define strrchrd strrchr
#define IsDBCSMode() (true)
inline void copychrd(char *dest,const char *src) {*dest=*src;}
#endif

#endif
