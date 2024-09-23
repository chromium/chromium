#ifndef _RAR_ERRHANDLER_
#define _RAR_ERRHANDLER_

enum RAR_EXIT // RAR exit code.
{ 
  RARX_SUCCESS   =   0,
  RARX_WARNING   =   1,
  RARX_FATAL     =   2,
  RARX_CRC       =   3,
  RARX_LOCK      =   4,
  RARX_WRITE     =   5,
  RARX_OPEN      =   6,
  RARX_USERERROR =   7,
  RARX_MEMORY    =   8,
  RARX_CREATE    =   9,
  RARX_NOFILES   =  10,
  RARX_BADPWD    =  11,
  RARX_READ      =  12,
  RARX_USERBREAK = 255
};


class ErrorHandler
{
  private:
    RAR_EXIT ExitCode = RARX_SUCCESS;
    uint ErrCount = 0;
    bool EnableBreak = true;
    bool Silent = false;
    bool DisableShutdown = false; // Shutdown is not suitable after last error.
    bool ReadErrIgnoreAll = false;
  public:
    void Clean();
    void MemoryError();
    void OpenError(const std::wstring &FileName);
    void CloseError(const std::wstring &FileName);
    void ReadError(const std::wstring &FileName);
    void AskRepeatRead(const std::wstring &FileName,bool &Ignore,bool &Retry,bool &Quit);
    void WriteError(const std::wstring &ArcName,const std::wstring &FileName);
    void WriteErrorFAT(const std::wstring &FileName);
    bool AskRepeatWrite(const std::wstring &FileName,bool DiskFull);
    void SeekError(const std::wstring &FileName);
    void GeneralErrMsg(const wchar *fmt,...);
    void MemoryErrorMsg();
    void OpenErrorMsg(const std::wstring &FileName);
    void OpenErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void CreateErrorMsg(const std::wstring &FileName);
    void CreateErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void ReadErrorMsg(const std::wstring &FileName);
    void ReadErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void WriteErrorMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void ArcBrokenMsg(const std::wstring &ArcName);
    void ChecksumFailedMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void UnknownMethodMsg(const std::wstring &ArcName,const std::wstring &FileName);
    void Exit(RAR_EXIT ExitCode);
    void SetErrorCode(RAR_EXIT Code);
    RAR_EXIT GetErrorCode() {return ExitCode;}
    uint GetErrorCount() {return ErrCount;}
    void SetSignalHandlers(bool Enable);
    void Throw(RAR_EXIT Code);
    void SetSilent(bool Mode) {Silent=Mode;}
    bool GetSysErrMsg(std::wstring &Msg);
    void SysErrMsg();
    int GetSystemErrorCode();
    void SetSystemErrorCode(int Code);
    void SetDisableShutdown() {DisableShutdown=true;}
    bool IsShutdownEnabled() {return !DisableShutdown;}

    bool UserBreak = false; // Ctrl+Break is pressed.
    bool MainExit = false; // main() is completed.
};


#endif
