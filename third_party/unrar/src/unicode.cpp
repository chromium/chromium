#include "rar.hpp"
#define MBFUNCTIONS

#if defined(_UNIX) && defined(MBFUNCTIONS)

static bool WideToCharMap(const wchar *Src,char *Dest,size_t DestSize,bool &Success);
static void CharToWideMap(const char *Src,wchar *Dest,size_t DestSize,bool &Success);

// In Unix we map high ASCII characters which cannot be converted to Unicode
// to 0xE000 - 0xE0FF private use Unicode area.
static const uint MapAreaStart=0xE000;

// Mapped string marker. Initially we used 0xFFFF for this purpose,
// but it causes MSVC2008 swprintf to fail (it treats 0xFFFF as error marker).
// While we could workaround it, it is safer to use another character.
static const uint MappedStringMark=0xFFFE;

#endif

bool WideToChar(const wchar *Src,char *Dest,size_t DestSize)
{
  bool RetCode=true;
  *Dest=0; // Set 'Dest' to zero just in case the conversion will fail.

#ifdef _WIN_ALL
  if (WideCharToMultiByte(CP_ACP,0,Src,-1,Dest,(int)DestSize,NULL,NULL)==0)
    RetCode=false;

// wcstombs is broken in Android NDK r9.
#elif defined(_APPLE)
  WideToUtf(Src,Dest,DestSize);

#elif defined(MBFUNCTIONS)
  if (!WideToCharMap(Src,Dest,DestSize,RetCode))
  {
    mbstate_t ps; // Use thread safe external state based functions.
    memset (&ps, 0, sizeof(ps));
    const wchar *SrcParam=Src; // wcsrtombs can change the pointer.

    // Some implementations of wcsrtombs can cause memory analyzing tools
    // like valgrind to report uninitialized data access. It happens because
    // internally these implementations call SSE4 based wcslen function,
    // which reads 16 bytes at once including those beyond of trailing 0.
    size_t ResultingSize=wcsrtombs(Dest,&SrcParam,DestSize,&ps);

    if (ResultingSize==(size_t)-1 && errno==EILSEQ)
    {
      // Aborted on inconvertible character not zero terminating the result.
      // EILSEQ helps to distinguish it from small output buffer abort.
      // We want to convert as much as we can, so we clean the output buffer
      // and repeat conversion.
      memset (&ps, 0, sizeof(ps));
      SrcParam=Src; // wcsrtombs can change the pointer.
      memset(Dest,0,DestSize);
      ResultingSize=wcsrtombs(Dest,&SrcParam,DestSize,&ps);
    }

    if (ResultingSize==(size_t)-1)
      RetCode=false;
    if (ResultingSize==0 && *Src!=0)
      RetCode=false;
  }
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(char)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
  if (DestSize>0)
    Dest[DestSize-1]=0;

  // We tried to return the empty string if conversion is failed,
  // but it does not work well. WideCharToMultiByte returns 'failed' code
  // and partially converted string even if we wanted to convert only a part
  // of string and passed DestSize smaller than required for fully converted
  // string. Such call is the valid behavior in RAR code and we do not expect
  // the empty string in this case.

  return RetCode;
}


bool CharToWide(const char *Src,wchar *Dest,size_t DestSize)
{
  bool RetCode=true;
  *Dest=0; // Set 'Dest' to zero just in case the conversion will fail.

#ifdef _WIN_ALL
  if (MultiByteToWideChar(CP_ACP,0,Src,-1,Dest,(int)DestSize)==0)
    RetCode=false;

// mbstowcs is broken in Android NDK r9.
#elif defined(_APPLE)
  UtfToWide(Src,Dest,DestSize);

#elif defined(MBFUNCTIONS)
  mbstate_t ps;
  memset (&ps, 0, sizeof(ps));
  const char *SrcParam=Src; // mbsrtowcs can change the pointer.
  size_t ResultingSize=mbsrtowcs(Dest,&SrcParam,DestSize,&ps);
  if (ResultingSize==(size_t)-1)
    RetCode=false;
  if (ResultingSize==0 && *Src!=0)
    RetCode=false;

  if (RetCode==false && DestSize>1)
    CharToWideMap(Src,Dest,DestSize,RetCode);
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(wchar_t)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
  if (DestSize>0)
    Dest[DestSize-1]=0;

  // We tried to return the empty string if conversion is failed,
  // but it does not work well. MultiByteToWideChar returns 'failed' code
  // even if we wanted to convert only a part of string and passed DestSize
  // smaller than required for fully converted string. Such call is the valid
  // behavior in RAR code and we do not expect the empty string in this case.

  return RetCode;
}


