#include "rar.hpp"

MKDIR_CODE MakeDir(const std::wstring &Name,bool SetAttr,uint Attr)
{
#ifdef _WIN_ALL
  // Windows automatically removes dots and spaces in the end of directory
  // name. So we detect such names and process them with \\?\ prefix.
  wchar LastChar=GetLastChar(Name);
  bool Special=LastChar=='.' || LastChar==' ';
  BOOL RetCode=Special ? FALSE : CreateDirectory(Name.c_str(),NULL);
  if (RetCode==0 && !FileExist(Name))
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      RetCode=CreateDirectory(LongName.c_str(),NULL);
  }
  if (RetCode!=0) // Non-zero return code means success for CreateDirectory.
  {
    if (SetAttr)
      SetFileAttr(Name,Attr);
    return MKDIR_SUCCESS;
  }
  int ErrCode=GetLastError();
  if (ErrCode==ERROR_FILE_NOT_FOUND || ErrCode==ERROR_PATH_NOT_FOUND)
    return MKDIR_BADPATH;
  return MKDIR_ERROR;
#elif defined(_UNIX)
  std::string NameA;
  WideToChar(Name,NameA);
  mode_t uattr=SetAttr ? (mode_t)Attr:0777;
  int ErrCode=mkdir(NameA.c_str(),uattr);
  if (ErrCode==-1)
    return errno==ENOENT ? MKDIR_BADPATH:MKDIR_ERROR;
  return MKDIR_SUCCESS;
#else
  return MKDIR_ERROR;
#endif
}


// Simplified version of MakeDir().
bool CreateDir(const std::wstring &Name)
{
  return MakeDir(Name,false,0)==MKDIR_SUCCESS;
}


bool CreatePath(const std::wstring &Path,bool SkipLastName,bool Silent)
{
  if (Path.empty())
    return false;

#ifdef _WIN_ALL
  uint DirAttr=0;
#else
  uint DirAttr=0777;
#endif
  
  bool Success=true;

  for (size_t I=0;I<Path.size();I++)
  {
    // Process all kinds of path separators, so user can enter Unix style
    // path in Windows or Windows in Unix. I>0 check avoids attempting
    // creating an empty directory for paths starting from path separator.
    if (IsPathDiv(Path[I]) && I>0)
    {
#ifdef _WIN_ALL
      // We must not attempt to create "D:" directory, because first
      // CreateDirectory will fail, so we'll use \\?\D:, which forces Wine
      // to create "D:" directory.
      if (I==2 && Path[1]==':')
        continue;
#endif
      std::wstring DirName=Path.substr(0,I);
      Success=MakeDir(DirName,true,DirAttr)==MKDIR_SUCCESS;
      if (Success && !Silent)
      {
        mprintf(St(MCreatDir),DirName.c_str());
        mprintf(L" %s",St(MOk));
      }
    }
  }
  if (!SkipLastName && !IsPathDiv(GetLastChar(Path)))
    Success=MakeDir(Path,true,DirAttr)==MKDIR_SUCCESS;
  return Success;
}


