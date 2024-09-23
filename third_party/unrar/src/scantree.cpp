#include "rar.hpp"

ScanTree::ScanTree(StringList *FileMasks,RECURSE_MODE Recurse,bool GetLinks,SCAN_DIRS GetDirs)
{
  ScanTree::FileMasks=FileMasks;
  ScanTree::Recurse=Recurse;
  ScanTree::GetLinks=GetLinks;
  ScanTree::GetDirs=GetDirs;

  ScanEntireDisk=false;
  FolderWildcards=false;

  FindStack.push_back(NULL); // We need a single NULL pointer for initial Depth==0.
  
  SetAllMaskDepth=0;
  Depth=0;
  Errors=0;
  Cmd=NULL;
  ErrDirList=NULL;
  ErrDirSpecPathLength=NULL;
}


ScanTree::~ScanTree()
{
  for (int I=Depth;I>=0;I--)
    if (FindStack[I]!=NULL)
      delete FindStack[I];
}


SCAN_CODE ScanTree::GetNext(FindData *FD)
{
  if (Depth<0)
    return SCAN_DONE;

#ifndef SILENT
  uint LoopCount=0;
#endif

  SCAN_CODE FindCode;
  while (1)
  {
    if (CurMask.empty() && !GetNextMask())
      return SCAN_DONE;

#ifndef SILENT
    // Let's return some ticks to system or WinRAR can become irresponsible
    // while scanning files in command like "winrar a -r arc c:\file.ext".
    // Also we reset system sleep timer here.
    if ((++LoopCount & 0x3ff)==0)
      Wait();
#endif

    FindCode=FindProc(FD);
    if (FindCode==SCAN_ERROR)
    {
      Errors++;
      continue;
    }
    if (FindCode==SCAN_NEXT)
      continue;
    if (FindCode==SCAN_SUCCESS && FD->IsDir && GetDirs==SCAN_SKIPDIRS)
      continue;
    if (FindCode==SCAN_DONE && GetNextMask())
      continue;
    if (FilterList.ItemsCount()>0 && FindCode==SCAN_SUCCESS)
      if (!CommandData::CheckArgs(&FilterList,FD->IsDir,FD->Name,false,MATCH_WILDSUBPATH))
        continue;
    break;
  }
  return FindCode;
}


// For masks like dir1\dir2*\*.ext in non-recursive mode.
bool ScanTree::ExpandFolderMask()
{
  bool WildcardFound=false;
  uint SlashPos=0;
  for (uint I=0;I<CurMask.size();I++)
  {
    if (CurMask[I]=='?' || CurMask[I]=='*')
      WildcardFound=true;
    if (WildcardFound && IsPathDiv(CurMask[I]))
    {
      // First path separator position after folder wildcard mask.
      // In case of dir1\dir2*\dir3\name.ext mask it may point not to file
      // name, so we cannot use PointToName() here.
      SlashPos=I; 
      break;
    }
  }

  std::wstring Mask=CurMask.substr(0,SlashPos);

  // Prepare the list of all folders matching the wildcard mask.
  ExpandedFolderList.Reset();
  FindFile Find;
  Find.SetMask(Mask);
  FindData FD;
  while (Find.Next(&FD))
    if (FD.IsDir)
    {
      FD.Name+=CurMask.substr(SlashPos);

      // Treat dir*\*, dir*\*.* or dir*\ as dir, so empty 'dir' is also matched
      // by such mask. Skipping empty dir with dir*\*.* confused some users.
      std::wstring LastMask=PointToName(FD.Name);
      if (LastMask==L"*" || LastMask==L"*.*" || LastMask.empty())
        RemoveNameFromPath(FD.Name);

      ExpandedFolderList.AddString(FD.Name);
    }
  if (ExpandedFolderList.ItemsCount()==0)
    return false;
  // Return the first matching folder name now.
  ExpandedFolderList.GetString(CurMask);
  return true;
}


