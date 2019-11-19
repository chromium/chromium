#ifndef _RAR_ARCMEM_
#define _RAR_ARCMEM_

// Memory interface for software fuzzers.

class ArcMemory
{
  private:
    bool Loaded;
    Array<byte> ArcData;
    uint64 SeekPos;
  public:
    ArcMemory();
    void Load(const byte *Data,size_t Size);
    bool Unload();
    bool IsLoaded() {return Loaded;}
    bool Read(void *Data,size_t Size,size_t &Result);
    bool Seek(int64 Offset,int Method);
    bool Tell(int64 *Pos);
};

#endif
