#ifndef _RAR_FILECREATE_
#define _RAR_FILECREATE_

bool FileCreate(CommandData *Cmd,File *NewFile,std::wstring &Name,
                bool *UserReject,int64 FileSize=INT64NDF,
                RarTime *FileTime=NULL,bool WriteOnly=false);

#if defined(_WIN_ALL)
bool UpdateExistingShortName(const std::wstring &Name);
#endif

#endif
