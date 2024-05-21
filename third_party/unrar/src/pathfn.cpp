#include "rar.hpp"

wchar* PointToName(const wchar *Path)
{
  for (int I=(int)wcslen(Path)-1;I>=0;I--)
    if (IsPathDiv(Path[I]))
      return (wchar*)&Path[I+1];
  return (wchar*)((*Path!=0 && IsDriveDiv(Path[1])) ? Path+2:Path);
}


std::wstring PointToName(const std::wstring &Path)
{
  return std::wstring(Path.substr(GetNamePos(Path)));
}


size_t GetNamePos(const std::wstring &Path)
{
  for (int I=(int)Path.size()-1;I>=0;I--)
    if (IsPathDiv(Path[I]))
      return I+1;
  return IsDriveLetter(Path) ? 2 : 0;
}


wchar* PointToLastChar(const wchar *Path)
{
  size_t Length=wcslen(Path);
  return (wchar*)(Length>0 ? Path+Length-1:Path);
}


wchar GetLastChar(const std::wstring &Path)
{
  return Path.empty() ? 0:Path.back();
}


size_t ConvertPath(const std::wstring *SrcPath,std::wstring *DestPath)
{
  const std::wstring &S=*SrcPath; // To avoid *SrcPath[] everywhere.
  size_t DestPos=0;

  // Prevent \..\ in any part of path string and \.. at the end of string
  for (size_t I=0;I<S.size();I++)
    if (IsPathDiv(S[I]) && S[I+1]=='.' && S[I+2]=='.' &&
        (IsPathDiv(S[I+3]) || S[I+3]==0))
      DestPos=S[I+3]==0 ? I+3 : I+4;

  // Remove any amount of <d>:\ and any sequence of . and \ in the beginning of path string.
  while (DestPos<S.size())
  {
    size_t I=DestPos;
    if (I+1<S.size() && IsDriveDiv(S[I+1]))
      I+=2;

    // Skip UNC Windows \\server\share\ or Unix //server/share/
    if (IsPathDiv(S[I]) && IsPathDiv(S[I+1]))
    {
      uint SlashCount=0;
      for (size_t J=I+2;J<S.size();J++)
        if (IsPathDiv(S[J]) && ++SlashCount==2)
        {
          I=J+1; // Found two more path separators after leading two.
          break;
        }
    }

    // Skip any amount of .\ and ..\ in the beginning of path.
    for (size_t J=I;J<S.size();J++)
      if (IsPathDiv(S[J]))
        I=J+1;
      else
        if (S[J]!='.')
          break;
    if (I==DestPos) // If nothing was removed.
      break;
    DestPos=I;
  }

  // SrcPath and DestPath can point to same memory area, so we always create
  // the new string with substr() here.
  if (DestPath!=nullptr)
    *DestPath=S.substr(DestPos);

  return DestPos;
}


void SetName(std::wstring &FullName,const std::wstring &Name)
{
  auto Pos=GetNamePos(FullName);
  FullName.replace(Pos,std::wstring::npos,Name);
}


void SetExt(std::wstring &Name,std::wstring NewExt)
{
  auto DotPos=GetExtPos(Name);
  if (DotPos!=std::wstring::npos)
    Name.erase(DotPos);
  Name+=L"."+NewExt;
}


// Unlike SetExt(Name,L""), it removes the trailing dot too.
void RemoveExt(std::wstring &Name)
{
  auto DotPos=GetExtPos(Name);
  if (DotPos!=std::wstring::npos)
    Name.erase(DotPos);
}


#ifndef SFX_MODULE
void SetSFXExt(std::wstring &SFXName)
{
#ifdef _WIN_ALL
  SetExt(SFXName,L"exe");
#elif defined(_UNIX)
  SetExt(SFXName,L"sfx");
#endif
}
#endif


// 'Ext' is an extension with the leading dot, like L".rar".
wchar *GetExt(const wchar *Name)
{
  return Name==NULL ? NULL:wcsrchr(PointToName(Name),'.');
}


// 'Ext' is an extension with the leading dot, like L".rar", or empty string
// if extension is missing.
std::wstring GetExt(const std::wstring &Name)
{
  auto ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos)
    ExtPos=Name.size(); // If '.' is missing, return the empty string.
  return Name.substr(ExtPos);
}


// Returns the position of extension leading dot or std::wstring::npos
// if extension is not present.
std::wstring::size_type GetExtPos(const std::wstring &Name)
{
  auto NamePos=GetNamePos(Name);
  auto DotPos=Name.rfind('.');
  return DotPos<NamePos ? std::wstring::npos : DotPos;
}