// For masks like dir1\dir2*\file.ext this function sets 'dir1' recursive mask
// and '*\dir2*\file.ext' filter. Masks without folder wildcards are
// returned as is.
bool ScanTree::GetFilteredMask()
{
  // If we have some matching folders left for non-recursive folder wildcard
  // mask, we return it here.
  if (ExpandedFolderList.ItemsCount()>0 && ExpandedFolderList.GetString(CurMask))
    return true;

  FolderWildcards=false;
  FilterList.Reset();
  if (!FileMasks->GetString(CurMask))
    return false;

  // Check if folder wildcards present.
  bool WildcardFound=false;
  uint FolderWildcardCount=0;
  uint SlashPos=0;
  uint StartPos=0;
#ifdef _WIN_ALL // Not treat the special NTFS \\?\d: path prefix as a wildcard.
  if (CurMask.rfind(L"\\\\?\\",0)==0)
    StartPos=4;
#endif
  for (uint I=StartPos;I<CurMask.size();I++)
  {
    if (CurMask[I]=='?' || CurMask[I]=='*')
      WildcardFound=true;
    if (IsPathDiv(CurMask[I]) || IsDriveDiv(CurMask[I]))
    {
      if (WildcardFound)
      {
        // Calculate a number of folder wildcards in current mask.
        FolderWildcardCount++;
        WildcardFound=false;
      }
      if (FolderWildcardCount==0)
        SlashPos=I; // Slash position before first folder wildcard mask.
    }
  }
  if (FolderWildcardCount==0)
    return true;
  FolderWildcards=true; // Global folder wildcards flag.

  // If we have only one folder wildcard component and -r is missing or -r-
  // is specified, prepare matching folders in non-recursive mode.
  // We assume -r for masks like dir1*\dir2*\file*, because it is complicated
  // to fast find them using OS file find API call.
  if ((Recurse==RECURSE_NONE || Recurse==RECURSE_DISABLE) && FolderWildcardCount==1)
    return ExpandFolderMask();

  // Convert path\dir*\ to *\dir filter to search for 'dir' in all 'path' subfolders.
  std::wstring Filter=L"*";
  AddEndSlash(Filter); // Path separator is OS dependent, so we set it here instead of variable declaration.

  // SlashPos might point or not point to path separator for masks like 'dir*', '\dir*' or 'd:dir*'
  std::wstring WildName=IsPathDiv(CurMask[SlashPos]) || IsDriveDiv(CurMask[SlashPos]) ? CurMask.substr(SlashPos+1) : CurMask.substr(SlashPos);
  Filter+=WildName;

  // Treat dir*\* or dir*\*.* as dir\, so empty 'dir' is also matched
  // by such mask. Skipping empty dir with dir*\*.* confused some users.
  std::wstring LastMask=PointToName(Filter);
  if (LastMask==L"*" || LastMask==L"*.*")
    GetPathWithSep(Filter,Filter);

  FilterList.AddString(Filter);

  bool RelativeDrive=IsDriveDiv(CurMask[SlashPos]);
  if (RelativeDrive)
    SlashPos++; // Use "d:" instead of "d" for d:* mask.

  CurMask.erase(SlashPos);

  if (!RelativeDrive) // Keep d: mask as is, not convert to d:\*
  {
    // We need to append "\*" both for -ep1 to work correctly and to
    // convert d:\* masks previously truncated to d: back to original form.
    AddEndSlash(CurMask);
    CurMask+=MASKALL;
  }
  return true;
}


bool ScanTree::GetNextMask()
{
  if (!GetFilteredMask())
    return false;
#ifdef _WIN_ALL
  UnixSlashToDos(CurMask,CurMask);
#endif

  // We prefer to scan entire disk if mask like \\server\share\ or c:\
  // is specified regardless of recursion mode. Use \\server\share\*.*
  // or c:\*.* mask to scan only the root directory.
  if (CurMask.size()>2 && CurMask[0]=='\\' && CurMask[1]=='\\')
  {
    auto Slash=CurMask.find('\\',2);
    if (Slash!=std::wstring::npos)
    {
      Slash=CurMask.find('\\',Slash+1);
      // If backslash is found and it is the last string character.
      ScanEntireDisk=Slash!=std::wstring::npos && Slash+1==CurMask.size();
    }
  }
  else
    ScanEntireDisk=IsDriveLetter(CurMask) && IsPathDiv(CurMask[2]) && CurMask[3]==0;


  auto NamePos=GetNamePos(CurMask);
  std::wstring Name=CurMask.substr(NamePos);
  if (Name.empty())
    CurMask+=MASKALL;
  if (Name==L"." || Name==L"..")
  {
    AddEndSlash(CurMask);
    CurMask+=MASKALL;
  }
  SpecPathLength=NamePos;
  Depth=0;

  OrigCurMask=CurMask;

  return true;
}


