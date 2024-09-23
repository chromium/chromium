bool ExtractHardlink(CommandData *Cmd,const std::wstring &NameNew,const std::wstring &NameExisting)
{
  if (!FileExist(NameExisting))
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    uiMsg(UIERROR_NOLINKTARGET);
    ErrHandler.SetErrorCode(RARX_CREATE);
    return false;
  }
  CreatePath(NameNew,true,Cmd->DisableNames);

#ifdef _WIN_ALL
  bool Success=CreateHardLink(NameNew.c_str(),NameExisting.c_str(),NULL)!=0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#elif defined(_UNIX)
  std::string NameExistingA,NameNewA;
  WideToChar(NameExisting,NameExistingA);
  WideToChar(NameNew,NameNewA);
  bool Success=link(NameExistingA.c_str(),NameNewA.c_str())==0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#else
  return false;
#endif
}