bool WideToChar(const std::wstring &Src,std::string &Dest)
{
  // We need more than 1 char per wchar_t for DBCS and up to 4 for UTF-8.
  std::vector<char> DestA(4*Src.size()+1); // "+1" for terminating zero.
  bool Result=WideToChar(Src.c_str(),DestA.data(),DestA.size());
  Dest=DestA.data();
  return Result;
}


bool CharToWide(const std::string &Src,std::wstring &Dest)
{
  // 2 wchar_t per char in case char is converted to UTF-16 surrogate pair.
  std::vector<wchar> DestW(2*Src.size()+1); // "+1" for terminating zero.
  bool Result=CharToWide(Src.c_str(),DestW.data(),DestW.size());
  Dest=DestW.data();
  return Result;
}


#if defined(_UNIX) && defined(MBFUNCTIONS)
// Convert and restore mapped inconvertible Unicode characters. 
// We use it for extended ASCII names in Unix.
bool WideToCharMap(const wchar *Src,char *Dest,size_t DestSize,bool &Success)
{
  // String with inconvertible characters mapped to private use Unicode area
  // must have the mark code somewhere.
  if (wcschr(Src,(wchar)MappedStringMark)==NULL)
    return false;

  // Seems to be that wcrtomb in some memory analyzing libraries
  // can produce uninitilized output while reporting success on garbage input.
  // So we clean the destination to calm analyzers.
  memset(Dest,0,DestSize);
  
  Success=true;
  uint SrcPos=0,DestPos=0;
  while (Src[SrcPos]!=0 && DestPos<DestSize-MB_CUR_MAX)
  {
    if (uint(Src[SrcPos])==MappedStringMark)
    {
      SrcPos++;
      continue;
    }
    // For security reasons do not restore low ASCII codes, so mapping cannot
    // be used to hide control codes like path separators.
    if (uint(Src[SrcPos])>=MapAreaStart+0x80 && uint(Src[SrcPos])<MapAreaStart+0x100)
      Dest[DestPos++]=char(uint(Src[SrcPos++])-MapAreaStart);
    else
    {
      mbstate_t ps;
      memset(&ps,0,sizeof(ps));
      if (wcrtomb(Dest+DestPos,Src[SrcPos],&ps)==(size_t)-1)
      {
        Dest[DestPos]='_';
        Success=false;
      }
      SrcPos++;
      memset(&ps,0,sizeof(ps));
      int Length=mbrlen(Dest+DestPos,MB_CUR_MAX,&ps);
      DestPos+=Max(Length,1);
    }
  }
  Dest[Min(DestPos,DestSize-1)]=0;
  return true;
}
#endif