SCAN_CODE ScanTree::FindProc(FindData *FD)
{
  if (CurMask.empty())
    return SCAN_NEXT;
  bool FastFindFile=false;
  
  if (FindStack[Depth]==NULL) // No FindFile object for this depth yet.
  {
    bool Wildcards=IsWildcard(CurMask);

    // If we have a file name without wildcards, we can try to use
    // FastFind to optimize speed. For example, in Unix it results in
    // stat call instead of opendir/readdir/closedir.
    bool FindCode=!Wildcards && FindFile::FastFind(CurMask,FD,GetLinks);

    // Link check is important for NTFS, where links can have "Directory"
    // attribute, but we do not want to recurse to them in "get links" mode.
    bool IsDir=FindCode && FD->IsDir && (!GetLinks || !FD->IsLink);

    // SearchAll means that we'll use "*" mask for search, so we'll find
    // subdirectories and will be able to recurse into them.
    // We do not use "*" for directories at any level or for files
    // at top level in recursion mode. We always comrpess the entire directory
    // if folder wildcard is specified.
    bool SearchAll=!IsDir && (Depth>0 || Recurse==RECURSE_ALWAYS ||
                   FolderWildcards && Recurse!=RECURSE_DISABLE || 
                   Wildcards && Recurse==RECURSE_WILDCARDS || 
                   ScanEntireDisk && Recurse!=RECURSE_DISABLE);
    if (Depth==0)
      SearchAllInRoot=SearchAll;
    if (SearchAll || Wildcards)
    {
      // Create the new FindFile object for wildcard based search.
      FindStack[Depth]=new FindFile;

      std::wstring SearchMask=CurMask;
      if (SearchAll)
        SetName(SearchMask,MASKALL);
      FindStack[Depth]->SetMask(SearchMask);
    }
    else
    {
      // Either we failed to fast find or we found a file or we found
      // a directory in RECURSE_DISABLE mode, so we do not need to scan it.
      // We can return here and do not need to process further.
      // We need to process further only if we fast found a directory.
      if (!FindCode || !IsDir || Recurse==RECURSE_DISABLE)
      {
         // Return SCAN_SUCCESS if we found a file.
        SCAN_CODE RetCode=SCAN_SUCCESS;

        if (!FindCode)
        {
          // Return SCAN_ERROR if problem is more serious than just
          // "file not found".
          RetCode=FD->Error ? SCAN_ERROR:SCAN_NEXT;

          // If we failed to find an object, but our current mask is excluded,
          // we skip this object and avoid indicating an error.
          if (Cmd!=NULL && Cmd->ExclCheck(CurMask,false,true,true))
            RetCode=SCAN_NEXT;
          else
          {
            ErrHandler.OpenErrorMsg(ErrArcName,CurMask);
            // User asked to return RARX_NOFILES and not RARX_OPEN here.
            ErrHandler.SetErrorCode(RARX_NOFILES);
          }
        }

        // If we searched only for one file or directory in "fast find" 
        // (without a wildcard) mode, let's set masks to zero, 
        // so calling function will know that current mask is used 
        // and next one must be read from mask list for next call.
        // It is not necessary for directories, because even in "fast find"
        // mode, directory recursing will quit by (Depth < 0) condition,
        // which returns SCAN_DONE to calling function.
        CurMask.clear();

        return RetCode;
      }

      // We found a directory using only FindFile::FastFind function.
      FastFindFile=true;
    }
  }

  if (!FastFindFile && !FindStack[Depth]->Next(FD,GetLinks))
  {
    // We cannot find anything more in directory either because of
    // some error or just as result of all directory entries already read.

    bool Error=FD->Error;
    if (Error)
      ScanError(Error);

    // Going to at least one directory level higher.
    delete FindStack[Depth];
    FindStack[Depth--]=NULL;
    while (Depth>=0 && FindStack[Depth]==NULL)
      Depth--;
    if (Depth < 0)
    {
      // Directories scanned both in normal and FastFindFile mode,
      // finally exit from scan here, by (Depth < 0) condition.

      if (Error)
        Errors++;
      return SCAN_DONE;
    }

    auto Slash=CurMask.rfind(CPATHDIVIDER);
    if (Slash!=std::wstring::npos)
    {
      std::wstring Mask;
      Mask=CurMask.substr(Slash); // Name mask with leading slash like \*.*
      if (Depth<SetAllMaskDepth)
        Mask.replace(1, std::wstring::npos, PointToName(OrigCurMask));
      CurMask.erase(Slash);

      std::wstring DirName=CurMask;

      auto PrevSlash=CurMask.rfind(CPATHDIVIDER);
      if (PrevSlash==std::wstring::npos)
        CurMask=Mask.substr(1); // Set to name only without leading slash.
      else
      {
        CurMask.erase(PrevSlash); // Remove one of two sequential slashes.
        CurMask+=Mask;
      }

      if (GetDirs==SCAN_GETDIRSTWICE &&
          FindFile::FastFind(DirName,FD,GetLinks) && FD->IsDir)
      {
        FD->Flags|=FDDF_SECONDDIR;
        return Error ? SCAN_ERROR:SCAN_SUCCESS;
      }
    }
    return Error ? SCAN_ERROR:SCAN_NEXT;
  }

  // Link check is required for NTFS links, not for Unix.
  if (FD->IsDir && (!GetLinks || !FD->IsLink))
  {
    // If we found the directory in top (Depth==0) directory
    // and if we are not in "fast find" (directory name only as argument)
    // or in recurse (SearchAll was set when opening the top directory) mode,
    // we do not recurse into this directory. We either return it by itself
    // or skip it.
    if (!FastFindFile && Depth==0 && !SearchAllInRoot)
      return GetDirs==SCAN_GETCURDIRS ? SCAN_SUCCESS:SCAN_NEXT;

    // Let's check if directory name is excluded, so we do not waste
    // time searching in directory, which will be excluded anyway.
    if (Cmd!=NULL && (Cmd->ExclCheck(FD->Name,true,false,false) ||
        Cmd->ExclDirByAttr(FD->FileAttr)))
    {
      // If we are here in "fast find" mode, it means that entire directory
      // specified in command line is excluded. Then we need to return
      // SCAN_DONE to go to next mask and avoid the infinite loop
      // in GetNext() function. Such loop would be possible in case of
      // SCAN_NEXT code and "rar a arc dir -xdir" command.

      return FastFindFile ? SCAN_DONE:SCAN_NEXT;
    }
    
    std::wstring Mask=FastFindFile ? MASKALL:PointToName(CurMask);
    CurMask=FD->Name;

    if (CurMask.size()+Mask.size()+1>=MAXPATHSIZE || Depth>=MAXSCANDEPTH-1)
    {
      uiMsg(UIERROR_PATHTOOLONG,CurMask,SPATHDIVIDER,Mask);
      return SCAN_ERROR;
    }

    AddEndSlash(CurMask);
    CurMask+=Mask;

    Depth++;

    FindStack.resize(Depth+1);

    // We need to use OrigCurMask for depths less than SetAllMaskDepth
    // and "*" for depths equal or larger than SetAllMaskDepth.
    // It is important when "fast finding" directories at Depth > 0.
    // For example, if current directory is RootFolder and we compress
    // the following directories structure:
    //   RootFolder
    //     +--Folder1
    //     |  +--Folder2
    //     |  +--Folder3
    //     +--Folder4
    // with 'rar a -r arcname Folder2' command, rar could add not only
    // Folder1\Folder2 contents, but also Folder1\Folder3 if we were using
    // "*" mask at all levels. We need to use "*" mask inside of Folder2,
    // but return to "Folder2" mask when completing scanning Folder2.
    // We can rewrite SearchAll expression above to avoid fast finding
    // directories at Depth > 0, but then 'rar a -r arcname Folder2'
    // will add the empty Folder2 and do not add its contents.

    if (FastFindFile)
      SetAllMaskDepth=Depth;
  }
  if (!FastFindFile && !CmpName(CurMask,FD->Name,MATCH_NAMES))
    return SCAN_NEXT;

  return SCAN_SUCCESS;
}