// 'Ext' is an extension without the leading dot, like L"rar".
bool CmpExt(const std::wstring &Name,const std::wstring &Ext)
{
  size_t ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos)
    return Ext.empty();
  // We need case insensitive compare, so can't use wstring::compare().
  return wcsicomp(&Name[ExtPos+1],Ext.data())==0;
}


bool IsWildcard(const std::wstring &Str)
{
  size_t StartPos=0;
#ifdef _WIN_ALL
  // Not treat the special NTFS \\?\d: path prefix as a wildcard.
  if (Str.rfind(L"\\\\?\\",0)==0)
    StartPos=4;
#endif
  return Str.find_first_of(L"*?",StartPos)!=std::wstring::npos;
}


bool IsPathDiv(int Ch)
{
#ifdef _WIN_ALL
  return Ch=='\\' || Ch=='/';
#else
  return Ch==CPATHDIVIDER;
#endif
}


bool IsDriveDiv(int Ch)
{
#ifdef _UNIX
  return false;
#else
  return Ch==':';
#endif
}


bool IsDriveLetter(const std::wstring &Path)
{
  if (Path.size()<2)
    return false;
  wchar Letter=etoupperw(Path[0]);
  return Letter>='A' && Letter<='Z' && IsDriveDiv(Path[1]);
}


int GetPathDisk(const std::wstring &Path)
{
  if (IsDriveLetter(Path))
    return etoupperw(Path[0])-'A';
  else
    return -1;
}


void AddEndSlash(std::wstring &Path)
{
  if (!Path.empty() && Path.back()!=CPATHDIVIDER)
    Path+=CPATHDIVIDER;
}


void MakeName(const std::wstring &Path,const std::wstring &Name,std::wstring &Pathname)
{
  // 'Path', 'Name' and 'Pathname' can point to same string. So we use
  // the temporary buffer instead of constructing the name in 'Pathname'.
  std::wstring OutName=Path;
  // Do not add slash to d:, we want to allow relative paths like d:filename.
  if (!IsDriveLetter(Path) || Path.size()>2)
    AddEndSlash(OutName);
  OutName+=Name;
  Pathname=OutName;
}


// Returns the file path including the trailing path separator symbol.
// It is allowed for both parameters to point to the same string.
void GetPathWithSep(const std::wstring &FullName,std::wstring &Path)
{
  if (std::addressof(FullName)!=std::addressof(Path))
    Path=FullName;
  Path.erase(GetNamePos(FullName));
}


// Removes name and returns file path without the trailing path separator.
// But for names like d:\name return d:\ with trailing path separator.
void RemoveNameFromPath(std::wstring &Path)
{
  auto NamePos=GetNamePos(Path);
  if (NamePos>=2 && (!IsDriveDiv(Path[1]) || NamePos>=4))
    NamePos--;
  Path.erase(NamePos);
}


#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool GetAppDataPath(std::wstring &Path,bool Create)
{
  LPMALLOC g_pMalloc;
  SHGetMalloc(&g_pMalloc);
  LPITEMIDLIST ppidl;
  Path.clear();
  bool Success=false;
  if (SHGetSpecialFolderLocation(NULL,CSIDL_APPDATA,&ppidl)==NOERROR &&
      SHGetPathStrFromIDList(ppidl,Path) && !Path.empty())
  {
    AddEndSlash(Path);
    Path+=L"WinRAR";
    Success=FileExist(Path);
    if (!Success && Create)
      Success=CreateDir(Path);
  }
  g_pMalloc->Free(ppidl);
  return Success;
}
#endif


#if defined(_WIN_ALL)
bool SHGetPathStrFromIDList(PCIDLIST_ABSOLUTE pidl,std::wstring &Path)
{
  std::vector<wchar> Buf(MAX_PATH);
  bool Success=SHGetPathFromIDList(pidl,Buf.data())!=FALSE;
  Path=Buf.data();
  return Success;
}
#endif


#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void GetRarDataPath(std::wstring &Path,bool Create)
{
  Path.clear();

  HKEY hKey;
  if (RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\WinRAR\\Paths",0,
                   KEY_QUERY_VALUE,&hKey)==ERROR_SUCCESS)
  {
    DWORD DataSize;
    LSTATUS Code=RegQueryValueEx(hKey,L"AppData",NULL,NULL,NULL,&DataSize);
    if (Code==ERROR_SUCCESS)
    {
      std::vector<wchar> PathBuf(DataSize/sizeof(wchar));
      RegQueryValueEx(hKey,L"AppData",0,NULL,(BYTE *)PathBuf.data(),&DataSize);
      Path=PathBuf.data();
      RegCloseKey(hKey);
    }
  }

  if (Path.empty() || !FileExist(Path))
    if (!GetAppDataPath(Path,Create))
    {
      Path=GetModuleFileStr();
      RemoveNameFromPath(Path);
    }
}
#endif