#if defined(_UNIX) && defined(MBFUNCTIONS)
// Convert and map inconvertible Unicode characters.
// We use it for extended ASCII names in Unix.
void CharToWideMap(const char *Src,wchar *Dest,size_t DestSize,bool &Success)
{
  // Map inconvertible characters to private use Unicode area 0xE000.
  // Mark such string by placing special non-character code before
  // first inconvertible character.
  Success=false;
  bool MarkAdded=false;
  uint SrcPos=0,DestPos=0;
  while (DestPos<DestSize)
  {
    if (Src[SrcPos]==0)
    {
      Success=true;
      break;
    }
    mbstate_t ps;
    memset(&ps,0,sizeof(ps));
    size_t res=mbrtowc(Dest+DestPos,Src+SrcPos,MB_CUR_MAX,&ps);
    if (res==(size_t)-1 || res==(size_t)-2)
    {
      // For security reasons we do not want to map low ASCII characters,
      // so we do not have additional .. and path separator codes.
      if (byte(Src[SrcPos])>=0x80)
      {
        if (!MarkAdded)
        {
          Dest[DestPos++]=MappedStringMark;
          MarkAdded=true;
          if (DestPos>=DestSize)
            break;
        }
        Dest[DestPos++]=byte(Src[SrcPos++])+MapAreaStart;
      }
      else
        break;
    }
    else
    {
      memset(&ps,0,sizeof(ps));
      int Length=mbrlen(Src+SrcPos,MB_CUR_MAX,&ps);
      SrcPos+=Max(Length,1);
      DestPos++;
    }
  }
  Dest[Min(DestPos,DestSize-1)]=0;
}
#endif


// SrcSize is source data size in wide characters, not in bytes.
// DestSize is the maximum allowed destination size.
byte* WideToRaw(const wchar *Src,size_t SrcSize,byte *Dest,size_t DestSize)
{
  for (size_t I=0;I<SrcSize && I*2+1<DestSize;I++,Src++)
  {
    Dest[I*2]=(byte)*Src;
    Dest[I*2+1]=(byte)(*Src>>8);
    if (*Src==0)
      break;
  }
  return Dest;
}


// Store UTF-16 raw byte stream.
void WideToRaw(const std::wstring &Src,std::vector<byte> &Dest)
{
  for (wchar C : Src)
  {
    Dest.push_back((byte)C);
    Dest.push_back((byte)(C>>8));
  }
  Dest.push_back(0); // 2 bytes of trailing UTF-16 zero.
  Dest.push_back(0);
}


wchar* RawToWide(const byte *Src,wchar *Dest,size_t DestSize)
{
  for (size_t I=0;I<DestSize;I++)
    if ((Dest[I]=Src[I*2]+(Src[I*2+1]<<8))==0)
      break;
  return Dest;
}


std::wstring RawToWide(const std::vector<byte> &Src)
{
  std::wstring Dest;
  for (size_t I=0;I+1<Src.size();I+=2)
  {
    wchar c=Src[I]+(Src[I+1]<<8);
    Dest.push_back(c);
    if (c==0)
      break;
  }
  return Dest;
}


void WideToUtf(const wchar *Src,char *Dest,size_t DestSize)
{
  long dsize=(long)DestSize;
  dsize--;
  while (*Src!=0 && --dsize>=0)
  {
    uint c=*(Src++);
    if (c<0x80)
      *(Dest++)=c;
    else
      if (c<0x800 && --dsize>=0)
      {
        *(Dest++)=(0xc0|(c>>6));
        *(Dest++)=(0x80|(c&0x3f));
      }
      else
      {
        if (c>=0xd800 && c<=0xdbff && *Src>=0xdc00 && *Src<=0xdfff) // Surrogate pair.
        {
          c=((c-0xd800)<<10)+(*Src-0xdc00)+0x10000;
          Src++;
        }
        if (c<0x10000 && (dsize-=2)>=0)
        {
          *(Dest++)=(0xe0|(c>>12));
          *(Dest++)=(0x80|((c>>6)&0x3f));
          *(Dest++)=(0x80|(c&0x3f));
        }
        else
          if (c < 0x200000 && (dsize-=3)>=0)
          {
            *(Dest++)=(0xf0|(c>>18));
            *(Dest++)=(0x80|((c>>12)&0x3f));
            *(Dest++)=(0x80|((c>>6)&0x3f));
            *(Dest++)=(0x80|(c&0x3f));
          }
      }
  }
  *Dest=0;
}


