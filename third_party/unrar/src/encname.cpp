#include "rar.hpp"

EncodeFileName::EncodeFileName()
{
  Flags=0;
  FlagBits=0;
  FlagsPos=0;
  DestSize=0;
}




void EncodeFileName::Decode(const char *Name,size_t NameSize,
                            const byte *EncName,size_t EncSize,
                            std::wstring &NameW)
{
  size_t EncPos=0,DecPos=0;
  byte HighByte=EncPos<EncSize ? EncName[EncPos++] : 0;
  while (EncPos<EncSize)
  {
    if (FlagBits==0)
    {
      Flags=EncName[EncPos++];
      FlagBits=8;
    }
    switch(Flags>>6)
    {
      case 0:
        if (EncPos>=EncSize)
          break;
        // We need DecPos also for ASCII "Name", so resize() instead of push_back().
        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos++];
        break;
      case 1:
        if (EncPos>=EncSize)
          break;
        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos++]+(HighByte<<8);
        break;
      case 2:
        if (EncPos+1>=EncSize)
          break;
        NameW.resize(DecPos+1);
        NameW[DecPos++]=EncName[EncPos]+(EncName[EncPos+1]<<8);
        EncPos+=2;
        break;
      case 3:
        {
          if (EncPos>=EncSize)
            break;
          int Length=EncName[EncPos++];
          if ((Length & 0x80)!=0)
          {
            if (EncPos>=EncSize)
              break;
            byte Correction=EncName[EncPos++];
            for (Length=(Length&0x7f)+2;Length>0 && DecPos<NameSize;Length--,DecPos++)
            {
              NameW.resize(DecPos+1);
              NameW[DecPos]=((Name[DecPos]+Correction)&0xff)+(HighByte<<8);
            }
          }
          else
            for (Length+=2;Length>0 && DecPos<NameSize;Length--,DecPos++)
            {
              NameW.resize(DecPos+1);
              NameW[DecPos]=Name[DecPos];
            }
        }
        break;
    }
    Flags<<=2;
    FlagBits-=2;
  }
}
