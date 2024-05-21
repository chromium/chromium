#include "base/process/memory.h"

#if defined(UNRAR_NO_EXCEPTIONS)
#define UNRAR_FATAL_BAD_ALLOC(size) base::TerminateBecauseOutOfMemory(size)
#else
#define UNRAR_FATAL_BAD_ALLOC(size) throw std::bad_alloc()
#endif

FragmentedWindow::FragmentedWindow()
{
  memset(Mem,0,sizeof(Mem));
  memset(MemSize,0,sizeof(MemSize));
  LastAllocated=0;
}


FragmentedWindow::~FragmentedWindow()
{
  Reset();
}


void FragmentedWindow::Reset()
{
  LastAllocated=0;
  for (uint I=0;I<ASIZE(Mem);I++)
    if (Mem[I]!=NULL)
    {
      free(Mem[I]);
      Mem[I]=NULL;
    }
}


void FragmentedWindow::Init(size_t WinSize)
{
  Reset();

  uint BlockNum=0;
  size_t TotalSize=0; // Already allocated.
  while (TotalSize<WinSize && BlockNum<ASIZE(Mem))
  {
    size_t Size=WinSize-TotalSize; // Size needed to allocate.

    // Minimum still acceptable block size. Next allocations cannot be larger
    // than current, so we do not need blocks if they are smaller than
    // "size left / attempts left". Also we do not waste time to blocks
    // smaller than some arbitrary constant.
    size_t MinSize=Max(Size/(ASIZE(Mem)-BlockNum), 0x400000);

    byte *NewMem=NULL;
    while (Size>=MinSize)
    {
      NewMem=(byte *)malloc(Size);
      if (NewMem!=NULL)
        break;
      Size-=Size/32;
    }
    if (NewMem==NULL)
      UNRAR_FATAL_BAD_ALLOC(Size);

    // Clean the window to generate the same output when unpacking corrupt
    // RAR files, which may access to unused areas of sliding dictionary.
    memset(NewMem,0,Size);

    Mem[BlockNum]=NewMem;
    TotalSize+=Size;
    MemSize[BlockNum]=TotalSize;
    BlockNum++;
  }
  if (TotalSize<WinSize) // Not found enough free blocks.
    UNRAR_FATAL_BAD_ALLOC(WinSize);

  LastAllocated=WinSize;
}


byte& FragmentedWindow::operator [](size_t Item)
{
  if (Item<MemSize[0])
    return Mem[0][Item];
  for (uint I=1;I<ASIZE(MemSize);I++)
    if (Item<MemSize[I])
      return Mem[I][Item-MemSize[I-1]];
  return Mem[0][0]; // Must never happen;
}


void FragmentedWindow::CopyString(uint Length,size_t Distance,size_t &UnpPtr,bool FirstWinDone,size_t MaxWinSize)
{
  size_t SrcPtr=UnpPtr-Distance;
  if (Distance>UnpPtr)
  {
    SrcPtr+=MaxWinSize;

    if (Distance>MaxWinSize || !FirstWinDone)
    {
      while (Length-- > 0)
      {
        (*this)[UnpPtr]=0;
        if (++UnpPtr>=MaxWinSize)
          UnpPtr-=MaxWinSize;
      }
      return;
    }
  }

  while (Length-- > 0)
  {
    (*this)[UnpPtr]=(*this)[SrcPtr];
    if (++SrcPtr>=MaxWinSize)
      SrcPtr-=MaxWinSize;
    if (++UnpPtr>=MaxWinSize)
      UnpPtr-=MaxWinSize;
  }
}


void FragmentedWindow::CopyData(byte *Dest,size_t WinPos,size_t Size)
{
  for (size_t I=0;I<Size;I++)
    Dest[I]=(*this)[WinPos+I];
}


size_t FragmentedWindow::GetBlockSize(size_t StartPos,size_t RequiredSize)
{
  for (uint I=0;I<ASIZE(MemSize);I++)
    if (StartPos<MemSize[I])
      return Min(MemSize[I]-StartPos,RequiredSize);
  return 0; // Must never be here.
}