void WideToUtf(const std::wstring &Src,std::string &Dest)
{
  for (size_t I=0;I<Src.size() && Src[I]!=0;)
  {
    uint c=Src[I++];
    if (c<0x80)
      Dest.push_back(c);
    else
      if (c<0x800)
      {
        Dest.push_back(0xc0|(c>>6));
        Dest.push_back(0x80|(c&0x3f));
      }
      else
      {
        if (c>=0xd800 && c<=0xdbff && I<Src.size() && Src[I]>=0xdc00 && Src[I]<=0xdfff) // Surrogate pair.
        {
          c=((c-0xd800)<<10)+(Src[I]-0xdc00)+0x10000;
          I++;
        }
        if (c<0x10000)
        {
          Dest.push_back(0xe0|(c>>12));
          Dest.push_back(0x80|((c>>6)&0x3f));
          Dest.push_back(0x80|(c&0x3f));
        }
        else
          if (c < 0x200000)
          {
            Dest.push_back(0xf0|(c>>18));
            Dest.push_back(0x80|((c>>12)&0x3f));
            Dest.push_back(0x80|((c>>6)&0x3f));
            Dest.push_back(0x80|(c&0x3f));
          }
      }
  }
}



size_t WideToUtfSize(const wchar *Src)
{
  size_t Size=0;
  for (;*Src!=0;Src++)
    if (*Src<0x80)
      Size++;
    else
      if (*Src<0x800)
        Size+=2;
      else
        if ((uint)*Src<0x10000) //(uint) to avoid Clang/win "always true" warning for 16-bit wchar_t.
        {
          if (Src[0]>=0xd800 && Src[0]<=0xdbff && Src[1]>=0xdc00 && Src[1]<=0xdfff)
          {
            Size+=4; // 4 output bytes for Unicode surrogate pair.
            Src++;
          }
          else
            Size+=3;
        }
        else
          if ((uint)*Src<0x200000) //(uint) to avoid Clang/win "always true" warning for 16-bit wchar_t.
            Size+=4;
  return Size+1; // Include terminating zero.
}


bool UtfToWide(const char *Src,wchar *Dest,size_t DestSize)
{
  bool Success=true;
  long dsize=(long)DestSize;
  dsize--;
  while (*Src!=0)
  {
    uint c=byte(*(Src++)),d;
    if (c<0x80)
      d=c;
    else
      if ((c>>5)==6)
      {
        if ((*Src&0xc0)!=0x80)
        {
          Success=false;
          break;
        }
        d=((c&0x1f)<<6)|(*Src&0x3f);
        Src++;
      }
      else
        if ((c>>4)==14)
        {
          if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80)
          {
            Success=false;
            break;
          }
          d=((c&0xf)<<12)|((Src[0]&0x3f)<<6)|(Src[1]&0x3f);
          Src+=2;
        }
        else
          if ((c>>3)==30)
          {
            if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80 || (Src[2]&0xc0)!=0x80)
            {
              Success=false;
              break;
            }
            d=((c&7)<<18)|((Src[0]&0x3f)<<12)|((Src[1]&0x3f)<<6)|(Src[2]&0x3f);
            Src+=3;
          }
          else
          {
            Success=false;
            break;
          }
    if (--dsize<0)
      break;
    if (d>0xffff)
    {
      if (--dsize<0)
        break;
      if (d>0x10ffff) // UTF-8 must end at 0x10ffff according to RFC 3629.
      {
        Success=false;
        continue;
      }
      if (sizeof(*Dest)==2) // Use the surrogate pair.
      {
        *(Dest++)=((d-0x10000)>>10)+0xd800;
        *(Dest++)=(d&0x3ff)+0xdc00;
      }
      else
        *(Dest++)=d;
    }
    else
      *(Dest++)=d;
  }
  *Dest=0;
  return Success;
}


