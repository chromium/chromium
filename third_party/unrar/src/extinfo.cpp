#include "rar.hpp"

#include "hardlinks.cpp"
#include "win32stm.cpp"

#ifdef _WIN_ALL
#include "win32acl.cpp"
#include "win32lnk.cpp"
#endif

#ifdef _UNIX
#include "uowners.cpp"
#ifdef SAVE_LINKS
#include "ulinks.cpp"
#endif
#endif



// RAR2 service header extra records.
#ifndef SFX_MODULE
void SetExtraInfo20(CommandData *Cmd,Archive &Arc,wchar *Name)
{
#ifdef _WIN_ALL
  if (Cmd->Test)
    return;
  switch(Arc.SubBlockHead.SubType)
  {
    case NTACL_HEAD:
      if (Cmd->ProcessOwners)
        ExtractACL20(Arc,Name);
      break;
    case STREAM_HEAD:
      ExtractStreams20(Arc,Name);
      break;
  }
#endif
}
#endif


// RAR3 and RAR5 service header extra records.
void SetExtraInfo(CommandData *Cmd,Archive &Arc,wchar *Name)
{
#ifdef _UNIX
  if (!Cmd->Test && Cmd->ProcessOwners && Arc.Format==RARFMT15 &&
      Arc.SubHead.CmpName(SUBHEAD_TYPE_UOWNER))
    ExtractUnixOwner30(Arc,Name);
#endif
#ifdef _WIN_ALL
  if (!Cmd->Test && Cmd->ProcessOwners && Arc.SubHead.CmpName(SUBHEAD_TYPE_ACL))
    ExtractACL(Arc,Name);
  if (Arc.SubHead.CmpName(SUBHEAD_TYPE_STREAM))
    ExtractStreams(Arc,Name,Cmd->Test);
#endif
}


// Extra data stored directly in file header.
void SetFileHeaderExtra(CommandData *Cmd,Archive &Arc,wchar *Name)
{
#ifdef _UNIX
   if (Cmd->ProcessOwners && Arc.Format==RARFMT50 && Arc.FileHead.UnixOwnerSet)
     SetUnixOwner(Arc,Name);
#endif
}




// Calculate a number of path components except \. and \..
static int CalcAllowedDepth(const wchar *Name)
{
  int AllowedDepth=0;
  while (*Name!=0)
  {
    if (IsPathDiv(Name[0]) && Name[1]!=0 && !IsPathDiv(Name[1]))
    {
      bool Dot=Name[1]=='.' && (IsPathDiv(Name[2]) || Name[2]==0);
      bool Dot2=Name[1]=='.' && Name[2]=='.' && (IsPathDiv(Name[3]) || Name[3]==0);
      if (!Dot && !Dot2)
        AllowedDepth++;
      else
        if (Dot2)
          AllowedDepth--;
    }
    Name++;
  }
  return AllowedDepth < 0 ? 0 : AllowedDepth;
}


// Check if all existing path components are directories and not links.
static bool LinkInPath(const wchar *Name)
{
  wchar Path[NM];
  if (wcslen(Name)>=ASIZE(Path))
    return true;  // It should not be that long, skip.
  wcsncpyz(Path,Name,ASIZE(Path));
  for (wchar *s=Path+wcslen(Path)-1;s>Path;s--)
    if (IsPathDiv(*s))
    {
      *s=0;
      FindData FD;
      if (FindFile::FastFind(Path,&FD,true) && (FD.IsLink || !FD.IsDir))
        return true;
    }
  return false;
}


bool IsRelativeSymlinkSafe(CommandData *Cmd,const wchar *SrcName,const wchar *PrepSrcName,const wchar *TargetName)
{
  // Catch root dir based /path/file paths also as stuff like \\?\.
  // Do not check PrepSrcName here, it can be root based if destination path
  // is a root based.
  if (IsFullRootPath(SrcName) || IsFullRootPath(TargetName))
    return false;

  // Number of ".." in link target.
  int UpLevels=0;
  for (int Pos=0;*TargetName!=0;Pos++)
  {
    bool Dot2=TargetName[0]=='.' && TargetName[1]=='.' && 
              (IsPathDiv(TargetName[2]) || TargetName[2]==0) &&
              (Pos==0 || IsPathDiv(*(TargetName-1)));
    if (Dot2)
      UpLevels++;
    TargetName++;
  }
  // If link target includes "..", it must not have another links in its
  // source path, because they can bypass our safety check. For example,
  // suppose we extracted "lnk1" -> "." first and "lnk1/lnk2" -> ".." next
  // or "dir/lnk1" -> ".." first, "dir/lnk1/lnk2" -> ".." next and
  // file "dir/lnk1/lnk2/poc.txt" last.
  // Do not confuse with link chains in target, this is in link source path.
  // It is important for Windows too, though this check can be omitted
  // if LinksToDirs is invoked in Windows as well.
  if (UpLevels>0 && LinkInPath(PrepSrcName))
    return false;
    
  // We could check just prepared src name, but for extra safety
  // we check both original (as from archive header) and prepared
  // (after applying the destination path and -ep switches) names.

  int AllowedDepth=CalcAllowedDepth(SrcName); // Original name depth.

  // Remove the destination path from prepared name if any. We should not
  // count the destination path depth, because the link target must point
  // inside of this path, not outside of it.
  size_t ExtrPathLength=wcslen(Cmd->ExtrPath);
  if (ExtrPathLength>0 && wcsncmp(PrepSrcName,Cmd->ExtrPath,ExtrPathLength)==0)
  {
    PrepSrcName+=ExtrPathLength;
    while (IsPathDiv(*PrepSrcName))
      PrepSrcName++;
  }
  int PrepAllowedDepth=CalcAllowedDepth(PrepSrcName);

  return AllowedDepth>=UpLevels && PrepAllowedDepth>=UpLevels;
}


bool ExtractSymlink(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,const wchar *LinkName,bool &UpLink)
{
  // Returning true in Uplink indicates that link target might include ".."
  // and enables additional checks. It is ok to falsely return true here,
  // as it implies only the minor performance penalty. But we shall always
  // return true for links with ".." in target for security reason.

  UpLink=true; // Assume the target might include potentially unsafe "..".
#if defined(SAVE_LINKS) && defined(_UNIX) || defined(_WIN_ALL)
  if (Arc.Format==RARFMT50) // For RAR5 archives we can check RedirName for both Unix and Windows.
    UpLink=wcsstr(Arc.FileHead.RedirName,L"..")!=NULL;
#endif

#if defined(SAVE_LINKS) && defined(_UNIX)
  // For RAR 3.x archives we process links even in test mode to skip link data.
  if (Arc.Format==RARFMT15)
    return ExtractUnixLink30(Cmd,DataIO,Arc,LinkName,UpLink);
  if (Arc.Format==RARFMT50)
    return ExtractUnixLink50(Cmd,LinkName,&Arc.FileHead);
#elif defined(_WIN_ALL)
  // RAR 5.0 archives store link information in file header, so there is
  // no need to additionally test it if we do not create a file.
  if (Arc.Format==RARFMT50)
    return CreateReparsePoint(Cmd,LinkName,&Arc.FileHead);
#endif
  return false;
}
