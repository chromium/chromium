#include "rar.hpp"

static bool match(const wchar *pattern,const wchar *string,bool ForceCase);
static int mwcsicompc(const wchar *Str1,const wchar *Str2,bool ForceCase);
static int mwcsnicompc(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase);
static bool IsWildcard(const wchar *Str,size_t CheckSize);

inline uint touppercw(uint ch,bool ForceCase)
{
  if (ForceCase)
    return ch;
#if defined(_UNIX)
  return ch;
#else
  return toupperw(ch);
#endif
}


bool CmpName(const wchar *Wildcard,const wchar *Name,uint CmpMode)
{
  bool ForceCase=(CmpMode&MATCH_FORCECASESENSITIVE)!=0;

  CmpMode&=MATCH_MODEMASK;

  wchar *Name1=PointToName(Wildcard);
  wchar *Name2=PointToName(Name);

  if (CmpMode!=MATCH_NAMES)
  {
    size_t WildLength=wcslen(Wildcard);
    if (CmpMode!=MATCH_EXACT && CmpMode!=MATCH_EXACTPATH && CmpMode!=MATCH_ALLWILD &&
        mwcsnicompc(Wildcard,Name,WildLength,ForceCase)==0)
    {
      // For all modes except MATCH_NAMES, MATCH_EXACT, MATCH_EXACTPATH, MATCH_ALLWILD,
      // "path1" mask must match "path1\path2\filename.ext" and "path1" names.
      wchar NextCh=Name[WildLength];
      if (NextCh==L'\\' || NextCh==L'/' || NextCh==0)
        return true;
    }

    // Nothing more to compare for MATCH_SUBPATHONLY.
    if (CmpMode==MATCH_SUBPATHONLY)
      return false;
    
    // 2023.08.29: We tried std::wstring Path1 and Path2 here, but performance
    // impact for O(n^2) complexity loop in CmdExtract::AnalyzeArchive()
    // was rather noticeable, 1.7s instead of 0.9s when extracting ~300 files
    // with @listfile from archive with ~7000 files.
    // This function can be invoked from other O(n^2) loops. So for now
    // we prefer to avoid wstring and use pointers and path sizes here.
    // Another option could be using std::wstring_view.

    size_t Path1Size=Name1-Wildcard;
    size_t Path2Size=Name2-Name;

    if ((CmpMode==MATCH_EXACT || CmpMode==MATCH_EXACTPATH) && 
        (Path1Size!=Path2Size ||
        mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0))
      return false;
    if (CmpMode==MATCH_ALLWILD)
      return match(Wildcard,Name,ForceCase);
    if (CmpMode==MATCH_SUBPATH || CmpMode==MATCH_WILDSUBPATH)
      if (IsWildcard(Wildcard,Path1Size))
        return match(Wildcard,Name,ForceCase);
      else
        if (CmpMode==MATCH_SUBPATH || IsWildcard(Wildcard))
        {
          if (Path1Size>0 && mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0)
            return false;
        }
        else
          if (Path1Size!=Path2Size || mwcsnicompc(Wildcard,Name,Path1Size,ForceCase)!=0)
            return false;
  }

  if (CmpMode==MATCH_EXACT)
    return mwcsicompc(Name1,Name2,ForceCase)==0;

  return match(Name1,Name2,ForceCase);
}


bool match(const wchar *pattern,const wchar *string,bool ForceCase)
{
  for (;; ++string)
  {
    wchar stringc=touppercw(*string,ForceCase);
    wchar patternc=touppercw(*pattern++,ForceCase);
    switch (patternc)
    {
      case 0:
        return stringc==0;
      case '?':
        if (stringc == 0)
          return false;
        break;
      case '*':
        if (*pattern==0)
          return true;
        if (*pattern=='.')
        {
          if (pattern[1]=='*' && pattern[2]==0)
            return true;
          const wchar *dot=wcschr(string,'.');
          if (pattern[1]==0)
            return (dot==NULL || dot[1]==0);
          if (dot!=NULL)
          {
            string=dot;
            if (wcspbrk(pattern,L"*?")==NULL && wcschr(string+1,'.')==NULL)
              return mwcsicompc(pattern+1,string+1,ForceCase)==0;
          }
        }

        while (*string)
          if (match(pattern,string++,ForceCase))
            return true;
        return false;
      default:
        if (patternc != stringc)
        {
          // Allow "name." mask match "name" and "name.\" match "name\".
          if (patternc=='.' && (stringc==0 || stringc=='\\' || stringc=='.'))
            return match(pattern,string,ForceCase);
          else
            return false;
        }
        break;
    }
  }
}


int mwcsicompc(const wchar *Str1,const wchar *Str2,bool ForceCase)
{
  if (ForceCase)
    return wcscmp(Str1,Str2);
  return wcsicompc(Str1,Str2);
}


int mwcsnicompc(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase)
{
  if (ForceCase)
    return wcsncmp(Str1,Str2,N);
#if defined(_UNIX)
  return wcsncmp(Str1,Str2,N);
#else
  return wcsnicomp(Str1,Str2,N);
#endif
}


bool IsWildcard(const wchar *Str,size_t CheckSize)
{
  size_t CheckPos=0;
#ifdef _WIN_ALL
  // Not treat the special NTFS \\?\d: path prefix as a wildcard.
  if (Str[0]=='\\' && Str[1]=='\\' && Str[2]=='?' && Str[3]=='\\')
    CheckPos+=4;
#endif
  for (size_t I=CheckPos;I<CheckSize && Str[I]!=0;I++)
    if (Str[I]=='*' || Str[I]=='?')
      return true;
  return false;
}
