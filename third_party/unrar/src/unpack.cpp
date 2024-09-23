// NOTE(vakh): The process.h file needs to be included first because "rar.hpp"
// defines certain macros that cause symbol redefinition errors
#if defined(UNRAR_NO_EXCEPTIONS)
#include "base/process/memory.h"
#endif  // defined(UNRAR_NO_EXCEPTIONS)

#include "rar.hpp"

#include "coder.cpp"
#include "suballoc.cpp"
#include "model.cpp"
#include "unpackinline.cpp"
#ifdef RAR_SMP
#include "unpack50mt.cpp"
#endif
#ifndef SFX_MODULE
#include "unpack15.cpp"
#include "unpack20.cpp"
#include "unpack30.cpp"
#endif
#include "unpack50.cpp"
#include "unpack50frag.cpp"

#if defined(UNRAR_NO_EXCEPTIONS)
#define UNRAR_FATAL_BAD_ALLOC(size) base::TerminateBecauseOutOfMemory(size)
#else
#define UNRAR_FATAL_BAD_ALLOC(size) throw std::bad_alloc()
#endif

Unpack::Unpack(ComprDataIO *DataIO)
:Inp(true),VMCodeInp(true)
{
  UnpIO=DataIO;
  Window=NULL;
  Fragmented=false;
  Suspended=false;
  UnpAllBuf=false;
  UnpSomeRead=false;
  ExtraDist=false;
#ifdef RAR_SMP
  MaxUserThreads=1;
  UnpThreadPool=NULL;
  ReadBufMT=NULL;
  UnpThreadData=NULL;
#endif
  AllocWinSize=0;
  MaxWinSize=0;
  MaxWinMask=0;

  // Perform initialization, which should be done only once for all files.
  // It prevents crash if first unpacked file has the wrong "true" Solid flag,
  // so first DoUnpack call is made with the wrong "true" Solid value later.
  UnpInitData(false);
#ifndef SFX_MODULE
  // RAR 1.5 decompression initialization
  UnpInitData15(false);
  InitHuff();
#endif
}


Unpack::~Unpack()
{
#ifndef SFX_MODULE
  InitFilters30(false);
#endif

  free(Window);
#ifdef RAR_SMP
  delete UnpThreadPool;
  delete[] ReadBufMT;
  delete[] UnpThreadData;
#endif
}


#ifdef RAR_SMP
void Unpack::SetThreads(uint Threads)
{
  // More than 8 threads are unlikely to provide noticeable gain
  // for unpacking, but would use the additional memory.
  MaxUserThreads=Min(Threads,8);
  UnpThreadPool=new ThreadPool(MaxUserThreads);
}
#endif


// We get 64-bit WinSize, so we still can check and quit for dictionaries
// exceeding a threshold in 32-bit builds. Then we convert WinSize to size_t
// MaxWinSize.
void Unpack::Init(uint64 WinSize,bool Solid)
{
  // Minimum window size must be at least twice more than maximum possible
  // size of filter block, which is 0x10000 in RAR now. If window size is
  // smaller, we can have a block with never cleared flt->NextWindow flag
  // in UnpWriteBuf(). Minimum window size 0x20000 would be enough, but let's
  // use 0x40000 for extra safety and possible filter area size expansion.
  const size_t MinAllocSize=0x40000;
  if (WinSize<MinAllocSize)
    WinSize=MinAllocSize;

  if (WinSize>Min(0x10000000000ULL,UNPACK_MAX_DICT)) // Window size must not exceed 1 TB.
    UNRAR_FATAL_BAD_ALLOC(WinSize);

  // 32-bit build can't unpack dictionaries exceeding 32-bit even in theory.
  // Also we've not verified if WrapUp and WrapDown work properly in 32-bit
  // version and >2GB dictionary and if 32-bit version can handle >2GB
  // distances. Since such version is unlikely to allocate >2GB anyway,
  // we prohibit >2GB dictionaries for 32-bit build here.
  if (WinSize>0x80000000 && sizeof(size_t)<=4)
    UNRAR_FATAL_BAD_ALLOC(WinSize);

  // Solid block shall use the same window size for all files.
  // But if Window isn't initialized when Solid is set, it means that
  // first file in solid block doesn't have the solid flag. We initialize
  // the window anyway for such malformed archive.
  // Non-solid files shall use their specific window sizes,
  // so current window size and unpack routine behavior doesn't depend on
  // previously unpacked files and their extraction order.
  if (!Solid || Window==nullptr)
  {
    MaxWinSize=(size_t)WinSize;
    MaxWinMask=MaxWinSize-1;
  }

  // Use the already allocated window when processing non-solid files
  // with reducing dictionary sizes.
  if (WinSize<=AllocWinSize)
    return;

  // Archiving code guarantees that window size does not grow in the same
  // solid stream. So if we are here, we are either creating a new window
  // or increasing the size of non-solid window. So we could safely reject
  // current window data without copying them to a new window.
  if (Solid && (Window!=NULL || Fragmented && WinSize>FragWindow.GetWinSize()))
    UNRAR_FATAL_BAD_ALLOC(WinSize);

  free(Window);
  
  Window=Fragmented ? NULL : (byte *)malloc((size_t)WinSize);

  if (Window==NULL)
    // Exclude RAR4, small dictionaries and 64-bit.
    if (WinSize<0x1000000 || sizeof(size_t)>4)
      UNRAR_FATAL_BAD_ALLOC(WinSize);
    else
    {
      if (WinSize>FragWindow.GetWinSize())
        FragWindow.Init((size_t)WinSize);
      Fragmented=true;
    }

  if (!Fragmented)
  {
    // Clean the window to generate the same output when unpacking corrupt
    // RAR files, which may access unused areas of sliding dictionary.
    // 2023.10.31: We've added FirstWinDone based unused area access check
    // in Unpack::CopyString(), so this memset might be unnecessary now.
//    memset(Window,0,(size_t)WinSize);

    AllocWinSize=WinSize;
  }
}