bool UtfToWide(const char *Src,std::wstring &Dest)
{
  bool Success=true;
  Dest.clear();
  while (*Src!=0)
  {
    uint c=byte(*(Src++)),d;
    if (c<0x80)
      d=c;
    else
      if ((c>>5)==6)
      {
        if ((*Src&0xc0)!=0x80)
        {
          Success=false;
          break;
        }
        d=((c&0x1f)<<6)|(*Src&0x3f);
        Src++;
      }
      else
        if ((c>>4)==14)
        {
          if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80)
          {
            Success=false;
            break;
          }
          d=((c&0xf)<<12)|((Src[0]&0x3f)<<6)|(Src[1]&0x3f);
          Src+=2;
        }
        else
          if ((c>>3)==30)
          {
            if ((Src[0]&0xc0)!=0x80 || (Src[1]&0xc0)!=0x80 || (Src[2]&0xc0)!=0x80)
            {
              Success=false;
              break;
            }
            d=((c&7)<<18)|((Src[0]&0x3f)<<12)|((Src[1]&0x3f)<<6)|(Src[2]&0x3f);
            Src+=3;
          }
          else
          {
            Success=false;
            break;
          }
    if (d>0xffff)
    {
      if (d>0x10ffff) // UTF-8 must end at 0x10ffff according to RFC 3629.
      {
        Success=false;
        continue;
      }
      if (sizeof(wchar_t)==2) // Use the surrogate pair.
      {
        Dest.push_back( ((d-0x10000)>>10)+0xd800 );
        Dest.push_back( (d&0x3ff)+0xdc00 );
      }
      else
        Dest.push_back( d );
    }
    else
      Dest.push_back( d );
  }
  return Success;
}


/*
bool UtfToWide(const std::vector<char> &Src,std::wstring &Dest)
{
  bool Success=true;
  Dest.clear();
  for (size_t I=0;I<Src.size() && Src[I]!=0;) // We expect it to always stop at 0.
  {
    uint c=byte(Src[I++]),d;
    if (c<0x80)
      d=c;
    else
      if ((c>>5)==6)
      {
        if (Src.size()-I<1 || (Src[I]&0xc0)!=0x80)
        {
          Success=false;
          break;
        }
        d=((c&0x1f)<<6)|(Src[I]&0x3f);
        I++;
      }
      else
        if ((c>>4)==14)
        {
          if (Src.size()-I<2 || (Src[I]&0xc0)!=0x80 || (Src[I+1]&0xc0)!=0x80)
          {
            Success=false;
            break;
          }
          d=((c&0xf)<<12)|((Src[I]&0x3f)<<6)|(Src[I+1]&0x3f);
          I+=2;
        }
        else
          if ((c>>3)==30)
          {
            if (Src.size()-I<3 || (Src[I]&0xc0)!=0x80 || (Src[I+1]&0xc0)!=0x80 || (Src[I+2]&0xc0)!=0x80)
            {
              Success=false;
              break;
            }
            d=((c&7)<<18)|((Src[I]&0x3f)<<12)|((Src[I+1]&0x3f)<<6)|(Src[I+2]&0x3f);
            I+=3;
          }
          else
          {
            Success=false;
            break;
          }
    if (d>0xffff)
    {
      if (d>0x10ffff) // UTF-8 must end at 0x10ffff according to RFC 3629.
      {
        Success=false;
        continue;
      }
      if (sizeof(Dest[0])==2) // Use the surrogate pair.
      {
        Dest.push_back( ((d-0x10000)>>10)+0xd800 );
        Dest.push_back( (d&0x3ff)+0xdc00 );
      }
      else
        Dest.push_back( d );
    }
    else
      Dest.push_back( d );
  }
  return Success;
}
*/


// For zero terminated strings.
bool IsTextUtf8(const byte *Src)
{
  return IsTextUtf8(Src,strlen((const char *)Src));
}


// Source data can be both with and without UTF-8 BOM.
bool IsTextUtf8(const byte *Src,size_t SrcSize)
{
  while (SrcSize-- > 0)
  {
    byte C=*(Src++);
    int HighOne=0; // Number of leftmost '1' bits.
    for (byte Mask=0x80;Mask!=0 && (C & Mask)!=0;Mask>>=1)
      HighOne++;
    if (HighOne==1 || HighOne>6)
      return false;
    while (--HighOne > 0)
      if (SrcSize-- <= 0 || (*(Src++) & 0xc0)!=0x80)
        return false;
  }
  return true;
}