#ifndef SFX_MODULE
bool EnumConfigPaths(uint Number,std::wstring &Path,bool Create)
{
#ifdef _UNIX
  static const wchar *ConfPath[]={
    L"/etc", L"/etc/rar", L"/usr/lib", L"/usr/local/lib", L"/usr/local/etc"
  };
  if (Number==0)
  {
    char *EnvStr=getenv("HOME");
    if (EnvStr!=NULL)
      CharToWide(EnvStr,Path);
    else
      Path=ConfPath[0];
    return true;
  }
  Number--;
  if (Number>=ASIZE(ConfPath))
    return false;
  Path=ConfPath[Number];
  return true;
#elif defined(_WIN_ALL)
  if (Number>1)
    return false;
  if (Number==0)
    GetRarDataPath(Path,Create);
  else
  {
    Path=GetModuleFileStr();
    RemoveNameFromPath(Path);
  }
  return true;
#else
  return false;
#endif
}
#endif


#ifndef SFX_MODULE
void GetConfigName(const std::wstring &Name,std::wstring &FullName,bool CheckExist,bool Create)
{
  FullName.clear();
  for (uint I=0;;I++)
  {
    std::wstring ConfPath;
    if (!EnumConfigPaths(I,ConfPath,Create))
      break;
    MakeName(ConfPath,Name,FullName);
    if (!CheckExist || WildFileExist(FullName))
      break;
  }
}
#endif


// Returns the position to rightmost digit of volume number or beginning
// of file name if numeric part is missing.
size_t GetVolNumPos(const std::wstring &ArcName)
{
  // We do not want to increment any characters in path component.
  size_t NamePos=GetNamePos(ArcName);

  if (NamePos==ArcName.size())
    return NamePos;

  // Pointing to last name character.
  size_t Pos=ArcName.size()-1;

  // Skipping the archive extension.
  while (!IsDigit(ArcName[Pos]) && Pos>NamePos)
    Pos--;

  // Skipping the numeric part of name.
  size_t NumPos=Pos;
  while (IsDigit(ArcName[NumPos]) && NumPos>NamePos)
    NumPos--;

  // Searching for first numeric part in names like name.part##of##.rar.
  // Stop search on the first dot.
  while (NumPos>NamePos && ArcName[NumPos]!='.')
  {
    if (IsDigit(ArcName[NumPos]))
    {
      // Validate the first numeric part only if it has a dot somewhere 
      // before it.
      auto DotPos=ArcName.find('.',NamePos);
      if (DotPos!=std::wstring::npos && DotPos<NumPos)
        Pos=NumPos;
      break;
    }
    NumPos--;
  }
  return Pos;
}


void NextVolumeName(std::wstring &ArcName,bool OldNumbering)
{
  auto DotPos=GetExtPos(ArcName);
  if (DotPos==std::wstring::npos)
  {
    ArcName+=L".rar";
    DotPos=GetExtPos(ArcName);
  }
  else
    if (DotPos+1==ArcName.size() || CmpExt(ArcName,L"exe") || CmpExt(ArcName,L"sfx"))
      SetExt(ArcName,L"rar");

  if (!OldNumbering)
  {
    size_t NumPos=GetVolNumPos(ArcName);

    // We should not check for IsDigit() here and should increment
    // even non-digits. If we got a corrupt archive with volume flag,
    // but without numeric part, we still need to modify its name somehow,
    // so "while (Exist()) {NextVolumeName();}" loops do not run infinitely.
    while (++ArcName[NumPos]=='9'+1)
    {
      ArcName[NumPos]='0';
      if (NumPos==0)
        break;
      NumPos--;
      if (!IsDigit(ArcName[NumPos]))
      {
        // Convert .part:.rar (.part9.rar after increment) to part10.rar.
        ArcName.insert(NumPos+1,1,'1');
        break;
      }
    }
  }
  else
  {
    // If extension is shorter than 3 characters, set it to "rar" to simplify
    // further processing.
    if (ArcName.size()-DotPos<3)
      ArcName.replace(DotPos+1,std::wstring::npos,L"rar");

    if (!IsDigit(ArcName[DotPos+2]) || !IsDigit(ArcName[DotPos+3]))
      ArcName.replace(DotPos+2,std::wstring::npos,L"00"); // From .rar to .r00.
    else
    {
      auto NumPos=ArcName.size()-1;  // Set to last character.
      while (++ArcName[NumPos]=='9'+1)
        if (NumPos==0 || ArcName[NumPos-1]=='.')
        {
          ArcName[NumPos]='a'; // From .999 to .a00 if started from .001 or for too short names.
          break;
        }
        else
          ArcName[NumPos--]='0';
    }
  }
}


