

static bool UnixSymlink(CommandData *Cmd,const std::string &Target,const wchar *LinkName,RarTime *ftm,RarTime *fta)
{
  CreatePath(LinkName,true,Cmd->DisableNames);

  // Overwrite prompt was already issued and confirmed earlier, so we can
  // remove existing symlink or regular file here. PrepareToDelete was also
  // called earlier inside of uiAskReplaceEx.
  DelFile(LinkName);

  std::string LinkNameA;
  WideToChar(LinkName,LinkNameA);
  if (symlink(Target.c_str(),LinkNameA.c_str())==-1) // Error.
  {
    if (errno==EEXIST)
      uiMsg(UIERROR_ULINKEXIST,LinkName);
    else
    {
      uiMsg(UIERROR_SLINKCREATE,L"",LinkName);
      ErrHandler.SetErrorCode(RARX_WARNING);
    }
    return false;
  }
#ifdef USE_LUTIMES
#ifdef UNIX_TIME_NS
  timespec times[2];
  times[0].tv_sec=fta->GetUnix();
  times[0].tv_nsec=fta->IsSet() ? long(fta->GetUnixNS()%1000000000) : UTIME_NOW;
  times[1].tv_sec=ftm->GetUnix();
  times[1].tv_nsec=ftm->IsSet() ? long(ftm->GetUnixNS()%1000000000) : UTIME_NOW;
  utimensat(AT_FDCWD,LinkNameA.c_str(),times,AT_SYMLINK_NOFOLLOW);
#else
  struct timeval tv[2];
  tv[0].tv_sec=fta->GetUnix();
  tv[0].tv_usec=long(fta->GetUnixNS()%1000000000/1000);
  tv[1].tv_sec=ftm->GetUnix();
  tv[1].tv_usec=long(ftm->GetUnixNS()%1000000000/1000);
  lutimes(LinkNameA.c_str(),tv);
#endif
#endif

  return true;
}


static bool IsFullPath(const char *PathA) // Unix ASCII version.
{
  return *PathA==CPATHDIVIDER;
}


// For security purpose we prefer to be sure that CharToWide completed
// successfully and even if it truncated a string for some reason,
// it didn't affect the number of path related characters we analyze
// in IsRelativeSymlinkSafe later.
// This check is likely to be excessive, but let's keep it anyway.
static bool SafeCharToWide(const std::string &Src,std::wstring &Dest)
{
  if (!CharToWide(Src,Dest) || Dest.empty())
    return false;
  uint SrcChars=0,DestChars=0;
  for (uint I=0;Src[I]!=0;I++)
    if (Src[I]=='/' || Src[I]=='.')
      SrcChars++;
  for (uint I=0;Dest[I]!=0;I++)
    if (Dest[I]=='/' || Dest[I]=='.')
      DestChars++;
  return SrcChars==DestChars;
}


static bool ExtractUnixLink30(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,
                              const wchar *LinkName,bool &UpLink)
{
  if (IsLink(Arc.FileHead.FileAttr))
  {
    size_t DataSize=(size_t)Arc.FileHead.PackSize;
    if (DataSize>MAXPATHSIZE)
      return false;
    std::vector<char> TargetBuf(DataSize+1);
    if ((size_t)DataIO.UnpRead((byte*)TargetBuf.data(),DataSize)!=DataSize)
      return false;
    std::string Target(TargetBuf.data(),TargetBuf.size());

    DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,1);
    DataIO.UnpHash.Update(Target.data(),strlen(Target.data()));
    DataIO.UnpHash.Result(&Arc.FileHead.FileHash);

    // Return true in case of bad checksum, so link will be processed further
    // and extraction routine will report the checksum error.
    if (!DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL))
      return true;

    std::wstring TargetW;
    if (!SafeCharToWide(Target.data(),TargetW))
      return false;
    TruncateAtZero(TargetW);
    // Use Arc.FileHead.FileName instead of LinkName, since LinkName
    // can include the destination path as a prefix, which can
    // confuse IsRelativeSymlinkSafe algorithm.
    if (!Cmd->AbsoluteLinks && (IsFullPath(TargetW) ||
        !IsRelativeSymlinkSafe(Cmd,Arc.FileHead.FileName.c_str(),LinkName,TargetW.c_str())))
    {
      uiMsg(UIERROR_SKIPUNSAFELINK,Arc.FileHead.FileName,TargetW);
      ErrHandler.SetErrorCode(RARX_WARNING);
      return false;
    }
    UpLink=Target.find("..")!=std::string::npos;
    return UnixSymlink(Cmd,Target,LinkName,&Arc.FileHead.mtime,&Arc.FileHead.atime);
  }
  return false;
}


static bool ExtractUnixLink50(CommandData *Cmd,const wchar *Name,FileHeader *hd)
{
  std::string Target;
  WideToChar(hd->RedirName,Target);
  if (hd->RedirType==FSREDIR_WINSYMLINK || hd->RedirType==FSREDIR_JUNCTION)
  {
    // Cannot create Windows absolute path symlinks in Unix. Only relative path
    // Windows symlinks can be created here. RAR 5.0 used \??\ prefix
    // for Windows absolute symlinks, since RAR 5.1 /??/ is used.
    // We escape ? as \? to avoid "trigraph" warning
    if (Target.rfind("\\??\\",0)!=std::string::npos || 
        Target.rfind("/\?\?/",0)!=std::string::npos)
    {
      uiMsg(UIERROR_SLINKCREATE,nullptr,L"\"" + hd->FileName + L"\" -> \"" + hd->RedirName + L"\"");
      ErrHandler.SetErrorCode(RARX_WARNING);
      return false;
    }
    DosSlashToUnix(Target,Target);
  }

  std::wstring TargetW;
  if (!SafeCharToWide(Target,TargetW))
    return false;
  // Use hd->FileName instead of LinkName, since LinkName can include
  // the destination path as a prefix, which can confuse
  // IsRelativeSymlinkSafe algorithm.
  if (!Cmd->AbsoluteLinks && (IsFullPath(TargetW) ||
      !IsRelativeSymlinkSafe(Cmd,hd->FileName.c_str(),Name,TargetW.c_str())))
  {
    uiMsg(UIERROR_SKIPUNSAFELINK,hd->FileName,TargetW);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return false;
  }
  return UnixSymlink(Cmd,Target,Name,&hd->mtime,&hd->atime);
}
