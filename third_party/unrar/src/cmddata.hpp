#ifndef _RAR_CMDDATA_
#define _RAR_CMDDATA_


#define DefaultStoreList L"7z;ace;arj;bz2;cab;gz;jpeg;jpg;lha;lz;lzh;mp3;rar;taz;tbz;tbz2;tgz;txz;xz;z;zip;zipx;zst;tzst"

enum RAR_CMD_LIST_MODE {RCLM_AUTO,RCLM_REJECT_LISTS,RCLM_ACCEPT_LISTS};

enum IS_PROCESS_FILE_FLAGS {IPFF_EXCLUDE_PARENT=1};

class CommandData:public RAROptions
{
  private:
    void ProcessSwitch(const wchar *Switch);
    void BadSwitch(const wchar *Switch);
    uint GetExclAttr(const wchar *Str,bool &Dir);
#if !defined(SFX_MODULE)
    void SetTimeFilters(const wchar *Mod,bool Before,bool Age);
    void SetStoreTimeMode(const wchar *S);
#endif
    int64 GetVolSize(const wchar *S,uint DefMultiplier);

    bool FileLists;
    bool NoMoreSwitches;
    RAR_CMD_LIST_MODE ListMode;
    bool BareOutput;
  public:
    CommandData();
    void Init();

    void ParseCommandLine(bool Preprocess,int argc, char *argv[]);
    void ParseArg(const wchar *ArgW);
    void ParseDone();
    void ParseEnvVar();
    void ReadConfig();
    void PreprocessArg(const wchar *Arg);
    void ProcessSwitchesString(const std::wstring &Str);
    void OutTitle();
    void OutHelp(RAR_EXIT ExitCode);
    bool IsSwitch(int Ch);
    bool ExclCheck(const std::wstring &CheckName,bool Dir,bool CheckFullPath,bool CheckInclList);
    static bool CheckArgs(StringList *Args,bool Dir,const std::wstring &CheckName,bool CheckFullPath,int MatchMode);
    bool ExclDirByAttr(uint FileAttr);
    bool TimeCheck(RarTime &ftm,RarTime &ftc,RarTime &fta);
    bool SizeCheck(int64 Size);
    bool AnyFiltersActive();
    int IsProcessFile(FileHeader &FileHead,bool *ExactMatch,int MatchType,
                      bool Flags,std::wstring *MatchedArg);
    void ProcessCommand();
    void AddArcName(const std::wstring &Name);
    bool GetArcName(wchar *Name,int MaxSize);
    bool GetArcName(std::wstring &Name);


#ifndef SFX_MODULE
    void ReportWrongSwitches(RARFORMAT Format);
#endif


    std::wstring Command;
    std::wstring ArcName;
    std::wstring ExtrPath;
    std::wstring TempPath;
    std::wstring SFXModule;
    std::wstring CommentFile;
    std::wstring ArcPath; // For -ap<path>.
    std::wstring ExclArcPath; // For -ep4<path> switch.
    std::wstring LogName;
    std::wstring EmailTo;

    // Read data from stdin and store in archive under a name specified here
    // when archiving. Read an archive from stdin if any non-empty string
    // is specified here when extracting.
    std::wstring UseStdin;

    StringList FileArgs;
    StringList ExclArgs;
    StringList InclArgs;
    StringList ArcNames;
    StringList StoreArgs;

    SecPassword Password;

    std::vector<int64> NextVolSizes;


#ifdef RARDLL
    std::wstring DllDestName;
#endif
};

#endif