bool IsNameUsable(const std::wstring &Name)
{
  // We were asked to apply Windows-like conversion in Linux in case
  // files are unpacked to Windows share. This code is invoked only
  // if file failed to be created, so it doesn't affect extraction
  // of Unix compatible names to native Unix drives.
#ifdef _UNIX
  // Windows shares in Unix do not allow the drive letter,
  // so unlike Windows version, we check all characters here.
  if (Name.find(':')!=std::wstring::npos)
    return false;
#else
  if (Name.find(':',2)!=std::wstring::npos)
    return false;
#endif
  for (size_t I=0;I<Name.size();I++)
  {
    if ((uint)Name[I]<32)
      return false;

     // It is for Windows shares in Unix. We can create such names in Windows.
#ifdef _UNIX
    // No spaces or dots before the path separator are allowed in Windows
    // shares. But they are allowed and automatically removed at the end of
    // file or folder name, so it is useless to replace them here.
    // Since such files or folders are created successfully, a supposed
    // conversion here would never be invoked.
    if ((Name[I]==' ' || Name[I]=='.') && IsPathDiv(Name[I+1]))
      return false;
#endif
  }
  return !Name.empty() && Name.find_first_of(L"?*<>|\"")==std::wstring::npos;
}


void MakeNameUsable(std::wstring &Name,bool Extended)
{
  for (size_t I=0;I<Name.size();I++)
  {
    if (wcschr(Extended ? L"?*<>|\"":L"?*",Name[I])!=NULL || 
        Extended && (uint)Name[I]<32)
      Name[I]='_';
#ifdef _UNIX
    // We were asked to apply Windows-like conversion in Linux in case
    // files are unpacked to Windows share. This code is invoked only
    // if file failed to be created, so it doesn't affect extraction
    // of Unix compatible names to native Unix drives.
    if (Extended)
    {
      // Windows shares in Unix do not allow the drive letter,
      // so unlike Windows version, we check all characters here.
      if (Name[I]==':')
        Name[I]='_';

      // No spaces or dots before the path separator are allowed on Windows
      // shares. But they are allowed and automatically removed at the end of
      // file or folder name, so we need to replace them only before
      // the path separator, but not at the end of file name.
      // Since such files or folders are created successfully, a supposed
      // conversion at the end of file name would never be invoked here.
      // While converting dots, we preserve "." and ".." path components,
      // such as when specifying ".." in the destination path.
      if (IsPathDiv(Name[I+1]) && (Name[I]==' ' || Name[I]=='.' && I>0 &&
          !IsPathDiv(Name[I-1]) && (Name[I-1]!='.' || I>1 && !IsPathDiv(Name[I-2]))))
        Name[I]='_';
    }
#else
    if (I>1 && Name[I]==':')
      Name[I]='_';
#endif
  }
}


void UnixSlashToDos(const char *SrcName,char *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='/' ? '\\':SrcName[Copied];
  DestName[Copied]=0;
}


void UnixSlashToDos(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='/' ? '\\':SrcName[Copied];
  DestName[Copied]=0;
}


void UnixSlashToDos(const std::string &SrcName,std::string &DestName)
{
  // SrcName and DestName can point to same string, so no .clear() here.
  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='/' ? '\\':SrcName[I];
}


void UnixSlashToDos(const std::wstring &SrcName,std::wstring &DestName)
{
  // SrcName and DestName can point to same string, so no .clear() here.
  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='/' ? '\\':SrcName[I];
}


void DosSlashToUnix(const char *SrcName,char *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='\\' ? '/':SrcName[Copied];
  DestName[Copied]=0;
}


void DosSlashToUnix(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
  size_t Copied=0;
  for (;Copied<MaxLength-1 && SrcName[Copied]!=0;Copied++)
    DestName[Copied]=SrcName[Copied]=='\\' ? '/':SrcName[Copied];
  DestName[Copied]=0;
}


void DosSlashToUnix(const std::string &SrcName,std::string &DestName)
{
  // SrcName and DestName can point to same string, so no .clear() here.
  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='\\' ? '/':SrcName[I];
}


void DosSlashToUnix(const std::wstring &SrcName,std::wstring &DestName)
{
  // SrcName and DestName can point to same string, so no .clear() here.
  DestName.resize(SrcName.size());
  for (size_t I=0;I<SrcName.size();I++)
    DestName[I]=SrcName[I]=='\\' ? '/':SrcName[I];
}


