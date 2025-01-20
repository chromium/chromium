#ifndef _RAR_MOTW_
#define _RAR_MOTW_

class MarkOfTheWeb
{
  private:
    const size_t MOTW_STREAM_MAX_SIZE=1024;
    const wchar* MOTW_STREAM_NAME=L":Zone.Identifier"; // Must start from ':'.

    int ParseZoneIdStream(std::string &Stream);

    std::string ZoneIdStream; // Store archive ":Zone.Identifier" NTFS stream data.
    int ZoneIdValue; // -1 if missing.
    bool AllFields; // Copy all MOTW fields or ZoneId only.
  public:
    MarkOfTheWeb();
    void Clear();
    void ReadZoneIdStream(const std::wstring &FileName,bool AllFields);
    void CreateZoneIdStream(const std::wstring &Name,StringList &MotwList);
    bool IsNameConflicting(const std::wstring &StreamName);
    bool IsFileStreamMoreSecure(std::string &FileStream);
};

#endif


