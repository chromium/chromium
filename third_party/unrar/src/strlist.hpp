#ifndef _RAR_STRLIST_
#define _RAR_STRLIST_

class StringList
{
  private:
    std::vector<wchar> StringData;
    size_t CurPos;

    size_t StringsCount;

    size_t SaveCurPos[16],SavePosNumber;
  public:
    StringList();
    void Reset();
//    void AddStringA(const char *Str);
    void AddString(const wchar *Str);
    void AddString(const std::wstring &Str);
//    bool GetStringA(char *Str,size_t MaxLength);
    bool GetString(wchar *Str,size_t MaxLength);
    bool GetString(std::wstring &Str);
    bool GetString(wchar *Str,size_t MaxLength,int StringNum);
    bool GetString(std::wstring &Str,int StringNum);
    wchar* GetString();
    bool GetString(wchar **Str);
    void Rewind();
    size_t ItemsCount() {return StringsCount;};
    size_t GetCharCount() {return StringData.size();}
    bool Search(const std::wstring &Str,bool CaseSensitive);
    void SavePosition();
    void RestorePosition();
};

#endif