void ConvertNameToFull(const std::wstring &Src,std::wstring &Dest)
{
  if (Src.empty())
  {
    Dest.clear();
    return;
  }
#ifdef _WIN_ALL
  {
    DWORD Code=GetFullPathName(Src.c_str(),0,NULL,NULL); // Get the buffer size.
    if (Code!=0)
    {
      std::vector<wchar> FullName(Code);
      Code=GetFullPathName(Src.c_str(),(DWORD)FullName.size(),FullName.data(),NULL);

      if (Code>0 && Code<=FullName.size())
      {
        Dest=FullName.data();
        return;
      }
    }

    std::wstring LongName;
    if (GetWinLongPath(Src,LongName)) // Failed with normal name, try long.
    {
      Code=GetFullPathName(LongName.c_str(),0,NULL,NULL); // Get the buffer size.
      if (Code!=0)
      {
        std::vector<wchar> FullName(Code);
        Code=GetFullPathName(LongName.c_str(),(DWORD)FullName.size(),FullName.data(),NULL);

        if (Code>0 && Code<=FullName.size())
        {
          Dest=FullName.data();
          return;
        }
      }
    }
    if (Src!=Dest)
      Dest=Src; // Copy source to destination in case of failure.
  }
#elif defined(_UNIX)
  if (IsFullPath(Src))
    Dest.clear();
  else
  {
    std::vector<char> CurDirA(MAXPATHSIZE);
    if (getcwd(CurDirA.data(),CurDirA.size())==NULL)
      CurDirA[0]=0;
    CharToWide(CurDirA.data(),Dest);
    AddEndSlash(Dest);
  }
  Dest+=Src;
#else
  Dest=Src;
#endif
}


bool IsFullPath(const std::wstring &Path)
{
#ifdef _WIN_ALL
  return Path.size()>=2 && Path[0]=='\\' && Path[1]=='\\' || 
         Path.size()>=3 && IsDriveLetter(Path) && IsPathDiv(Path[2]);
#else
  return Path.size()>=1 && IsPathDiv(Path[0]);
#endif
}


bool IsFullRootPath(const std::wstring &Path)
{
  return IsFullPath(Path) || IsPathDiv(Path[0]);
}


// Both source and destination can point to the same string.
void GetPathRoot(const std::wstring &Path,std::wstring &Root)
{
  if (IsDriveLetter(Path))
    Root=Path.substr(0,2) + L"\\";
  else
    if (Path[0]=='\\' && Path[1]=='\\')
    {
      size_t Slash=Path.find('\\',2);
      if (Slash!=std::wstring::npos)
      {
        size_t Length;
        if ((Slash=Path.find('\\',Slash+1))!=std::wstring::npos)
          Length=Slash+1;
        else
          Length=Path.size();
        Root=Path.substr(0,Length);
      }
    }
    else
      Root.clear();
}


int ParseVersionFileName(std::wstring &Name,bool Truncate)
{
  int Version=0;
  auto VerPos=Name.rfind(';');
  if (VerPos!=std::wstring::npos && VerPos+1<Name.size())
  {
    Version=atoiw(&Name[VerPos+1]);
    if (Truncate)
      Name.erase(VerPos);
  }
  return Version;
}


#if !defined(SFX_MODULE)
// Get the name of first volume. Return the leftmost digit position of volume number.
size_t VolNameToFirstName(const std::wstring &VolName,std::wstring &FirstName,bool NewNumbering)
{
  // Source and destination can point at the same string, so we use
  // the intermediate variable.
  std::wstring Name=VolName;
  size_t VolNumStart=0;
  if (NewNumbering)
  {
    wchar N='1';

    // From the rightmost digit of volume number to the left.
    for (size_t Pos=GetVolNumPos(Name);Pos>0;Pos--)
      if (IsDigit(Name[Pos]))
      {
        Name[Pos]=N; // Set the rightmost digit to '1' and others to '0'.
        N='0';
      }
      else
        if (N=='0') // If we already set the rightmost '1' before.
        {
          VolNumStart=Pos+1; // Store the position of leftmost digit in volume number.
          break;
        }
  }
  else
  {
    // Old volume numbering scheme. Just set the extension to ".rar".
    SetExt(Name,L"rar");
    VolNumStart=GetExtPos(Name);
  }
  if (!FileExist(Name))
  {
    // If the first volume, which name we just generated, does not exist,
    // check if volume with same name and any other extension is available.
    // It can help in case of *.exe or *.sfx first volume.
    std::wstring Mask=Name;
    SetExt(Mask,L"*");
    FindFile Find;
    Find.SetMask(Mask);
    FindData FD;
    while (Find.Next(&FD))
    {
      Archive Arc;
      if (Arc.Open(FD.Name,0) && Arc.IsArchive(true) && Arc.FirstVolume)
      {
        Name=FD.Name;
        break;
      }
    }
  }
  FirstName=Name;
  return VolNumStart;
}
#endif