void ScanTree::ScanError(bool &Error)
{
#ifdef _WIN_ALL
  if (Error)
  {
    // Get attributes of parent folder and do not display an error
    // if it is reparse point. We cannot scan contents of standard
    // Windows reparse points like "C:\Documents and Settings"
    // and we do not want to issue numerous useless errors for them.
    // We cannot just check FD->FileAttr here, it can be undefined
    // if we process "folder\*" mask or if we process "folder" mask,
    // but "folder" is inaccessible.
    auto Slash=GetNamePos(CurMask);
    if (Slash>1)
    {
      std::wstring Parent=CurMask.substr(0,Slash-1);
      DWORD Attr=GetFileAttr(Parent);
      if (Attr!=0xffffffff && (Attr & FILE_ATTRIBUTE_REPARSE_POINT)!=0)
        Error=false;
    }

    // Do not display an error if we cannot scan contents of
    // "System Volume Information" folder. Normally it is not accessible.
    if (CurMask.find(L"System Volume Information\\")!=std::wstring::npos)
      Error=false;
  }
#endif

  if (Error && Cmd!=NULL && Cmd->ExclCheck(CurMask,false,true,true))
    Error=false;

  if (Error)
  {
    if (ErrDirList!=NULL)
      ErrDirList->AddString(CurMask);
    if (ErrDirSpecPathLength!=NULL)
      ErrDirSpecPathLength->push_back((uint)SpecPathLength);
    std::wstring FullName;
    // This conversion works for wildcard masks too.
    ConvertNameToFull(CurMask,FullName);
    uiMsg(UIERROR_DIRSCAN,FullName);
    ErrHandler.SysErrMsg();
  }
}