void SetDirTime(const std::wstring &Name,RarTime *ftm,RarTime *ftc,RarTime *fta)
{
#if defined(_WIN_ALL)
  bool sm=ftm!=NULL && ftm->IsSet();
  bool sc=ftc!=NULL && ftc->IsSet();
  bool sa=fta!=NULL && fta->IsSet();

  uint DirAttr=GetFileAttr(Name);
  bool ResetAttr=(DirAttr!=0xffffffff && (DirAttr & FILE_ATTRIBUTE_READONLY)!=0);
  if (ResetAttr)
    SetFileAttr(Name,0);

  HANDLE hFile=CreateFile(Name.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                          NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      hFile=CreateFile(LongName.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                       NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
  }

  if (hFile==INVALID_HANDLE_VALUE)
    return;
  FILETIME fm,fc,fa;
  if (sm)
    ftm->GetWinFT(&fm);
  if (sc)
    ftc->GetWinFT(&fc);
  if (sa)
    fta->GetWinFT(&fa);
  SetFileTime(hFile,sc ? &fc:NULL,sa ? &fa:NULL,sm ? &fm:NULL);
  CloseHandle(hFile);
  if (ResetAttr)
    SetFileAttr(Name,DirAttr);
#endif
#ifdef _UNIX
  File::SetCloseFileTimeByName(Name,ftm,fta);
#endif
}


bool IsRemovable(const std::wstring &Name)
{
#if defined(_WIN_ALL)
  std::wstring Root;
  GetPathRoot(Name,Root);
  int Type=GetDriveType(Root.empty() ? nullptr : Root.c_str());
  return Type==DRIVE_REMOVABLE || Type==DRIVE_CDROM;
#else
  return false;
#endif
}


#ifndef SFX_MODULE
int64 GetFreeDisk(const std::wstring &Name)
{
#ifdef _WIN_ALL
  std::wstring Root;
  GetPathWithSep(Name,Root);

  ULARGE_INTEGER uiTotalSize,uiTotalFree,uiUserFree;
  uiUserFree.u.LowPart=uiUserFree.u.HighPart=0;
  if (GetDiskFreeSpaceEx(Root.empty() ? NULL:Root.c_str(),&uiUserFree,&uiTotalSize,&uiTotalFree) &&
      uiUserFree.u.HighPart<=uiTotalFree.u.HighPart)
    return INT32TO64(uiUserFree.u.HighPart,uiUserFree.u.LowPart);
  return 0;
#elif defined(_UNIX)
  std::wstring Root;
  GetPathWithSep(Name,Root);
  std::string RootA;
  WideToChar(Root,RootA);
  struct statvfs sfs;
  if (statvfs(RootA.empty() ? ".":RootA.c_str(),&sfs)!=0)
    return 0;
  int64 FreeSize=sfs.f_bsize;
  FreeSize=FreeSize*sfs.f_bavail;
  return FreeSize;
#else
  return 0;
#endif
}
#endif


#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
// Return 'true' for FAT and FAT32, so we can adjust the maximum supported
// file size to 4 GB for these file systems.
bool IsFAT(const std::wstring &Name)
{
  std::wstring Root;
  GetPathRoot(Name,Root);
  wchar FileSystem[MAX_PATH+1];
  // Root can be empty, when we create volumes with -v in the current folder.
  if (GetVolumeInformation(Root.empty() ? NULL:Root.c_str(),NULL,0,NULL,NULL,NULL,FileSystem,ASIZE(FileSystem)))
    return wcscmp(FileSystem,L"FAT")==0 || wcscmp(FileSystem,L"FAT32")==0;
  return false;
}
#endif


bool FileExist(const std::wstring &Name)
{
#ifdef _WIN_ALL
  return GetFileAttr(Name)!=0xffffffff;
#elif defined(ENABLE_ACCESS)
  std::string NameA;
  WideToChar(Name,NameA);
  return access(NameA.c_str(),0)==0;
#else
  FindData FD;
  return FindFile::FastFind(Name,&FD);
#endif
}
 

bool WildFileExist(const std::wstring &Name)
{
  if (IsWildcard(Name))
  {
    FindFile Find;
    Find.SetMask(Name);
    FindData fd;
    return Find.Next(&fd);
  }
  return FileExist(Name);
}


bool IsDir(uint Attr)
{
#ifdef _WIN_ALL
  return Attr!=0xffffffff && (Attr & FILE_ATTRIBUTE_DIRECTORY)!=0;
#endif
#if defined(_UNIX)
  return (Attr & 0xF000)==0x4000;
#endif
}


bool IsUnreadable(uint Attr)
{
#if defined(_UNIX) && defined(S_ISFIFO) && defined(S_ISSOCK) && defined(S_ISCHR)
  return S_ISFIFO(Attr) || S_ISSOCK(Attr) || S_ISCHR(Attr);
#else
  return false;
#endif
}


bool IsLink(uint Attr)
{
#ifdef _UNIX
  return (Attr & 0xF000)==0xA000;
#elif defined(_WIN_ALL)
  return (Attr & FILE_ATTRIBUTE_REPARSE_POINT)!=0;
#else
  return false;
#endif
}






bool IsDeleteAllowed(uint FileAttr)
{
#ifdef _WIN_ALL
  return (FileAttr & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN))==0;
#else
  return (FileAttr & (S_IRUSR|S_IWUSR))==(S_IRUSR|S_IWUSR);
#endif
}