#ifndef SFX_MODULE
static void GenArcName(std::wstring &ArcName,const std::wstring &GenerateMask,uint ArcNumber,bool &ArcNumPresent)
{
  size_t Pos=0;
  bool Prefix=false;
  if (GenerateMask[0]=='+')
  {
    Prefix=true;    // Add the time string before the archive name.
    Pos++;          // Skip '+' in the beginning of time mask.
  }

  std::wstring Mask=!GenerateMask.empty() ? GenerateMask.substr(Pos):L"yyyymmddhhmmss";

  bool QuoteMode=false;
  uint MAsMinutes=0; // By default we treat 'M' as months.
  for (uint I=0;I<Mask.size();I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    if (QuoteMode)
      continue;
    int CurChar=toupperw(Mask[I]);
    if (CurChar=='H')
      MAsMinutes=2; // Treat next two 'M' after 'H' as minutes.
    if (CurChar=='D' || CurChar=='Y')
      MAsMinutes=0; // Treat 'M' in HHDDMMYY and HHYYMMDD as month.

    if (MAsMinutes>0 && CurChar=='M')
    {
      // Replace minutes with 'I'. We use 'M' both for months and minutes,
      // so we treat as minutes only those 'M', which are found after hours.
      Mask[I]='I';
      MAsMinutes--;
    }
    if (CurChar=='N')
    {
      uint Digits=GetDigits(ArcNumber);
      uint NCount=0;
      while (toupperw(Mask[I+NCount])=='N')
        NCount++;

      // Here we ensure that we have enough 'N' characters to fit all digits
      // of archive number. We'll replace them by actual number later
      // in this function.
      if (NCount<Digits)
        Mask.insert(I,Digits-NCount,L'N');
      I+=Max(Digits,NCount)-1;
      ArcNumPresent=true;
      continue;
    }
  }

  RarTime CurTime;
  CurTime.SetCurrentTime();
  RarLocalTime rlt;
  CurTime.GetLocal(&rlt);

  std::wstring Ext;
  auto ExtPos=GetExtPos(ArcName);
  if (ExtPos==std::wstring::npos)
    Ext=PointToName(ArcName).empty() ? L".rar":L"";
  else
  {
    Ext=ArcName.substr(ExtPos);
    ArcName.erase(ExtPos);
  }

  int WeekDay=rlt.wDay==0 ? 6:rlt.wDay-1;
  int StartWeekDay=rlt.yDay-WeekDay;
  if (StartWeekDay<0)
    if (StartWeekDay<=-4)
      StartWeekDay+=IsLeapYear(rlt.Year-1) ? 366:365;
    else
      StartWeekDay=0;
  int CurWeek=StartWeekDay/7+1;
  if (StartWeekDay%7>=4)
    CurWeek++;

  const size_t FieldSize=11;
  char Field[10][FieldSize];

  snprintf(Field[0],FieldSize,"%04u",rlt.Year);
  snprintf(Field[1],FieldSize,"%02u",rlt.Month);
  snprintf(Field[2],FieldSize,"%02u",rlt.Day);
  snprintf(Field[3],FieldSize,"%02u",rlt.Hour);
  snprintf(Field[4],FieldSize,"%02u",rlt.Minute);
  snprintf(Field[5],FieldSize,"%02u",rlt.Second);
  snprintf(Field[6],FieldSize,"%02u",(uint)CurWeek);
  snprintf(Field[7],FieldSize,"%u",(uint)WeekDay+1);
  snprintf(Field[8],FieldSize,"%03u",rlt.yDay+1);
  snprintf(Field[9],FieldSize,"%05u",ArcNumber);

  const wchar *MaskChars=L"YMDHISWAEN";

  // How many times every modifier character was encountered in the mask.
  int CField[sizeof(Field)/sizeof(Field[0])]{};

  QuoteMode=false;
  for (uint I=0;I<Mask.size();I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    if (QuoteMode)
      continue;
    const wchar *ChPtr=wcschr(MaskChars,toupperw(Mask[I]));
    if (ChPtr!=NULL)
      CField[ChPtr-MaskChars]++;
   }

  wchar DateText[MAX_GENERATE_MASK];
  *DateText=0;
  QuoteMode=false;
  for (size_t I=0,J=0;I<Mask.size() && J<ASIZE(DateText)-1;I++)
  {
    if (Mask[I]=='{' || Mask[I]=='}')
    {
      QuoteMode=(Mask[I]=='{');
      continue;
    }
    const wchar *ChPtr=wcschr(MaskChars,toupperw(Mask[I]));
    if (ChPtr==NULL || QuoteMode)
    {
      DateText[J]=Mask[I];
#ifdef _WIN_ALL
      // We do not allow ':' in Windows because of NTFS streams.
      // Users had problems after specifying hh:mm mask.
      if (DateText[J]==':')
        DateText[J]='_';
#endif
    }
    else
    {
      size_t FieldPos=ChPtr-MaskChars;
      int CharPos=(int)strlen(Field[FieldPos])-CField[FieldPos]--;

      // CField[FieldPos] shall have exactly 3 "MMM" symbols, so we do not
      // repeat the month name in case "MMMMMMMM" mask. But since we
      // decremented CField[FieldPos] above, we compared it with 2.
      if (FieldPos==1 && CField[FieldPos]==2 &&
          toupperw(Mask[I+1])=='M' && toupperw(Mask[I+2])=='M')
      {
        wcsncpyz(DateText+J,GetMonthName(rlt.Month-1),ASIZE(DateText)-J);
        J=wcslen(DateText);
        I+=2;
        continue;
      }
      // If CharPos is negative, we have more modifier characters than
      // matching time data. We prefer to issue a modifier character
      // instead of repeating time data from beginning, so user can notice
      // excessive modifiers added by mistake.
      if (CharPos<0)
        DateText[J]=Mask[I];
      else
        DateText[J]=Field[FieldPos][CharPos];
    }
    DateText[++J]=0;
  }

  if (Prefix)
  {
    std::wstring NewName;
    GetPathWithSep(ArcName,NewName);
    NewName+=DateText;
    NewName+=PointToName(ArcName);
    ArcName=NewName;
  }
  else
    ArcName+=DateText;
  ArcName+=Ext;
}


