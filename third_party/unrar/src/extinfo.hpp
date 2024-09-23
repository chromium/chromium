#ifndef _RAR_EXTINFO_
#define _RAR_EXTINFO_

bool IsRelativeSymlinkSafe(CommandData *Cmd,const std::wstring &SrcName,std::wstring PrepSrcName,const std::wstring &TargetName);
bool ExtractSymlink(CommandData *Cmd,ComprDataIO &DataIO,Archive &Arc,const std::wstring &LinkName,bool &UpLink);
#ifdef _UNIX
void SetUnixOwner(Archive &Arc,const std::wstring &FileName);
#endif

bool ExtractHardlink(CommandData *Cmd,const std::wstring &NameNew,const std::wstring &NameExisting);

std::wstring GetStreamNameNTFS(Archive &Arc);

#ifdef _WIN_ALL
bool SetPrivilege(LPCTSTR PrivName);
#endif

void SetExtraInfo20(CommandData *Cmd,Archive &Arc,const std::wstring &Name);
void SetExtraInfo(CommandData *Cmd,Archive &Arc,const std::wstring &Name);
void SetFileHeaderExtra(CommandData *Cmd,Archive &Arc,const std::wstring &Name);


#endif
