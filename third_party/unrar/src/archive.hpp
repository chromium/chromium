#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

class PPack;
class RawRead;
class RawWrite;

enum NOMODIFY_FLAGS 
{
  NMDF_ALLOWLOCK=1,NMDF_ALLOWANYVOLUME=2,NMDF_ALLOWFIRSTVOLUME=4
};

enum RARFORMAT {RARFMT_NONE,RARFMT14,RARFMT15,RARFMT50,RARFMT_FUTURE};

enum ADDSUBDATA_FLAGS
{
  ASDF_SPLIT          = 1, // Allow to split archive just before header if necessary.
  ASDF_COMPRESS       = 2, // Allow to compress data following subheader.
  ASDF_CRYPT          = 4, // Encrypt data after subheader if password is set.
  ASDF_CRYPTIFHEADERS = 8  // Encrypt data after subheader only in -hp mode.
};

// RAR5 headers must not exceed 2 MB.
#define MAX_HEADER_SIZE_RAR5 0x200000

class Archive:public File
{
  private:
    void UpdateLatestTime(FileHeader *CurBlock);
    void ConvertNameCase(std::wstring &Name);
    void ConvertFileHeader(FileHeader *hd);
    size_t ReadHeader14();
    size_t ReadHeader15();
    size_t ReadHeader50();
    void ProcessExtra50(RawRead *Raw,size_t ExtraSize,const BaseBlock *bb);
    void RequestArcPassword(RarCheckPassword *SelPwd);
    void UnexpEndArcMsg();
    void BrokenHeaderMsg();
    void UnkEncVerMsg(const std::wstring &Name,const std::wstring &Info);
    bool DoGetComment(std::wstring &CmtData);
    bool ReadCommentData(std::wstring &CmtData);

#if !defined(RAR_NOCRYPT)
    CryptData HeadersCrypt;
#endif
    ComprDataIO SubDataIO;
    bool DummyCmd;
    CommandData *Cmd;


    RarTime LatestTime;
    int LastReadBlock;
    HEADER_TYPE CurHeaderType;

    bool SilentOpen;
#ifdef USE_QOPEN
    QuickOpen QOpen;
    bool ProhibitQOpen;
#endif

#if defined(CHROMIUM_UNRAR)
    // A handle for a temporary file that should be used when extracting the
    // archive. This is used to extract the contents while in a sandbox.
    FileHandle hTempFile;
#endif

  public:
    Archive(CommandData *InitCmd=NULL);
    ~Archive();
    static RARFORMAT IsSignature(const byte *D,size_t Size);
    bool IsArchive(bool EnableBroken);
    size_t SearchBlock(HEADER_TYPE HeaderType);
    size_t SearchSubBlock(const wchar *Type);
    size_t SearchRR();
    size_t ReadHeader();
    void CheckArc(bool EnableBroken);
    void CheckOpen(const std::wstring &Name);
    bool WCheckOpen(const std::wstring &Name);
    bool GetComment(std::wstring &CmtData);
    void ViewComment();
    void SetLatestTime(RarTime *NewTime);
    void SeekToNext();
    bool CheckAccess();
    bool IsArcDir();
    void ConvertAttributes();
    void VolSubtractHeaderSize(size_t SubSize);
    uint FullHeaderSize(size_t Size);
    int64 GetStartPos();
    void AddSubData(const byte *SrcData,uint64 DataSize,File *SrcFile,
         const wchar *Name,uint Flags);
    bool ReadSubData(std::vector<byte> *UnpData,File *DestFile,bool TestMode);
    HEADER_TYPE GetHeaderType() {return CurHeaderType;}
    CommandData* GetCommandData() {return Cmd;}
    void SetSilentOpen(bool Mode) {SilentOpen=Mode;}
#ifdef USE_QOPEN
    bool Open(const std::wstring &Name,uint Mode=FMF_READ) override;
    int Read(void *Data,size_t Size) override;
    void Seek(int64 Offset,int Method) override;
    int64 Tell() override;
    void QOpenUnload() {QOpen.Unload();}
    void SetProhibitQOpen(bool Mode) {ProhibitQOpen=Mode;}
#endif
#if defined(CHROMIUM_UNRAR)
    void SetTempFileHandle(FileHandle hF);
    FileHandle GetTempFileHandle();
#endif
    static uint64 GetWinSize(uint64 Size,uint &Flags);

    // Needed to see wstring based Open from File. Otherwise compiler finds
    // Open in Archive and doesn't check the base class overloads.
    using File::Open;

    BaseBlock ShortBlock;
    MarkHeader MarkHead;
    MainHeader MainHead;
    CryptHeader CryptHead;
    FileHeader FileHead;
    EndArcHeader EndArcHead;
    SubBlockHeader SubBlockHead;
    FileHeader SubHead;
    CommentHeader CommHead;
    ProtectHeader ProtectHead;
    EAHeader EAHead;
    StreamHeader StreamHead;

    int64 CurBlockPos;
    int64 NextBlockPos;

    RARFORMAT Format;
    bool Solid;
    bool Volume;
    bool MainComment;
    bool Locked;
    bool Signed;
    bool FirstVolume;
    bool NewNumbering;
    bool Protected;
    bool Encrypted;
    size_t SFXSize;
    bool BrokenHeader;
    bool FailedHeaderDecryption;

#if !defined(RAR_NOCRYPT)
    byte ArcSalt[SIZE_SALT50];
#endif

    bool Splitting;

    uint VolNumber;
    int64 VolWrite;

    // Total size of files adding to archive. Might also include the size of
    // files repacked in solid archive.
    uint64 AddingFilesSize;

    uint64 AddingHeadersSize;

    bool NewArchive;

    std::wstring FirstVolumeName;
};


#endif