void Unpack::DoUnpack(uint Method,bool Solid)
{
  // Methods <50 will crash in Fragmented mode when accessing NULL Window.
  // They cannot be called in such mode now, but we check it below anyway
  // just for extra safety.
  switch(Method)
  {
#ifndef SFX_MODULE
    case 15: // RAR 1.5 compression.
      if (!Fragmented)
        Unpack15(Solid);
      break;
    case 20: // RAR 2.x compression.
    case 26: // Files larger than 2GB.
      if (!Fragmented)
        Unpack20(Solid);
      break;
    case 29: // RAR 3.x compression.
      if (!Fragmented)
        Unpack29(Solid);
      break;
#endif
    case VER_PACK5: // 50. RAR 5.0 and 7.0 compression algorithms.
    case VER_PACK7: // 70.
      ExtraDist=(Method==VER_PACK7);
#ifdef RAR_SMP
      if (MaxUserThreads>1)
      {
//      We do not use the multithreaded unpack routine to repack RAR archives
//      in 'suspended' mode, because unlike the single threaded code it can
//      write more than one dictionary for same loop pass. So we would need
//      larger buffers of unknown size. Also we do not support multithreading
//      in fragmented window mode.
          if (!Fragmented)
          {
            Unpack5MT(Solid);
            break;
          }
      }
#endif
      Unpack5(Solid);
      break;
  }
}


void Unpack::UnpInitData(bool Solid)
{
  if (!Solid)
  {
    OldDist[0]=OldDist[1]=OldDist[2]=OldDist[3]=(size_t)-1;

    OldDistPtr=0;

    LastDist=(uint)-1; // Initialize it to -1 like LastDist.
    LastLength=0;

//    memset(Window,0,MaxWinSize);
    memset(&BlockTables,0,sizeof(BlockTables));
    UnpPtr=WrPtr=0;
    PrevPtr=0;
    FirstWinDone=false;
    WriteBorder=Min(MaxWinSize,UNPACK_MAX_WRITE);
  }
  // Filters never share several solid files, so we can safely reset them
  // even in solid archive.
  InitFilters();

  Inp.InitBitInput();
  WrittenFileSize=0;
  ReadTop=0;
  ReadBorder=0;

  memset(&BlockHeader,0,sizeof(BlockHeader));
  BlockHeader.BlockSize=-1;  // '-1' means not defined yet.
#ifndef SFX_MODULE
  UnpInitData20(Solid);
  UnpInitData30(Solid);
#endif
  UnpInitData50(Solid);
}


