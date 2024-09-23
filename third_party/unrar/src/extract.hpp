#ifndef _RAR_EXTRACT_
#define _RAR_EXTRACT_

enum EXTRACT_ARC_CODE {EXTRACT_ARC_NEXT,EXTRACT_ARC_REPEAT};

class CmdExtract
{
  private:
    struct ExtractRef
    {
      std::wstring RefName;
      std::wstring TmpName;
      uint64 RefCount;
    };
    std::vector<ExtractRef> RefList;

    struct AnalyzeData
    {
      std::wstring StartName;
      uint64 StartPos;
      std::wstring EndName;
      uint64 EndPos;
    } Analyze;

    bool ArcAnalyzed;

    void FreeAnalyzeData();
    EXTRACT_ARC_CODE ExtractArchive();
    bool ExtractFileCopy(File &New,const std::wstring &ArcName,const std::wstring &RedirName,const std::wstring &NameNew,const std::wstring &NameExisting,int64 UnpSize);
    void ExtrPrepareName(Archive &Arc,const std::wstring &ArcFileName,std::wstring &DestName);
#ifdef RARDLL
    bool ExtrDllGetPassword();
#else
    bool ExtrGetPassword(Archive &Arc,const std::wstring &ArcFileName,RarCheckPassword *CheckPwd);
#endif
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    void ConvertDosPassword(Archive &Arc,SecPassword &DestPwd);
#endif
    void ExtrCreateDir(Archive &Arc,const std::wstring &ArcFileName);
    bool ExtrCreateFile(Archive &Arc,File &CurFile);
    bool CheckUnpVer(Archive &Arc,const std::wstring &ArcFileName);
#ifndef SFX_MODULE
    void AnalyzeArchive(const std::wstring &ArcName,bool Volume,bool NewNumbering);
    void GetFirstVolIfFullSet(const std::wstring &SrcName,bool NewNumbering,std::wstring &DestName);
#endif
    bool CheckWinLimit(Archive &Arc,std::wstring &ArcFileName);

    RarTime StartTime; // Time when extraction started.

    CommandData *Cmd;

    ComprDataIO DataIO;
    Unpack *Unp;
    unsigned long TotalFileCount;

    unsigned long FileCount;
    unsigned long MatchedArgs;
    bool FirstFile;
    bool AllMatchesExact;
    bool ReconstructDone=false;
    bool UseExactVolName=false;

    // If any non-zero solid file was successfully unpacked before current.
    // If true and if current encrypted file is broken, obviously
    // the password is correct and we can report broken CRC without
    // any wrong password hints.
    bool AnySolidDataUnpackedWell;

    std::wstring ArcName;

    bool GlobalPassword;
    bool PrevProcessed; // If previous file was successfully extracted or tested.
    std::wstring DestFileName;
    bool SuppressNoFilesMessage;

    // In Windows it is set to true if at least one symlink with ".."
    // in target was extracted.
    bool ConvertSymlinkPaths;

    // Last path checked for symlinks. We use it to improve the performance,
    // so we do not check recently checked folders again.
    std::wstring LastCheckedSymlink;

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
    bool Fat32,NotFat32;
#endif
  public:
    CmdExtract(CommandData *Cmd);
    ~CmdExtract();
    void DoExtract();
    void ExtractArchiveInit(Archive &Arc);
    bool ExtractCurrentFile(Archive &Arc,size_t HeaderSize,bool &Repeat);
    static void UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize);

#if defined(CHROMIUM_UNRAR)
    int64 GetCurrentFileSize() { return DataIO.CurUnpWrite; }
    bool IsMissingNextVolume() { return DataIO.NextVolumeMissing; }
#endif
};

#endif
