#ifndef _RAR_ENCNAME_
#define _RAR_ENCNAME_

class EncodeFileName
{
  private:
    void AddFlags(byte Value,std::vector<byte> &EncName);

    byte Flags;
    uint FlagBits;
    size_t FlagsPos;
    size_t DestSize;
  public:
    EncodeFileName();
    void Encode(const std::string &Name,const std::wstring &NameW,std::vector<byte> &EncName);
    void Decode(const char *Name,size_t NameSize,const byte *EncName,size_t EncSize,std::wstring &NameW);
};

#endif