void GenerateArchiveName(std::wstring &ArcName,const std::wstring &GenerateMask,bool Archiving)
{
  std::wstring NewName;

  uint ArcNumber=1;
  while (true) // Loop for 'N' (archive number) processing.
  {
    NewName=ArcName;
    
    bool ArcNumPresent=false;

    GenArcName(NewName,GenerateMask,ArcNumber,ArcNumPresent);
    
    if (!ArcNumPresent)
      break;
    if (!FileExist(NewName))
    {
      if (!Archiving && ArcNumber>1)
      {
        // If we perform non-archiving operation, we need to use the last
        // existing archive before the first unused name. So we generate
        // the name for (ArcNumber-1) below.
        NewName=ArcName;
        GenArcName(NewName,GenerateMask,ArcNumber-1,ArcNumPresent);
      }
      break;
    }
    ArcNumber++;
  }
  ArcName=NewName;
}
#endif


#ifdef _WIN_ALL
// We should return 'true' even if resulting path is shorter than MAX_PATH,
// because we can also use this function to open files with non-standard
// characters, even if their path length is normal.
bool GetWinLongPath(const std::wstring &Src,std::wstring &Dest)
{
  if (Src.empty())
    return false;
  const std::wstring Prefix=L"\\\\?\\";

  bool FullPath=Src.size()>=3 && IsDriveLetter(Src) && IsPathDiv(Src[2]);
  if (IsFullPath(Src)) // Paths in d:\path\name format.
  {
    if (IsDriveLetter(Src))
    {
      Dest=Prefix+Src; // "\\?\D:\very long path".
      return true;
    }
    else
      if (Src.size()>2 && Src[0]=='\\' && Src[1]=='\\')
      {
        Dest=Prefix+L"UNC"+Src.substr(1);  // "\\?\UNC\server\share".
        return true;
      }
    // We can be here only if modify IsFullPath() in the future.
    return false;
  }
  else
  {
    std::wstring CurDir;
    if (!GetCurDir(CurDir))
      return false;

    if (IsPathDiv(Src[0])) // Paths in \path\name format.
    {
      Dest=Prefix+CurDir[0]+L':'+Src;  // Copy drive letter 'd:'.
      return true;
    }
    else  // Paths in path\name format.
    {
      Dest=Prefix+CurDir;
      AddEndSlash(Dest);

      size_t Pos=0;
      if (Src[0]=='.' && IsPathDiv(Src[1])) // Remove leading .\ in pathname.
        Pos=2;

      Dest+=Src.substr(Pos);
      return true;
    }
  }
  return false;
}