int wcsicomp(const wchar *s1,const wchar *s2)
{
#ifdef _WIN_ALL
  return CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,-1,s2,-1)-2;
#else
  while (true)
  {
    wchar u1 = towupper(*s1);
    wchar u2 = towupper(*s2);
    if (u1 != u2)
      return u1 < u2 ? -1 : 1;
    if (*s1==0)
      break;
    s1++;
    s2++;
  }
  return 0;
#endif
}


int wcsnicomp(const wchar *s1,const wchar *s2,size_t n)
{
#ifdef _WIN_ALL
  // If we specify 'n' exceeding the actual string length, CompareString goes
  // beyond the trailing zero and compares garbage. So we need to limit 'n'
  // to real string length.
  size_t sl1=wcslen(s1); // Pre-compute to not call wcslen() in Min() twice.
  size_t l1=Min(sl1+1,n);
  size_t sl2=wcslen(s2); // Pre-compute to not call wcslen() in Min() twice.
  size_t l2=Min(sl2+1,n);
  return CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,(int)l1,s2,(int)l2)-2;
#else
  if (n==0)
    return 0;
  while (true)
  {
    wchar u1 = towupper(*s1);
    wchar u2 = towupper(*s2);
    if (u1 != u2)
      return u1 < u2 ? -1 : 1;
    if (*s1==0 || --n==0)
      break;
    s1++;
    s2++;
  }
  return 0;
#endif
}


// Case insensitive wcsstr().
const wchar_t* wcscasestr(const wchar_t *str, const wchar_t *search)
{
  for (size_t i=0;str[i]!=0;i++)
    for (size_t j=0;;j++)
    {
      if (search[j]==0)
        return str+i;
      if (tolowerw(str[i+j])!=tolowerw(search[j]))
        break;
    }
  return nullptr;
}


// Case insensitive std::wstring substring search.
std::wstring::size_type wcscasestr(const std::wstring &str, const std::wstring &search)
{
  const wchar *Found=wcscasestr(str.c_str(),search.c_str());
  return Found==nullptr ? std::wstring::npos : Found-str.c_str();
}


#ifndef SFX_MODULE
wchar* wcslower(wchar *s)
{
#ifdef _WIN_ALL
#if defined(CHROMIUM_UNRAR)
  // kernel32!LCMapStringEx instead of user32.dll!CharUpper due to win32k
  // lockdown sandbox in chromium, user32.dll can't be loaded.
  const size_t s_length = wcslen(s) + 1;
  (void)LCMapStringEx(LOCALE_NAME_USER_DEFAULT, LCMAP_LOWERCASE, s, s_length, s,
                      s_length, NULL, NULL, 0);
  return s;
#else
  // _wcslwr requires setlocale and we do not want to depend on setlocale
  // in Windows. Also CharLower involves less overhead.
  CharLower(s);
#endif  // defined(CHROMIUM_UNRAR)
#else
  for (wchar *c=s;*c!=0;c++)
    *c=towlower(*c);
#endif
  return s;
}


void wcslower(std::wstring &s)
{
  wcslower(&s[0]);
}


wchar* wcsupper(wchar *s)
{
#ifdef _WIN_ALL
#if defined(CHROMIUM_UNRAR)
  // kernel32!LCMapStringEx instead of user32.dll!CharUpper due to win32k
  // lockdown sandbox in chromium, user32.dll can't be loaded.
  const size_t s_length = wcslen(s) + 1;
  (void)LCMapStringEx(LOCALE_NAME_USER_DEFAULT, LCMAP_UPPERCASE, s, s_length, s,
                      s_length, NULL, NULL, 0);
  return s;
#else
  // _wcsupr requires setlocale and we do not want to depend on setlocale
  // in Windows. Also CharUpper involves less overhead.
  CharUpper(s);
#endif  // defined(CHROMIUM_UNRAR)
#else
  for (wchar *c=s;*c!=0;c++)
    *c=towupper(*c);
#endif
  return s;
}