void PrepareToDelete(const std::wstring &Name)
{
#ifdef _WIN_ALL
  SetFileAttr(Name,0);
#endif
#ifdef _UNIX
  std::string NameA;
  WideToChar(Name,NameA);
  chmod(NameA.c_str(),S_IRUSR|S_IWUSR|S_IXUSR);
#endif
}


uint GetFileAttr(const std::wstring &Name)
{
#ifdef _WIN_ALL
  DWORD Attr=GetFileAttributes(Name.c_str());
  if (Attr==0xffffffff)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Attr=GetFileAttributes(LongName.c_str());
  }
  return Attr;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  struct stat st;
  if (stat(NameA.c_str(),&st)!=0)
    return 0;
  return st.st_mode;
#endif
}


bool SetFileAttr(const std::wstring &Name,uint Attr)
{
#ifdef _WIN_ALL
  bool Success=SetFileAttributes(Name.c_str(),Attr)!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=SetFileAttributes(LongName.c_str(),Attr)!=0;
  }
  return Success;
#elif defined(_UNIX)
  std::string NameA;
  WideToChar(Name,NameA);
  return chmod(NameA.c_str(),(mode_t)Attr)==0;
#else
  return false;
#endif
}


wchar* MkTemp(wchar *Name,size_t MaxSize)
{
  size_t Length=wcslen(Name);

  RarTime CurTime;
  CurTime.SetCurrentTime();

  // We cannot use CurTime.GetWin() as is, because its lowest bits can
  // have low informational value, like being a zero or few fixed numbers.
  uint Random=(uint)(CurTime.GetWin()/100000);

  // Using PID we guarantee that different RAR copies use different temp names
  // even if started in exactly the same time.
  uint PID=0;
#ifdef _WIN_ALL
  PID=(uint)GetCurrentProcessId();
#elif defined(_UNIX)
  PID=(uint)getpid();
#endif

  for (uint Attempt=0;;Attempt++)
  {
    uint Ext=Random%50000+Attempt;
    wchar RndText[50];
    // User asked to specify the single extension for all temporary files,
    // so it can be added to server ransomware protection exceptions.
    // He wrote, this protection blocks temporary files when adding
    // a file to RAR archive with drag and drop.
    swprintf(RndText,ASIZE(RndText),L"%u.%03u.rartemp",PID,Ext);
    if (Length+wcslen(RndText)>=MaxSize || Attempt==1000)
      return NULL;
    wcsncpyz(Name+Length,RndText,MaxSize-Length);
    if (!FileExist(Name))
      break;
  }
  return Name;
}


bool MkTemp(std::wstring &Name)
{
  RarTime CurTime;
  CurTime.SetCurrentTime();

  // We cannot use CurTime.GetWin() as is, because its lowest bits can
  // have low informational value, like being a zero or few fixed numbers.
  uint Random=(uint)(CurTime.GetWin()/100000);

  // Using PID we guarantee that different RAR copies use different temp names
  // even if started in exactly the same time.
  uint PID=0;
#ifdef _WIN_ALL
  PID=(uint)GetCurrentProcessId();
#elif defined(_UNIX)
  PID=(uint)getpid();
#endif

  for (uint Attempt=0;;Attempt++)
  {
    uint Ext=Random%50000+Attempt;
    if (Attempt==1000)
      return false;
    std::wstring NewName=Name + std::to_wstring(PID) + L"." + std::to_wstring(Ext) + L".rartemp";
    if (!FileExist(NewName))
    {
      Name=NewName;
      break;
    }
  }
  return true;
}


