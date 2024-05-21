#include "rar.hpp"

FindFile::FindFile()
{
  FirstCall=true;
#ifdef _WIN_ALL
  hFind=INVALID_HANDLE_VALUE;
#else
  dirp=NULL;
#endif
}


FindFile::~FindFile()
{
#ifdef _WIN_ALL
  if (hFind!=INVALID_HANDLE_VALUE)
    FindClose(hFind);
#else
  if (dirp!=NULL)
    closedir(dirp);
#endif
}


void FindFile::SetMask(const std::wstring &Mask)
{
  FindMask=Mask;
  FirstCall=true;
}


bool FindFile::Next(FindData *fd,bool GetSymLink)
{
  fd->Error=false;
  if (FindMask.empty())
    return false;
#ifdef _WIN_ALL
  if (FirstCall)
  {
    if ((hFind=Win32Find(INVALID_HANDLE_VALUE,FindMask,fd))==INVALID_HANDLE_VALUE)
      return false;
  }
  else
    if (Win32Find(hFind,FindMask,fd)==INVALID_HANDLE_VALUE)
      return false;
#else
  if (FirstCall)
  {
    std::wstring DirName;
    DirName=FindMask;
    RemoveNameFromPath(DirName);
    if (DirName.empty())
      DirName=L".";
    std::string DirNameA;
    WideToChar(DirName,DirNameA);
    if ((dirp=opendir(DirNameA.c_str()))==NULL)
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  }
  while (1)
  {
    std::wstring Name;
    struct dirent *ent=readdir(dirp);
    if (ent==NULL)
      return false;
    if (strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0)
      continue;
    if (!CharToWide(std::string(ent->d_name),Name))
      uiMsg(UIERROR_INVALIDNAME,L"",Name);

    if (CmpName(FindMask,Name,MATCH_NAMES))
    {
      std::wstring FullName=FindMask;
      FullName.erase(GetNamePos(FullName));
      if (FullName.size()+Name.size()>=MAXPATHSIZE)
      {
        uiMsg(UIERROR_PATHTOOLONG,FullName,L"",Name);
        return false;
      }
      FullName+=Name;
      if (!FastFind(FullName,fd,GetSymLink))
      {
        ErrHandler.OpenErrorMsg(FullName);
        continue;
      }
      fd->Name=FullName;
      break;
    }
  }
#endif
  fd->Flags=0;
  fd->IsDir=IsDir(fd->FileAttr);
  fd->IsLink=IsLink(fd->FileAttr);

  FirstCall=false;
  std::wstring NameOnly=PointToName(fd->Name);
  if (NameOnly==L"." || NameOnly==L"..")
    return Next(fd);
  return true;
}


bool FindFile::FastFind(const std::wstring &FindMask,FindData *fd,bool GetSymLink)
{
  fd->Error=false;
#ifndef _UNIX
  if (IsWildcard(FindMask))
    return false;
#endif    
#ifdef _WIN_ALL
  HANDLE hFind=Win32Find(INVALID_HANDLE_VALUE,FindMask,fd);
  if (hFind==INVALID_HANDLE_VALUE)
    return false;
  FindClose(hFind);
#elif defined(_UNIX)
  std::string FindMaskA;
  WideToChar(FindMask,FindMaskA);

  struct stat st;
  if (GetSymLink)
  {
#ifdef SAVE_LINKS
    if (lstat(FindMaskA.c_str(),&st)!=0)
#else
    if (stat(FindMaskA.c_str(),&st)!=0)
#endif
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  }
  else
    if (stat(FindMaskA.c_str(),&st)!=0)
    {
      fd->Error=(errno!=ENOENT);
      return false;
    }
  fd->FileAttr=st.st_mode;
  fd->Size=st.st_size;

  File::StatToRarTime(st,&fd->mtime,&fd->ctime,&fd->atime);

  fd->Name=FindMask;
#endif
  fd->Flags=0;
  fd->IsDir=IsDir(fd->FileAttr);
  fd->IsLink=IsLink(fd->FileAttr);

  return true;
}


#ifdef _WIN_ALL
HANDLE FindFile::Win32Find(HANDLE hFind,const std::wstring &Mask,FindData *fd)
{
  WIN32_FIND_DATA FindData;
  if (hFind==INVALID_HANDLE_VALUE)
  {
    hFind=FindFirstFile(Mask.c_str(),&FindData);
    if (hFind==INVALID_HANDLE_VALUE)
    {
      std::wstring LongMask;
      if (GetWinLongPath(Mask,LongMask))
        hFind=FindFirstFile(LongMask.c_str(),&FindData);
    }
    if (hFind==INVALID_HANDLE_VALUE)
    {
      int SysErr=GetLastError();
      // We must not issue an error for "file not found" and "path not found",
      // because it is normal to not find anything for wildcard mask when
      // archiving. Also searching for non-existent file is normal in some
      // other modules, like WinRAR scanning for winrar_theme_description.txt
      // to check if any themes are available.
      fd->Error=SysErr!=ERROR_FILE_NOT_FOUND && 
                SysErr!=ERROR_PATH_NOT_FOUND &&
                SysErr!=ERROR_NO_MORE_FILES;
    }
  }
  else
    if (!FindNextFile(hFind,&FindData))
    {
      hFind=INVALID_HANDLE_VALUE;
      fd->Error=GetLastError()!=ERROR_NO_MORE_FILES;
    }

  if (hFind!=INVALID_HANDLE_VALUE)
  {
    fd->Name=Mask;
    SetName(fd->Name,FindData.cFileName);
    fd->Size=INT32TO64(FindData.nFileSizeHigh,FindData.nFileSizeLow);
    fd->FileAttr=FindData.dwFileAttributes;
    fd->ftCreationTime=FindData.ftCreationTime;
    fd->ftLastAccessTime=FindData.ftLastAccessTime;
    fd->ftLastWriteTime=FindData.ftLastWriteTime;
    fd->mtime.SetWinFT(&FindData.ftLastWriteTime);
    fd->ctime.SetWinFT(&FindData.ftCreationTime);
    fd->atime.SetWinFT(&FindData.ftLastAccessTime);


  }
  fd->Flags=0;
  return hFind;
}
#endif