void wcsupper(std::wstring &s)
{
  wcsupper(&s[0]);
}
#endif




int toupperw(int ch)
{
#if defined(_WIN_ALL)
#if defined(CHROMIUM_UNRAR)
  wchar_t ch_as_wide_char = static_cast<wchar_t>(ch & 0xffff);
  // kernel32!LCMapStringEx instead of user32.dll!CharUpper due to win32k
  // lockdown sandbox in chromium, user32.dll can't be loaded.
  (void)LCMapStringEx(LOCALE_NAME_USER_DEFAULT, LCMAP_UPPERCASE,
                      &ch_as_wide_char, 1, &ch_as_wide_char, 1, NULL, NULL, 0);
  return int(ch_as_wide_char);
#else
  // CharUpper is more reliable than towupper in Windows, which seems to be
  // C locale dependent even in Unicode version. For example, towupper failed
  // to convert lowercase Russian characters. Use 0xffff mask to prevent crash
  // if value larger than 0xffff is passed to this function.
  return (int)(INT_PTR)CharUpper((wchar *)(INT_PTR)(ch&0xffff));
#endif  // defined(CHROMIUM_UNRAR)
#else
  return towupper(ch);
#endif
}


int tolowerw(int ch)
{
#if defined(_WIN_ALL)
#if defined(CHROMIUM_UNRAR)
  wchar_t ch_as_wide_char = static_cast<wchar_t>(ch & 0xffff);
  // kernel32!LCMapStringEx instead of user32.dll!CharLower due to win32k
  // lockdown sandbox in chromium, user32.dll can't be loaded.
  (void)LCMapStringEx(LOCALE_NAME_USER_DEFAULT, LCMAP_LOWERCASE,
                      &ch_as_wide_char, 1, &ch_as_wide_char, 1, NULL, NULL, 0);
  return int(ch_as_wide_char);
#else
  // CharLower is more reliable than towlower in Windows.
  // See comment for towupper above. Use 0xffff mask to prevent crash
  // if value larger than 0xffff is passed to this function.
  return (int)(INT_PTR)CharLower((wchar *)(INT_PTR)(ch&0xffff));
#endif  // defined(CHROMIUM_UNRAR)
#else
  return towlower(ch);
#endif
}


int atoiw(const std::wstring &s)
{
  return (int)atoilw(s);
}


int64 atoilw(const std::wstring &s)
{
  bool sign=false;
  size_t Pos=0;
  if (s[Pos]=='-') // We do use signed integers here, for example, in GUI SFX.
  {
    Pos++;
    sign=true;
  }
  // Use unsigned type here, since long string can overflow the variable
  // and signed integer overflow is undefined behavior in C++.
  uint64 n=0;
  while (s[Pos]>='0' && s[Pos]<='9')
  {
    n=n*10+(s[Pos]-'0');
    Pos++;
  }
  // Check int64(n)>=0 to avoid the signed overflow with undefined behavior
  // when negating 0x8000000000000000.
  return sign && int64(n)>=0 ? -int64(n) : int64(n);
}


#ifdef DBCS_SUPPORTED

SupportDBCS::SupportDBCS()
{
  Init();
}


void SupportDBCS::Init()
{
  CPINFO CPInfo;
  GetCPInfo(CP_ACP,&CPInfo);
  DBCSMode=CPInfo.MaxCharSize > 1;
  for (uint I=0;I<ASIZE(IsLeadByte);I++)
    IsLeadByte[I]=IsDBCSLeadByte(I)!=0;
}

// static
SupportDBCS& SupportDBCS::GetInstance() {
  static SupportDBCS supportDBCS;
  return supportDBCS;
}

char* SupportDBCS::charnext(const char *s)
{
  // Zero cannot be the trail byte. So if next byte after the lead byte
  // is 0, the string is corrupt and we'll better return the pointer to 0,
  // to break string processing loops.
  return (char *)(IsLeadByte[(byte)*s] && s[1]!=0 ? s+2:s+1);
}
#endif


