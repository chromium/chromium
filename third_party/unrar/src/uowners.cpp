

void ExtractUnixOwner30(Archive &Arc,const wchar *FileName)
{
  // There must be 0 byte between owner and group strings.
  // Otherwise strlen call below wouldn't be safe.
  if (memchr(Arc.SubHead.SubData.data(),0,Arc.SubHead.SubData.size())==NULL)
    return;

  char *OwnerName=(char *)Arc.SubHead.SubData.data();
  int OwnerSize=strlen(OwnerName)+1;
  int GroupSize=Arc.SubHead.SubData.size()-OwnerSize;
  char *GroupName=(char *)&Arc.SubHead.SubData[OwnerSize];
  std::string GroupStr(GroupName,GroupName+GroupSize);

  struct passwd *pw;
  if ((pw=getpwnam(OwnerName))==NULL)
  {
    uiMsg(UIERROR_UOWNERGETOWNERID,Arc.FileName,GetWide(OwnerName));
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }
  uid_t OwnerID=pw->pw_uid;

  struct group *gr;
  if ((gr=getgrnam(GroupStr.c_str()))==NULL)
  {
    uiMsg(UIERROR_UOWNERGETGROUPID,Arc.FileName,GetWide(GroupName));
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }
  uint Attr=GetFileAttr(FileName);
  gid_t GroupID=gr->gr_gid;

  std::string NameA;
  WideToChar(FileName,NameA);

#if defined(SAVE_LINKS) && !defined(_APPLE)
  if (lchown(NameA.c_str(),OwnerID,GroupID)!=0)
#else
  if (chown(NameA.c_str(),OwnerID,GroupID)!=0)
#endif
  {
    uiMsg(UIERROR_UOWNERSET,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  SetFileAttr(FileName,Attr);
}


void SetUnixOwner(Archive &Arc,const std::wstring &FileName)
{
  // First, we try to resolve symbolic names. If they are missing or cannot
  // be resolved, we try to use numeric values if any. If numeric values
  // are missing too, function fails.
  FileHeader &hd=Arc.FileHead;
  if (*hd.UnixOwnerName!=0)
  {
    struct passwd *pw;
    if ((pw=getpwnam(hd.UnixOwnerName))==NULL)
    {
      if (!hd.UnixOwnerNumeric)
      {
        uiMsg(UIERROR_UOWNERGETOWNERID,Arc.FileName,GetWide(hd.UnixOwnerName));
        ErrHandler.SetErrorCode(RARX_WARNING);
        return;
      }
    }
    else
      hd.UnixOwnerID=pw->pw_uid;
  }
  if (*hd.UnixGroupName!=0)
  {
    struct group *gr;
    if ((gr=getgrnam(hd.UnixGroupName))==NULL)
    {
      if (!hd.UnixGroupNumeric)
      {
        uiMsg(UIERROR_UOWNERGETGROUPID,Arc.FileName,GetWide(hd.UnixGroupName));
        ErrHandler.SetErrorCode(RARX_WARNING);
        return;
      }
    }
    else
      hd.UnixGroupID=gr->gr_gid;
  }

  std::string NameA;
  WideToChar(FileName,NameA);

#if defined(SAVE_LINKS) && !defined(_APPLE)
  if (lchown(NameA.c_str(),hd.UnixOwnerID,hd.UnixGroupID)!=0)
#else
  if (chown(NameA.c_str(),hd.UnixOwnerID,hd.UnixGroupID)!=0)
#endif
  {
    uiMsg(UIERROR_UOWNERSET,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
}
