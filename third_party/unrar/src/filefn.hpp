#ifndef _RAR_FILEFN_
#define _RAR_FILEFN_

enum MKDIR_CODE {MKDIR_SUCCESS,MKDIR_ERROR,MKDIR_BADPATH};

MKDIR_CODE MakeDir(const std::wstring &Name,bool SetAttr,uint Attr);
bool CreateDir(const std::wstring &Name);
bool CreatePath(const std::wstring &Path,bool SkipLastName,bool Silent);
void SetDirTime(const std::wstring &Name,RarTime *ftm,RarTime *ftc,RarTime *fta);
bool IsRemovable(const std::wstring &Name);

#ifndef SFX_MODULE
int64 GetFreeDisk(const std::wstring &Name);
#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
bool IsFAT(const std::wstring &Root);
#endif

bool FileExist(const std::wstring &Name);
bool WildFileExist(const std::wstring &Name);
bool IsDir(uint Attr);
bool IsUnreadable(uint Attr);
bool IsLink(uint Attr);
void SetSFXMode(const std::wstring &FileName);
void EraseDiskContents(const std::wstring &FileName);
bool IsDeleteAllowed(uint FileAttr);
void PrepareToDelete(const std::wstring &Name);
uint GetFileAttr(const std::wstring &Name);
bool SetFileAttr(const std::wstring &Name,uint Attr);
wchar* MkTemp(wchar *Name,size_t MaxSize);
bool MkTemp(std::wstring &Name);

enum CALCFSUM_FLAGS {CALCFSUM_SHOWTEXT=1,CALCFSUM_SHOWPERCENT=2,CALCFSUM_SHOWPROGRESS=4,CALCFSUM_CURPOS=8};

void CalcFileSum(File *SrcFile,uint *CRC32,byte *Blake2,uint Threads,int64 Size=INT64NDF,uint Flags=0);

bool RenameFile(const std::wstring &SrcName,const std::wstring &DestName);
bool DelFile(const std::wstring &Name);
bool DelDir(const std::wstring &Name);

#if defined(_WIN_ALL) && !defined(SFX_MODULE)
bool SetFileCompression(const std::wstring &Name,bool State);
void ResetFileCache(const std::wstring &Name);
#endif





// Keep it here and not in extinfo.cpp, because it is invoked from Zip.SFX too.
bool LinksToDirs(const std::wstring &SrcName,const std::wstring &SkipPart,std::wstring &LastChecked);

#endif