// Convert Unix, OS X and Android decomposed chracters to Windows precomposed.
void ConvertToPrecomposed(std::wstring &Name)
{
  if (WinNT()<WNT_VISTA) // MAP_PRECOMPOSED is not supported in XP.
    return;
  int Size=FoldString(MAP_PRECOMPOSED,Name.c_str(),-1,NULL,0);
  if (Size<=0)
    return;
  std::vector<wchar> FileName(Size);
  if (FoldString(MAP_PRECOMPOSED,Name.c_str(),-1,FileName.data(),(int)FileName.size())!=0)
    Name=FileName.data();
}


void MakeNameCompatible(std::wstring &Name)
{
  // Remove trailing spaces and dots in file name and in dir names in path.
  for (int I=0;I<(int)Name.size();I++)
    if (I+1==Name.size() || IsPathDiv(Name[I+1]))
      while (I>=0 && (Name[I]=='.' || Name[I]==' '))
      {
        if (I==0 && Name[I]==' ')
        {
          // Windows 10 Explorer can't rename or delete " " files and folders.
          Name[I]='_'; // Convert " /path" to "_/path".
          break;
        }
        if (Name[I]=='.')
        {
          // 2024.05.01: Permit ./path1, path1/./path2, ../path1,
          // path1/../path2 and exotic Win32 d:.\path1, d:..\path1 paths
          // requested by user. Leading dots are possible here if specified
          // by user in the destination path.
          if (I==0 || IsPathDiv(Name[I-1]) || I==2 && IsDriveLetter(Name))
            break;
          if (I>=1 && Name[I-1]=='.' && (I==1 || IsPathDiv(Name[I-2]) ||
              I==3 && IsDriveLetter(Name)))
            break;
        }
        Name.erase(I,1);
        I--;
      }

  // Rename reserved device names, such as aux.txt to _aux.txt.
  // We check them in path components too, where they are also prohibited.
  for (size_t I=0;I<Name.size();I++)
    if (I==0 || I>0 && IsPathDiv(Name[I-1]))
    {
      static const wchar *Devices[]={L"CON",L"PRN",L"AUX",L"NUL",L"COM#",L"LPT#"};
      const wchar *s=&Name[I];
      bool MatchFound=false;
      for (uint J=0;J<ASIZE(Devices);J++)
        for (uint K=0;;K++)
          if (Devices[J][K]=='#')
          {
            if (!IsDigit(s[K]))
              break;
          }
          else
            if (Devices[J][K]==0)
            {
              // Names like aux.txt are accessible without \\?\ prefix
              // since Windows 11. Pure aux is still prohibited.
              MatchFound=s[K]==0 || s[K]=='.' && !IsWindows11OrGreater() || IsPathDiv(s[K]);
              break;
            }
            else
              if (Devices[J][K]!=toupperw(s[K]))
                break;
      if (MatchFound)
      {
        std::wstring OrigName=Name;
        Name.insert(I,1,'_');
#ifndef SFX_MODULE
        uiMsg(UIMSG_CORRECTINGNAME,nullptr);
        uiMsg(UIERROR_RENAMING,nullptr,OrigName,Name);
#endif
      }
    }
}
#endif




#ifdef _WIN_ALL
std::wstring GetModuleFileStr()
{
  HMODULE hModule=nullptr;
  
  std::vector<wchar> Path(256);
  while (Path.size()<=MAXPATHSIZE)
  {
    if (GetModuleFileName(hModule,Path.data(),(DWORD)Path.size())<Path.size())
      break;
    Path.resize(Path.size()*4);
  }
  return std::wstring(Path.data());
}


// Return the pathname of file in RAR or WinRAR folder.
// 'Name' can point to non-existent file and include wildcards.
std::wstring GetProgramFile(const std::wstring &Name)
{
  std::wstring FullName=GetModuleFileStr();
  SetName(FullName,Name);
  return FullName;
}
#endif


#if defined(_WIN_ALL)
bool SetCurDir(const std::wstring &Dir)
{
  return SetCurrentDirectory(Dir.c_str())!=0;
}
#endif


#ifdef _WIN_ALL
bool GetCurDir(std::wstring &Dir)
{
  DWORD BufSize=GetCurrentDirectory(0,NULL);
  if (BufSize==0)
    return false;
  std::vector<wchar> Buf(BufSize);
  DWORD Code=GetCurrentDirectory((DWORD)Buf.size(),Buf.data());
  Dir=Buf.data();
  return Code!=0;
}
#endif


