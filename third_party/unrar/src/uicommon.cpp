static bool GetAutoRenamedName(std::wstring &Name);
static SOUND_NOTIFY_MODE uiSoundNotify;

void uiInit(SOUND_NOTIFY_MODE Sound)
{
  uiSoundNotify = Sound;
}


// Additionally to handling user input, it analyzes and sets command options.
// Returns only 'replace', 'skip' and 'cancel' codes.
UIASKREP_RESULT uiAskReplaceEx(CommandData *Cmd,std::wstring &Name,int64 FileSize,RarTime *FileTime,uint Flags)
{
  if (Cmd->Overwrite==OVERWRITE_NONE)
    return UIASKREP_R_SKIP;

#if !defined(SFX_MODULE) && !defined(SILENT)
  // Must be before Cmd->AllYes check or -y switch would override -or.
  if (Cmd->Overwrite==OVERWRITE_AUTORENAME && GetAutoRenamedName(Name))
    return UIASKREP_R_REPLACE;
#endif

  std::wstring NewName=Name;
  UIASKREP_RESULT Choice=Cmd->AllYes || Cmd->Overwrite==OVERWRITE_ALL ? 
                  UIASKREP_R_REPLACE : uiAskReplace(NewName,FileSize,FileTime,Flags);

  if (Choice==UIASKREP_R_REPLACE || Choice==UIASKREP_R_REPLACEALL)
  {
    PrepareToDelete(Name);

    // Overwrite the link itself instead of its target.
    // For normal files we prefer to inherit file attributes, permissions
    // and hard links.
    FindData FD;
    if (FindFile::FastFind(Name,&FD,true) && FD.IsLink)
      DelFile(Name);
  }

  if (Choice==UIASKREP_R_REPLACEALL)
  {
    Cmd->Overwrite=OVERWRITE_ALL;
    return UIASKREP_R_REPLACE;
  }
  if (Choice==UIASKREP_R_SKIPALL)
  {
    Cmd->Overwrite=OVERWRITE_NONE;
    return UIASKREP_R_SKIP;
  }
  if (Choice==UIASKREP_R_RENAME)
  {
    if (GetNamePos(NewName)==0)
      SetName(Name,NewName);
    else
      Name=NewName;
    if (FileExist(Name))
      return uiAskReplaceEx(Cmd,Name,FileSize,FileTime,Flags);
    return UIASKREP_R_REPLACE;
  }
#if !defined(SFX_MODULE) && !defined(SILENT)
  if (Choice==UIASKREP_R_RENAMEAUTO && GetAutoRenamedName(Name))
  {
    Cmd->Overwrite=OVERWRITE_AUTORENAME;
    return UIASKREP_R_REPLACE;
  }
#endif
  return Choice;
}


bool GetAutoRenamedName(std::wstring &Name)
{
  std::wstring Ext=GetExt(Name);
  for (uint FileVer=1;FileVer<1000000;FileVer++)
  {
    std::wstring NewName=Name;
    RemoveExt(NewName);
    wchar Ver[10];
    itoa(FileVer,Ver,ASIZE(Ver));
    NewName = NewName + L"(" + Ver + L")" + Ext;
    if (!FileExist(NewName))
    {
      Name=NewName;
      return true;
    }
  }
  return false;
}