// LengthTable contains the length in bits for every element of alphabet.
// Dec is the structure to decode Huffman code/
// Size is size of length table and DecodeNum field in Dec structure,
void Unpack::MakeDecodeTables(byte *LengthTable,DecodeTable *Dec,uint Size)
{
  // Size of alphabet and DecodePos array.
  Dec->MaxNum=Size;

  // Calculate how many entries for every bit length in LengthTable we have.
  uint LengthCount[16];
  memset(LengthCount,0,sizeof(LengthCount));
  for (size_t I=0;I<Size;I++)
    LengthCount[LengthTable[I] & 0xf]++;

  // We must not calculate the number of zero length codes.
  LengthCount[0]=0;

  // Set the entire DecodeNum to zero.
  memset(Dec->DecodeNum,0,Size*sizeof(*Dec->DecodeNum));

  // Initialize not really used entry for zero length code.
  Dec->DecodePos[0]=0;

  // Start code for bit length 1 is 0.
  Dec->DecodeLen[0]=0;

  // Right aligned upper limit code for current bit length.
  uint UpperLimit=0;

  for (size_t I=1;I<16;I++)
  {
    // Adjust the upper limit code.
    UpperLimit+=LengthCount[I];

    // Left aligned upper limit code.
    uint LeftAligned=UpperLimit<<(16-I);

    // Prepare the upper limit code for next bit length.
    UpperLimit*=2;

    // Store the left aligned upper limit code.
    Dec->DecodeLen[I]=(uint)LeftAligned;

    // Every item of this array contains the sum of all preceding items.
    // So it contains the start position in code list for every bit length. 
    Dec->DecodePos[I]=Dec->DecodePos[I-1]+LengthCount[I-1];
  }

  // Prepare the copy of DecodePos. We'll modify this copy below,
  // so we cannot use the original DecodePos.
  uint CopyDecodePos[ASIZE(Dec->DecodePos)];
  memcpy(CopyDecodePos,Dec->DecodePos,sizeof(CopyDecodePos));

  // For every bit length in the bit length table and so for every item
  // of alphabet.
  for (uint I=0;I<Size;I++)
  {
    // Get the current bit length.
    byte CurBitLength=LengthTable[I] & 0xf;

    if (CurBitLength!=0)
    {
      // Last position in code list for current bit length.
      uint LastPos=CopyDecodePos[CurBitLength];

      // Prepare the decode table, so this position in code list will be
      // decoded to current alphabet item number.
      Dec->DecodeNum[LastPos]=(ushort)I;

      // We'll use next position number for this bit length next time.
      // So we pass through the entire range of positions available
      // for every bit length.
      CopyDecodePos[CurBitLength]++;
    }
  }

  // Define the number of bits to process in quick mode. We use more bits
  // for larger alphabets. More bits means that more codes will be processed
  // in quick mode, but also that more time will be spent to preparation
  // of tables for quick decode.
  switch (Size)
  {
    case NC:
    case NC20:
    case NC30:
      Dec->QuickBits=MAX_QUICK_DECODE_BITS;
      break;
    default:
      Dec->QuickBits=MAX_QUICK_DECODE_BITS>3 ? MAX_QUICK_DECODE_BITS-3 : 0;
      break;
  }

  // Size of tables for quick mode.
  uint QuickDataSize=1<<Dec->QuickBits;

  // Bit length for current code, start from 1 bit codes. It is important
  // to use 1 bit instead of 0 for minimum code length, so we are moving
  // forward even when processing a corrupt archive.
  uint CurBitLength=1;

  // For every right aligned bit string which supports the quick decoding.
  for (uint Code=0;Code<QuickDataSize;Code++)
  {
    // Left align the current code, so it will be in usual bit field format.
    uint BitField=Code<<(16-Dec->QuickBits);

    // Prepare the table for quick decoding of bit lengths.
  
    // Find the upper limit for current bit field and adjust the bit length
    // accordingly if necessary.
    while (CurBitLength<ASIZE(Dec->DecodeLen) && BitField>=Dec->DecodeLen[CurBitLength])
      CurBitLength++;

    // Translation of right aligned bit string to bit length.
    Dec->QuickLen[Code]=CurBitLength;

    // Prepare the table for quick translation of position in code list
    // to position in alphabet.

    // Calculate the distance from the start code for current bit length.
    uint Dist=BitField-Dec->DecodeLen[CurBitLength-1];

    // Right align the distance.
    Dist>>=(16-CurBitLength);

    // Now we can calculate the position in the code list. It is the sum
    // of first position for current bit length and right aligned distance
    // between our bit field and start code for current bit length.
    uint Pos;
    if (CurBitLength<ASIZE(Dec->DecodePos) &&
        (Pos=Dec->DecodePos[CurBitLength]+Dist)<Size)
    {
      // Define the code to alphabet number translation.
      Dec->QuickNum[Code]=Dec->DecodeNum[Pos];
    }
    else
    {
      // Can be here for length table filled with zeroes only (empty).
      Dec->QuickNum[Code]=0;
    }
  }
}