#if !defined(SFX_MODULE)
void CalcFileSum(File *SrcFile,uint *CRC32,byte *Blake2,uint Threads,int64 Size,uint Flags)
{
  int64 SavePos=SrcFile->Tell();
#ifndef SILENT
  int64 FileLength=Size==INT64NDF ? SrcFile->FileLength() : Size;
#endif

  if ((Flags & (CALCFSUM_SHOWTEXT|CALCFSUM_SHOWPERCENT))!=0)
    uiMsg(UIEVENT_FILESUMSTART);

  if ((Flags & CALCFSUM_CURPOS)==0)
    SrcFile->Seek(0,SEEK_SET);

  const size_t BufSize=0x100000;
  std::vector<byte> Data(BufSize);

  DataHash HashCRC,HashBlake2;
  HashCRC.Init(HASH_CRC32,Threads);
  HashBlake2.Init(HASH_BLAKE2,Threads);

  int64 BlockCount=0;
  int64 TotalRead=0;
  while (true)
  {
    size_t SizeToRead;
    if (Size==INT64NDF)   // If we process the entire file.
      SizeToRead=BufSize; // Then always attempt to read the entire buffer.
    else
      SizeToRead=(size_t)Min((int64)BufSize,Size);
    int ReadSize=SrcFile->Read(Data.data(),SizeToRead);
    if (ReadSize==0)
      break;
    TotalRead+=ReadSize;

    if ((++BlockCount & 0xf)==0)
    {
#ifndef SILENT
      if ((Flags & CALCFSUM_SHOWPROGRESS)!=0)
      {
        // Update only the current file progress in WinRAR, set the total to 0
        // to keep it as is. It looks better for WinRAR.
        uiExtractProgress(TotalRead,FileLength,0,0);
      }
      else
      {
        if ((Flags & CALCFSUM_SHOWPERCENT)!=0)
          uiMsg(UIEVENT_FILESUMPROGRESS,ToPercent(TotalRead,FileLength));
      }
#endif
      Wait();
    }

    if (CRC32!=NULL)
      HashCRC.Update(Data.data(),ReadSize);
    if (Blake2!=NULL)
      HashBlake2.Update(Data.data(),ReadSize);

    if (Size!=INT64NDF)
      Size-=ReadSize;
  }
  SrcFile->Seek(SavePos,SEEK_SET);

  if ((Flags & CALCFSUM_SHOWPERCENT)!=0)
    uiMsg(UIEVENT_FILESUMEND);

  if (CRC32!=NULL)
    *CRC32=HashCRC.GetCRC32();
  if (Blake2!=NULL)
  {
    HashValue Result;
    HashBlake2.Result(&Result);
    memcpy(Blake2,Result.Digest,sizeof(Result.Digest));
  }
}
#endif


bool RenameFile(const std::wstring &SrcName,const std::wstring &DestName)
{
#ifdef _WIN_ALL
  bool Success=MoveFile(SrcName.c_str(),DestName.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName1,LongName2;
    if (GetWinLongPath(SrcName,LongName1) && GetWinLongPath(DestName,LongName2))
      Success=MoveFile(LongName1.c_str(),LongName2.c_str())!=0;
  }
  return Success;
#else
  std::string SrcNameA,DestNameA;
  WideToChar(SrcName,SrcNameA);
  WideToChar(DestName,DestNameA);
  bool Success=rename(SrcNameA.c_str(),DestNameA.c_str())==0;
  return Success;
#endif
}


bool DelFile(const std::wstring &Name)
{
#ifdef _WIN_ALL
  bool Success=DeleteFile(Name.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=DeleteFile(LongName.c_str())!=0;
  }
  return Success;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  bool Success=remove(NameA.c_str())==0;
  return Success;
#endif
}


bool DelDir(const std::wstring &Name)
{
#ifdef _WIN_ALL
  bool Success=RemoveDirectory(Name.c_str())!=0;
  if (!Success)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      Success=RemoveDirectory(LongName.c_str())!=0;
  }
  return Success;
#else
  std::string NameA;
  WideToChar(Name,NameA);
  bool Success=rmdir(NameA.c_str())==0;
  return Success;
#endif
}


