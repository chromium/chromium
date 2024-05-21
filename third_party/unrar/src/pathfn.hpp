#ifndef _RAR_PATHFN_
#define _RAR_PATHFN_

wchar* PointToName(const wchar *Path);
std::wstring PointToName(const std::wstring &Path);
size_t GetNamePos(const std::wstring &Path);
wchar* PointToLastChar(const wchar *Path);
wchar GetLastChar(const std::wstring &Path);
size_t ConvertPath(const std::wstring *SrcPath,std::wstring *DestPath);
void SetName(std::wstring &FullName,const std::wstring &Name);
void SetExt(std::wstring &Name,std::wstring NewExt);
void RemoveExt(std::wstring &Name);
void SetSFXExt(std::wstring &SFXName);
wchar *GetExt(const wchar *Name);
std::wstring GetExt(const std::wstring &Name);
std::wstring::size_type GetExtPos(const std::wstring &Name);
bool CmpExt(const std::wstring &Name,const std::wstring &Ext);
bool IsWildcard(const std::wstring &Str);
bool IsPathDiv(int Ch);
bool IsDriveDiv(int Ch);
bool IsDriveLetter(const std::wstring &Path);
int GetPathDisk(const std::wstring &Path);
void AddEndSlash(std::wstring &Path);
void MakeName(const std::wstring &Path,const std::wstring &Name,std::wstring &Pathname);
void GetPathWithSep(const std::wstring &FullName,std::wstring &Path);
void RemoveNameFromPath(std::wstring &Path);
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool GetAppDataPath(std::wstring &Path,bool Create);
void GetRarDataPath(std::wstring &Path,bool Create);
#endif
#ifdef _WIN_ALL
bool SHGetPathStrFromIDList(PCIDLIST_ABSOLUTE pidl,std::wstring &Path);
#endif
#ifndef SFX_MODULE
bool EnumConfigPaths(uint Number,std::wstring &Path,bool Create);
void GetConfigName(const std::wstring &Name,std::wstring &FullName,bool CheckExist,bool Create);
#endif
size_t GetVolNumPos(const std::wstring &ArcName);
void NextVolumeName(std::wstring &ArcName,bool OldNumbering);
bool IsNameUsable(const std::wstring &Name);
void MakeNameUsable(std::wstring &Name,bool Extended);

void UnixSlashToDos(const char *SrcName,char *DestName,size_t MaxLength);
void UnixSlashToDos(const wchar *SrcName,wchar *DestName,size_t MaxLength);
void UnixSlashToDos(const std::string &SrcName,std::string &DestName);
void UnixSlashToDos(const std::wstring &SrcName,std::wstring &DestName);
void DosSlashToUnix(const char *SrcName,char *DestName,size_t MaxLength);
void DosSlashToUnix(const wchar *SrcName,wchar *DestName,size_t MaxLength);
void DosSlashToUnix(const std::string &SrcName,std::string &DestName);
void DosSlashToUnix(const std::wstring &SrcName,std::wstring &DestName);

inline void SlashToNative(const char *SrcName,char *DestName,size_t MaxLength)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName,MaxLength);
#else
  DosSlashToUnix(SrcName,DestName,MaxLength);
#endif
}

inline void SlashToNative(const std::string &SrcName,std::string &DestName)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName);
#else
  DosSlashToUnix(SrcName,DestName);
#endif
}

inline void SlashToNative(const wchar *SrcName,wchar *DestName,size_t MaxLength)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName,MaxLength);
#else
  DosSlashToUnix(SrcName,DestName,MaxLength);
#endif
}

inline void SlashToNative(const std::wstring &SrcName,std::wstring &DestName)
{
#ifdef _WIN_ALL
  UnixSlashToDos(SrcName,DestName);
#else
  DosSlashToUnix(SrcName,DestName);
#endif
}

void ConvertNameToFull(const std::wstring &Src,std::wstring &Dest);
bool IsFullPath(const std::wstring &Path);
bool IsFullRootPath(const std::wstring &Path);
void GetPathRoot(const std::wstring &Path,std::wstring &Root);
int ParseVersionFileName(std::wstring &Name,bool Truncate);
size_t VolNameToFirstName(const std::wstring &VolName,std::wstring &FirstName,bool NewNumbering);

#ifndef SFX_MODULE
void GenerateArchiveName(std::wstring &ArcName,const std::wstring &GenerateMask,bool Archiving);
#endif

#ifdef _WIN_ALL
bool GetWinLongPath(const std::wstring &Src,std::wstring &Dest);
void ConvertToPrecomposed(std::wstring &Name);
void MakeNameCompatible(std::wstring &Name);
#endif


#ifdef _WIN_ALL
std::wstring GetModuleFileStr();
std::wstring GetProgramFile(const std::wstring &Name);
#endif

#if defined(_WIN_ALL)
bool SetCurDir(const std::wstring &Dir);
#endif

#ifdef _WIN_ALL
bool GetCurDir(std::wstring &Dir);
#endif


#endif
