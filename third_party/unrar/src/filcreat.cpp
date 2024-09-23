#include "rar.hpp"

// If NewFile==NULL, we delete created file after user confirmation.
// It is useful if we need to overwrite an existing folder or file,
// but need user confirmation for that.
bool FileCreate(CommandData *Cmd,File *NewFile,std::wstring &Name,
                bool *UserReject,int64 FileSize,RarTime *FileTime,bool WriteOnly)
{
  if (UserReject!=NULL)
    *UserReject=false;
#ifdef _WIN_ALL
  bool ShortNameChanged=false;
#endif
  while (FileExist(Name))
  {
#if defined(_WIN_ALL)
    if (!ShortNameChanged)
    {
      // Avoid the infinite loop if UpdateExistingShortName returns
      // the same name.
      ShortNameChanged=true;

      // Maybe our long name matches the short name of existing file.
      // Let's check if we can change the short name.
      if (UpdateExistingShortName(Name))
        continue;
    }
    // Allow short name check again. It is necessary, because rename and
    // autorename below can change the name, so we need to check it again.
    ShortNameChanged=false;
#endif
    UIASKREP_RESULT Choice=uiAskReplaceEx(Cmd,Name,FileSize,FileTime,(NewFile==NULL ? UIASKREP_F_NORENAME:0));

    if (Choice==UIASKREP_R_REPLACE)
      break;
    if (Choice==UIASKREP_R_SKIP)
    {
      if (UserReject!=NULL)
        *UserReject=true;
      return false;
    }
    if (Choice==UIASKREP_R_CANCEL)
      ErrHandler.Exit(RARX_USERBREAK);
  }

  // Try to truncate the existing file first instead of delete,
  // so we preserve existing file permissions, such as NTFS permissions,
  // also as "Compressed" attribute and hard links. In GUI version we avoid
  // deleting an existing file for non-.rar archive formats as well.
  uint FileMode=WriteOnly ? FMF_WRITE|FMF_SHAREREAD:FMF_UPDATE|FMF_SHAREREAD;
  if (NewFile!=NULL && NewFile->Create(Name,FileMode))
    return true;

  CreatePath(Name,true,Cmd->DisableNames);
  return NewFile!=NULL ? NewFile->Create(Name,FileMode):DelFile(Name);
}


#if defined(_WIN_ALL)
// If we find a file, which short name is equal to 'Name', we try to change
// its short name, while preserving the long name. It helps when unpacking
// an archived file, which long name is equal to short name of already
// existing file. Otherwise we would overwrite the already existing file,
// even though its long name does not match the name of unpacking file.
bool UpdateExistingShortName(const std::wstring &Name)
{
  DWORD Res=GetLongPathName(Name.c_str(),NULL,0);
  if (Res==0)
    return false;
  std::vector<wchar> LongPathBuf(Res);
  Res=GetLongPathName(Name.c_str(),LongPathBuf.data(),(DWORD)LongPathBuf.size());
  if (Res==0 || Res>=LongPathBuf.size())
    return false;
  Res=GetShortPathName(Name.c_str(),NULL,0);
  if (Res==0)
    return false;
  std::vector<wchar> ShortPathBuf(Res);
  Res=GetShortPathName(Name.c_str(),ShortPathBuf.data(),(DWORD)ShortPathBuf.size());
  if (Res==0 || Res>=ShortPathBuf.size())
    return false;
  std::wstring LongPathName=LongPathBuf.data();
  std::wstring ShortPathName=ShortPathBuf.data();
  
  std::wstring LongName=PointToName(LongPathName);
  std::wstring ShortName=PointToName(ShortPathName);

  // We continue only if file has a short name, which does not match its
  // long name, and this short name is equal to name of file which we need
  // to create.
  if (ShortName.empty() || wcsicomp(LongName,ShortName)==0 ||
      wcsicomp(PointToName(Name),ShortName)!=0)
    return false;

  // Generate the temporary new name for existing file.
  std::wstring NewName;
  for (uint I=0;I<10000 && NewName.empty();I+=123)
  {
    // Here we copy the path part of file to create. We'll make the temporary
    // file in the same folder.
    NewName=Name;

    // Here we set the random name part.
    SetName(NewName,std::wstring(L"rtmp") + std::to_wstring(I));
    
    // If such file is already exist, try next random name.
    if (FileExist(NewName))
      NewName.clear();
  }

  // If we could not generate the name not used by any other file, we return.
  if (NewName.empty())
    return false;
  
  // FastFind returns the name without path, but we need the fully qualified
  // name for renaming, so we use the path from file to create and long name
  // from existing file.
  std::wstring FullName=Name;
  SetName(FullName,LongName);
  
  // Rename the existing file to randomly generated name. Normally it changes
  // the short name too.
  if (!MoveFile(FullName.c_str(),NewName.c_str()))
    return false;

  // Now we need to create the temporary empty file with same name as
  // short name of our already existing file. We do it to occupy its previous
  // short name and not allow to use it again when renaming the file back to
  // its original long name.
  File KeepShortFile;
  bool Created=false;
  if (!FileExist(Name))
    Created=KeepShortFile.Create(Name,FMF_WRITE|FMF_SHAREREAD);

  // Now we rename the existing file from temporary name to original long name.
  // Since its previous short name is occupied by another file, it should
  // get another short name.
  MoveFile(NewName.c_str(),FullName.c_str());

  if (Created)
  {
    // Delete the temporary zero length file occupying the short name,
    KeepShortFile.Close();
    KeepShortFile.Delete();
  }
  // We successfully changed the short name. We do not use the simpler
  // SetFileShortName Windows API call, because it requires SE_RESTORE_NAME
  // privilege.
  return true;
}
#endif