#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool SetFileCompression(const std::wstring &Name,bool State)
{
  HANDLE hFile=CreateFile(Name.c_str(),FILE_READ_DATA|FILE_WRITE_DATA,
                 FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
                 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    std::wstring LongName;
    if (GetWinLongPath(Name,LongName))
      hFile=CreateFile(LongName.c_str(),FILE_READ_DATA|FILE_WRITE_DATA,
                 FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,
                 FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  }
  if (hFile==INVALID_HANDLE_VALUE)
    return false;
  SHORT NewState=State ? COMPRESSION_FORMAT_DEFAULT:COMPRESSION_FORMAT_NONE;
  DWORD Result;
  int RetCode=DeviceIoControl(hFile,FSCTL_SET_COMPRESSION,&NewState,
                              sizeof(NewState),NULL,0,&Result,NULL);
  CloseHandle(hFile);
  return RetCode!=0;
}


void ResetFileCache(const std::wstring &Name)
{
  // To reset file cache in Windows it is enough to open it with
  // FILE_FLAG_NO_BUFFERING and then close it.
  HANDLE hSrc=CreateFile(Name.c_str(),GENERIC_READ,
                         FILE_SHARE_READ|FILE_SHARE_WRITE,
                         NULL,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,NULL);
  if (hSrc!=INVALID_HANDLE_VALUE)
    CloseHandle(hSrc);
}
#endif












// Delete symbolic links in file path, if any, and replace them by directories.
// Prevents extracting files outside of destination folder with symlink chains.
bool LinksToDirs(const std::wstring &SrcName,const std::wstring &SkipPart,std::wstring &LastChecked)
{
  // Unlike Unix, Windows doesn't expand lnk1 in symlink targets like
  // "lnk1/../dir", but converts the path to "dir". In Unix we need to call
  // this function to prevent placing unpacked files outside of destination
  // folder if previously we unpacked "dir/lnk1" -> "..",
  // "dir/lnk2" -> "lnk1/.." and "dir/lnk2/anypath/poc.txt".
  // We may still need this function to prevent abusing symlink chains
  // in link source path if we remove detection of such chains
  // in IsRelativeSymlinkSafe. This function seems to make other symlink
  // related safety checks redundant, but for now we prefer to keep them too.
  //
  // 2022.12.01: the performance impact is minimized after adding the check
  // against the previous path and enabling this verification only after
  // extracting a symlink with ".." in target. So we enabled it for Windows
  // as well for extra safety.
//#ifdef _UNIX
  std::wstring Path=SrcName;

  size_t SkipLength=SkipPart.size();

  if (SkipLength>0 && Path.rfind(SkipPart,0)!=0)
    SkipLength=0; // Parameter validation, not really needed now.

  // Do not check parts already checked in previous path to improve performance.
  for (size_t I=0;I<Path.size() && I<LastChecked.size() && Path[I]==LastChecked[I];I++)
    if (IsPathDiv(Path[I]) && I>SkipLength)
      SkipLength=I;

  // Avoid converting symlinks in destination path part specified by user.
  while (SkipLength<Path.size() && IsPathDiv(Path[SkipLength]))
    SkipLength++;

  if (Path.size()>0)
    for (size_t I=Path.size()-1;I>SkipLength;I--)
      if (IsPathDiv(Path[I]))
      {
        Path.erase(I);
        FindData FD;
        if (FindFile::FastFind(Path,&FD,true) && FD.IsLink)
        {
#ifdef _WIN_ALL
          // Normally Windows symlinks to directory look like a directory
          // and are deleted with DelDir(). It is possible to create
          // a file-like symlink pointing at directory, which can be deleted
          // only with  && DelFile, but such symlink isn't really functional.
          // Here we prefer to fail deleting such symlink and skip extracting
          // a file.
          if (!DelDir(Path))
#else
          if (!DelFile(Path))
#endif
          {
            ErrHandler.CreateErrorMsg(SrcName); // Extraction command will skip this file or directory.
            return false; // Couldn't delete the symlink to replace it with directory.
          }
        }
      }
  LastChecked=SrcName;
//#endif
  return true;
}
