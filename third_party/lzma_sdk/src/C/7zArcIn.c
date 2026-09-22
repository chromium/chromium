/* 7zArcIn.c -- 7z Input functions
: Igor Pavlov : Public domain */

#include "Precomp.h"

#include <string.h>

#include "7z.h"
#include "7zBuf.h"
#include "7zCrc.h"
#include "CpuArch.h"

#define MY_ALLOC(T, p, size, alloc) \
  { if ((p = (T *)ISzAlloc_Alloc(alloc, (size_t)(size) * sizeof(T))) == NULL) return SZ_ERROR_MEM; }

#define MY_ALLOC_ZE(T, p, size, alloc) \
  { if ((size) == 0) p = NULL; else MY_ALLOC(T, p, size, alloc) }

#define MY_ALLOC_AND_CPY(to, size, from, alloc) \
  { MY_ALLOC(Byte, to, size, alloc); memcpy(to, from, size); }

#define MY_ALLOC_ZE_AND_CPY(to, size, from, alloc) \
  { if ((size) == 0) to = NULL; else { MY_ALLOC_AND_CPY(to, size, from, alloc) } }

enum EIdEnum
{
  k7zIdEnd,
  k7zIdHeader,
  k7zIdArchiveProperties,
  k7zIdAdditionalStreamsInfo,
  k7zIdMainStreamsInfo,
  k7zIdFilesInfo,
  k7zIdPackInfo,
  k7zIdUnpackInfo,
  k7zIdSubStreamsInfo,
  k7zIdSize,
  k7zIdCRC,
  k7zIdFolder,
  k7zIdCodersUnpackSize,
  k7zIdNumUnpackStream,
  k7zIdEmptyStream,
  k7zIdEmptyFile,
  k7zIdAnti,
  k7zIdName,
  k7zIdCTime,
  k7zIdATime,
  k7zIdMTime,
  k7zIdWinAttrib,
  k7zIdComment,
  k7zIdEncodedHeader,
  k7zIdStartPos,
  k7zIdDummy
};

const Byte k7zSignature[k7zSignatureSize] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};

#define SzBitUi32s_INIT(p) { (p)->Defs = NULL; (p)->Vals = NULL; }

Z7_FORCE_INLINE
static SRes SzBitUi32s_Alloc(CSzBitUi32s *p, size_t num, ISzAllocPtr alloc)
{
  if (num == 0)
  {
    p->Defs = NULL;
    p->Vals = NULL;
  }
  else
  {
    MY_ALLOC(Byte, p->Defs, (num + 7) >> 3, alloc)
    MY_ALLOC(UInt32, p->Vals, num, alloc)
  }
  return SZ_OK;
}

Z7_FORCE_INLINE
static void SzBitUi32s_Free(CSzBitUi32s *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->Defs); p->Defs = NULL;
  ISzAlloc_Free(alloc, p->Vals); p->Vals = NULL;
}

#define SzBitUi64s_INIT(p) { (p)->Defs = NULL; (p)->Vals = NULL; }

Z7_FORCE_INLINE
static void SzBitUi64s_Free(CSzBitUi64s *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->Defs); p->Defs = NULL;
  ISzAlloc_Free(alloc, p->Vals); p->Vals = NULL;
}

Z7_NO_INLINE
static void SzAr_Init(CSzAr *p)
{
  p->NumPackStreams = 0;
  p->NumFolders = 0;
  
  p->PackPositions = NULL;
  SzBitUi32s_INIT(&p->FolderCRCs)

  p->FoCodersOffsets = NULL;
  p->FoStartPackStreamIndex = NULL;
  p->FoToCoderUnpackSizes = NULL;
  p->FoToMainUnpackSizeIndex = NULL;
  p->CoderUnpackSizes = NULL;

  p->CodersData = NULL;

  p->RangeLimit = 0;
}

Z7_NO_INLINE
static void SzAr_Free(CSzAr *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->PackPositions);
  SzBitUi32s_Free(&p->FolderCRCs, alloc);
  ISzAlloc_Free(alloc, p->FoCodersOffsets);
  ISzAlloc_Free(alloc, p->FoStartPackStreamIndex);
  ISzAlloc_Free(alloc, p->FoToCoderUnpackSizes);
  ISzAlloc_Free(alloc, p->FoToMainUnpackSizeIndex);
  ISzAlloc_Free(alloc, p->CoderUnpackSizes);
  ISzAlloc_Free(alloc, p->CodersData);

  SzAr_Init(p);
}

Z7_NO_INLINE
void SzArEx_Init(CSzArEx *p)
{
  SzAr_Init(&p->db);
  
  p->NumFiles = 0;
  p->dataPos = 0;
  
  p->UnpackPositions = NULL;
  p->IsDirs = NULL;
  
  p->FolderToFile = NULL;
  p->FileToFolder = NULL;
  
  p->FileNameOffsets = NULL;
  p->FileNames = NULL;
  
  SzBitUi32s_INIT(&p->CRCs)
  SzBitUi32s_INIT(&p->Attribs)
  // SzBitUi32s_INIT(&p->Parents)
  SzBitUi64s_INIT(&p->MTime)
  SzBitUi64s_INIT(&p->CTime)
}

Z7_NO_INLINE
void SzArEx_Free(CSzArEx *p, ISzAllocPtr alloc)
{
  ISzAlloc_Free(alloc, p->UnpackPositions);
  ISzAlloc_Free(alloc, p->IsDirs);

  ISzAlloc_Free(alloc, p->FolderToFile);
  ISzAlloc_Free(alloc, p->FileToFolder);

  ISzAlloc_Free(alloc, p->FileNameOffsets);
  ISzAlloc_Free(alloc, p->FileNames);

  SzBitUi32s_Free(&p->CRCs, alloc);
  SzBitUi32s_Free(&p->Attribs, alloc);
  // SzBitUi32s_Free(&p->Parents, alloc);
  SzBitUi64s_Free(&p->MTime, alloc);
  SzBitUi64s_Free(&p->CTime, alloc);
  
  SzAr_Free(&p->db, alloc);
  SzArEx_Init(p);
}


#define SzData_CLEAR(p) { (p)->Data = NULL; (p)->Size = 0; }

#define SZ_READ_BYTE_SD_NOCHECK(_sd_, dest) \
    (_sd_)->Size--; dest = *(_sd_)->Data++;

#define SZ_READ_BYTE_SD(_sd_, dest) \
    if ((_sd_)->Size == 0) return SZ_ERROR_ARCHIVE; \
    SZ_READ_BYTE_SD_NOCHECK(_sd_, dest)

#define SZ_READ_BYTE(dest) SZ_READ_BYTE_SD(sd, dest)

#define SZ_READ_BYTE_2(dest) \
    if (sd.Size == 0) return SZ_ERROR_ARCHIVE; \
    sd.Size--; dest = *sd.Data++;

#define SKIP_DATA(sd, size) { sd->Size -= (size_t)(size); sd->Data += (size_t)(size); }
#define SKIP_DATA2(sd, size) { sd.Size -= (size_t)(size); sd.Data += (size_t)(size); }

static Z7_NO_INLINE SRes ReadNumber(CSzData *sd, UInt64 *value)
{
  unsigned firstByte, mask, i, v;

  SZ_READ_BYTE(firstByte)
  if ((firstByte & 0x80) == 0)
  {
    *value = firstByte;
    return SZ_OK;
  }
  SZ_READ_BYTE(v)
  if ((firstByte & 0x40) == 0)
  {
    *value = (((UInt32)firstByte & 0x3F) << 8) | v;
    return SZ_OK;
  }
  SZ_READ_BYTE(mask)
  *value = v | ((UInt32)mask << 8);
  mask = 0x20;
  for (i = 2 * 8; i < 8 * 8; i += 8)
  {
    unsigned b;
    if ((firstByte & mask) == 0)
    {
      const UInt64 highPart = (unsigned)firstByte & (unsigned)(mask - 1);
      *value |= (highPart << i);
      return SZ_OK;
    }
    mask >>= 1;
    SZ_READ_BYTE(b)
    *value |= ((UInt64)b << i);
  }
  return SZ_OK;
}


static Z7_NO_INLINE SRes SzReadNumber32(CSzData *sd, UInt32 *value)
{
  unsigned firstByte;
  UInt64 value64;
  if (sd->Size == 0)
    return SZ_ERROR_ARCHIVE;
  firstByte = *sd->Data;
  if ((firstByte & 0x80) == 0)
  {
    *value = firstByte;
    sd->Data++;
    sd->Size--;
    return SZ_OK;
  }
  RINOK(ReadNumber(sd, &value64))
  if (value64 >= (UInt32)0x80000000 - 1)
    return SZ_ERROR_UNSUPPORTED;
  if (value64 >= ((UInt64)(1) << ((sizeof(size_t) - 1) * 8 + 4)))
    return SZ_ERROR_UNSUPPORTED;
  *value = (UInt32)value64;
  return SZ_OK;
}

#define ReadID(sd, value) ReadNumber(sd, value)

Z7_NO_INLINE
static SRes SkipData(CSzData *sd)
{
  UInt64 size;
  RINOK(ReadNumber(sd, &size))
  if (size > sd->Size)
    return SZ_ERROR_ARCHIVE;
  SKIP_DATA(sd, size)
  return SZ_OK;
}

Z7_NO_INLINE
static SRes WaitId(CSzData *sd, UInt32 id)
{
  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == id)
      return SZ_OK;
    if (type == k7zIdEnd)
      return SZ_ERROR_ARCHIVE;
    RINOK(SkipData(sd))
  }
}


Z7_NO_INLINE
static UInt32 CountDefinedBits(const Byte *bits, UInt32 numItems)
{
  unsigned b = 0;
  unsigned m = 0;
  UInt32 sum = 0;
  for (; numItems != 0; numItems--)
  {
    if (m == 0)
    {
      b = *bits++;
      m = 8;
    }
    m--;
    sum += (UInt32)((b >> m) & 1);
  }
  return sum;
}

static Z7_NO_INLINE SRes ReadBitVector(CSzData *sd, size_t numItems, Byte **v, ISzAllocPtr alloc)
{
  Byte allAreDefined;
  Byte *v2;
  const size_t numBytes = (numItems + 7) >> 3;
  *v = NULL;
  SZ_READ_BYTE(allAreDefined)
  if (numBytes == 0)
    return SZ_OK;
  if (allAreDefined == 0)
  {
    if (numBytes > sd->Size)
      return SZ_ERROR_ARCHIVE;
    MY_ALLOC_AND_CPY(*v, numBytes, sd->Data, alloc)
    SKIP_DATA(sd, numBytes)
    return SZ_OK;
  }
  MY_ALLOC(Byte, *v, numBytes, alloc)
  v2 = *v;
  memset(v2, 0xFF, (size_t)numBytes);
  {
    const unsigned numBits = (unsigned)numItems & 7;
    if (numBits)
      v2[(size_t)numBytes - 1] = (Byte)(0xff00u >> numBits);
  }
  return SZ_OK;
}

static Z7_NO_INLINE SRes ReadUi32s(CSzData *sd2, size_t numItems, CSzBitUi32s *crcs, ISzAllocPtr alloc)
{
  size_t i;
  const Byte *data;
  size_t size;
  UInt32 *vals;
  const Byte *defs;
  MY_ALLOC_ZE(UInt32, vals, numItems, alloc)
  crcs->Vals = vals;
  defs = crcs->Defs;
  data = sd2->Data;
  size = sd2->Size;
  for (i = 0; i < numItems; i++)
    if (SzBitArray_Check(defs, i))
    {
      if (size < 4)
        return SZ_ERROR_ARCHIVE;
      size -= 4;
      vals[i] = GetUi32(data);
      data += 4;
    }
    else
      vals[i] = 0;
  sd2->Data = data;
  sd2->Size = size;
  return SZ_OK;
}

static SRes ReadBitUi32s(CSzData *sd, size_t numItems, CSzBitUi32s *crcs, ISzAllocPtr alloc)
{
  if (crcs->Defs)
    return SZ_ERROR_ARCHIVE;
  RINOK(ReadBitVector(sd, numItems, &crcs->Defs, alloc))
  return ReadUi32s(sd, numItems, crcs, alloc);
}

Z7_NO_INLINE
static SRes SkipBitUi32s(CSzData *sd, UInt32 numItems)
{
  Byte allAreDefined;
  UInt32 numDefined = numItems;
  SZ_READ_BYTE(allAreDefined)
  if (!allAreDefined)
  {
    const size_t numBytes = (numItems + 7) >> 3;
    if (numBytes > sd->Size)
      return SZ_ERROR_ARCHIVE;
    numDefined = CountDefinedBits(sd->Data, numItems);
    SKIP_DATA(sd, numBytes)
  }
  if (numDefined > (sd->Size >> 2))
    return SZ_ERROR_ARCHIVE;
  SKIP_DATA(sd, (size_t)numDefined * 4)
  return SZ_OK;
}

static SRes ReadPackInfo(CSzAr *p, CSzData *sd, ISzAllocPtr alloc)
{
  RINOK(SzReadNumber32(sd, &p->NumPackStreams))
  RINOK(WaitId(sd, k7zIdSize))
  {
    UInt64 *packPositions;
    UInt64 sum;
    size_t num = (size_t)p->NumPackStreams + 1;
    MY_ALLOC(UInt64, packPositions, num, alloc)
    p->PackPositions = packPositions;
    sum = 0;
    for (;;)
    {
      UInt64 packSize;
      *packPositions++ = sum;
      if (--num == 0)
        break;
      RINOK(ReadNumber(sd, &packSize))
      sum += packSize;
      if (sum < packSize)
        return SZ_ERROR_ARCHIVE;
    }
  }
  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      return SZ_OK;
    if (type == k7zIdCRC)
    {
      /* CRC of packed streams is unused now */
      RINOK(SkipBitUi32s(sd, p->NumPackStreams))
      continue;
    }
    RINOK(SkipData(sd))
  }
}


#define k_NumCodersStreams_in_Folder_MAX (SZ_NUM_BONDS_IN_FOLDER_MAX + SZ_NUM_PACK_STREAMS_IN_FOLDER_MAX)

SRes SzGetNextFolderItem(CSzFolder *f, CSzData *sd)
{
  UInt32 numCoders, i, numInStreams = 0;
  const Byte * const dataStart = sd->Data;

  // f->NumCoders = 0;
  // f->NumBonds = 0;
  // f->NumPackStreams = 0;
  f->UnpackStream = 0;
  
  RINOK(SzReadNumber32(sd, &numCoders))
  if (numCoders == 0 || numCoders > SZ_NUM_CODERS_IN_FOLDER_MAX)
    return SZ_ERROR_UNSUPPORTED;
  f->NumCoders = numCoders;
  
  for (i = 0; i < numCoders; i++)
  {
    Byte mainByte;
    CSzCoderInfo *coder;
    {
      unsigned idSize, j;
      UInt64 id;
      const Byte *data;
      
      if (!sd->Size)
        return SZ_ERROR_ARCHIVE;
      sd->Size--;
      data = sd->Data;
      mainByte = *data++;
      if (mainByte & 0xC0)
        return SZ_ERROR_UNSUPPORTED;
      idSize = mainByte & 0xF;
      if (idSize > sizeof(id))
        return SZ_ERROR_UNSUPPORTED;
      if (idSize > sd->Size)
        return SZ_ERROR_ARCHIVE;
      sd->Size -= idSize;
      id = 0;
      for (j = 0; j < idSize; j++)
        id = (id << 8) | *data++;
      sd->Data = data;
      if (id > (UInt32)0xFFFFFFFF)
        return SZ_ERROR_UNSUPPORTED;
      coder = f->Coders + i;
      coder->MethodID = (UInt32)id;
    }
    
    coder->NumStreams = 1;
    coder->PropsOffset = 0;
    coder->PropsSize = 0;
    
    if (mainByte & 0x10)
    {
      UInt32 numStreams;
      RINOK(SzReadNumber32(sd, &numStreams))
      if (numStreams > k_NumCodersStreams_in_Folder_MAX)
        return SZ_ERROR_UNSUPPORTED;
      coder->NumStreams = (Byte)numStreams;
      RINOK(SzReadNumber32(sd, &numStreams))
      if (numStreams != 1)
        return SZ_ERROR_UNSUPPORTED;
    }

    numInStreams += coder->NumStreams;
    if (numInStreams > k_NumCodersStreams_in_Folder_MAX)
      return SZ_ERROR_UNSUPPORTED;

    if (mainByte & 0x20)
    {
      UInt32 propsSize;
      RINOK(SzReadNumber32(sd, &propsSize))
      if (propsSize > sd->Size)
        return SZ_ERROR_ARCHIVE;
      if (propsSize >= 0x80)
        return SZ_ERROR_UNSUPPORTED;
      coder->PropsOffset = (size_t)(sd->Data - dataStart);
      coder->PropsSize = (Byte)propsSize;
      sd->Data += (size_t)propsSize;
      sd->Size -= (size_t)propsSize;
    }
  }

  {
    Byte streamUsed[k_NumCodersStreams_in_Folder_MAX];
    UInt32 numBonds, numPackStreams;
    
    numBonds = numCoders - 1;
    if (numInStreams < numBonds)
      return SZ_ERROR_ARCHIVE;
    if (numBonds > SZ_NUM_BONDS_IN_FOLDER_MAX)
      return SZ_ERROR_UNSUPPORTED;
    f->NumBonds = numBonds;
    
    numPackStreams = numInStreams - numBonds;
    if (numPackStreams > SZ_NUM_PACK_STREAMS_IN_FOLDER_MAX)
      return SZ_ERROR_UNSUPPORTED;
    f->NumPackStreams = numPackStreams;
  
    for (i = 0; i < Z7_ARRAY_SIZE(streamUsed); i++) // numInStreams
      streamUsed[i] = False;
    
    if (numBonds)
    {
      Byte coderUsed[SZ_NUM_CODERS_IN_FOLDER_MAX];
      for (i = 0; i < Z7_ARRAY_SIZE(coderUsed); i++) // numCoders
        coderUsed[i] = False;
      
      for (i = 0; i < numBonds; i++)
      {
        CSzBond *bp = f->Bonds + i;
        
        RINOK(SzReadNumber32(sd, &bp->InIndex))
        if (bp->InIndex >= numInStreams || streamUsed[bp->InIndex])
          return SZ_ERROR_ARCHIVE;
        streamUsed[bp->InIndex] = True;
        
        RINOK(SzReadNumber32(sd, &bp->OutIndex))
        if (bp->OutIndex >= numCoders || coderUsed[bp->OutIndex])
          return SZ_ERROR_ARCHIVE;
        coderUsed[bp->OutIndex] = True;
      }
      
      for (i = 0;;)
      {
        if (!coderUsed[i])
          break;
        if (++i == numCoders)
          return SZ_ERROR_ARCHIVE;
      }
      f->UnpackStream = i;
    }
    
    if (numPackStreams == 1)
    {
      for (i = 0;; i++)
      {
        if (i == numInStreams)
          return SZ_ERROR_ARCHIVE;
        if (!streamUsed[i])
          break;
      }
      f->PackStreams[0] = i;
    }
    else
      for (i = 0; i < numPackStreams; i++)
      {
        UInt32 index;
        RINOK(SzReadNumber32(sd, &index))
        if (index >= numInStreams || streamUsed[index])
          return SZ_ERROR_ARCHIVE;
        streamUsed[index] = True;
        f->PackStreams[i] = index;
      }
  }
  return SZ_OK;
}


static SRes SkipNumbers(CSzData *sd2, UInt32 num)
{
  const Byte *data = sd2->Data;
  size_t size = sd2->Size;
  for (; num != 0; num--)
  {
    unsigned firstByte;
    unsigned i;
    if (size == 0)
      return SZ_ERROR_ARCHIVE;
    firstByte = *data;
    for (i = 1; firstByte & 0x80; i++)
      firstByte <<= 1;
    if (size < i)
      return SZ_ERROR_ARCHIVE;
    size -= i;
    data += i;
  }
  sd2->Data = data;
  sd2->Size = size;
  return SZ_OK;
}


#define k_Scan_NumCoders_MAX 64
#define k_Scan_NumCodersStreams_in_Folder_MAX 64

static SRes ReadUnpackInfo(CSzAr *p, CSzData *sd2, const UInt32 numFoldersMax,
    const CBuf *tempBufs, UInt32 numTempBufs, ISzAllocPtr alloc)
{
  CSzData sd;
  UInt32 fo, numFolders, numCodersOutStreams, packStreamIndex;
  const Byte *startBufPtr;
  Byte external;
  
  RINOK(WaitId(sd2, k7zIdFolder))
  
  RINOK(SzReadNumber32(sd2, &numFolders))
  if (numFolders > numFoldersMax)
    return SZ_ERROR_UNSUPPORTED;
  p->NumFolders = numFolders;

  SZ_READ_BYTE_SD(sd2, external)
  if (external == 0)
    sd = *sd2;
  else
  {
    UInt32 index;
    RINOK(SzReadNumber32(sd2, &index))
    if (index >= numTempBufs)
      return SZ_ERROR_ARCHIVE;
    sd.Data = tempBufs[index].data;
    sd.Size = tempBufs[index].size;
  }
  
  MY_ALLOC(size_t, p->FoCodersOffsets, (size_t)numFolders + 1, alloc)
  MY_ALLOC(UInt32, p->FoStartPackStreamIndex, (size_t)numFolders + 1, alloc)
  MY_ALLOC(UInt32, p->FoToCoderUnpackSizes, (size_t)numFolders + 1, alloc)
  MY_ALLOC_ZE(Byte, p->FoToMainUnpackSizeIndex, (size_t)numFolders, alloc)
  
  startBufPtr = sd.Data;
  
  packStreamIndex = 0;
  numCodersOutStreams = 0;

  for (fo = 0; fo < numFolders; fo++)
  {
    UInt32 numCoders, ci, numInStreams = 0;
    
    p->FoCodersOffsets[fo] = (size_t)(sd.Data - startBufPtr);
    
    RINOK(SzReadNumber32(&sd, &numCoders))
    if (numCoders == 0 || numCoders > k_Scan_NumCoders_MAX)
      return SZ_ERROR_UNSUPPORTED;
    
    for (ci = 0; ci < numCoders; ci++)
    {
      Byte mainByte;
      unsigned idSize;
      UInt32 coderInStreams;
      
      SZ_READ_BYTE_2(mainByte)
      if ((mainByte & 0xC0) != 0)
        return SZ_ERROR_UNSUPPORTED;
      idSize = (mainByte & 0xF);
      if (idSize > 8)
        return SZ_ERROR_UNSUPPORTED;
      if (idSize > sd.Size)
        return SZ_ERROR_ARCHIVE;
      SKIP_DATA2(sd, idSize)
      
      coderInStreams = 1;
      
      if ((mainByte & 0x10) != 0)
      {
        UInt32 coderOutStreams;
        RINOK(SzReadNumber32(&sd, &coderInStreams))
        RINOK(SzReadNumber32(&sd, &coderOutStreams))
        if (coderInStreams > k_Scan_NumCodersStreams_in_Folder_MAX || coderOutStreams != 1)
          return SZ_ERROR_UNSUPPORTED;
      }
      
      numInStreams += coderInStreams;

      if ((mainByte & 0x20) != 0)
      {
        UInt32 propsSize;
        RINOK(SzReadNumber32(&sd, &propsSize))
        if (propsSize > sd.Size)
          return SZ_ERROR_ARCHIVE;
        SKIP_DATA2(sd, propsSize)
      }
    }
    
    {
      UInt32 indexOfMainStream = 0;
      UInt32 numPackStreams = 1;
      
      if (numCoders != 1 || numInStreams != 1)
      {
        Byte streamUsed[k_Scan_NumCodersStreams_in_Folder_MAX];
        Byte coderUsed[k_Scan_NumCoders_MAX];
        UInt32 i;
        const UInt32 numBonds = numCoders - 1;
        if (numInStreams < numBonds)
          return SZ_ERROR_ARCHIVE;
        if (numInStreams > k_Scan_NumCodersStreams_in_Folder_MAX)
          return SZ_ERROR_UNSUPPORTED;
        for (i = 0; i < numInStreams; i++)
          streamUsed[i] = False;
        for (i = 0; i < numCoders; i++)
          coderUsed[i] = False;
        
        for (i = 0; i < numBonds; i++)
        {
          UInt32 index;
          
          RINOK(SzReadNumber32(&sd, &index))
          if (index >= numInStreams || streamUsed[index])
            return SZ_ERROR_ARCHIVE;
          streamUsed[index] = True;
          
          RINOK(SzReadNumber32(&sd, &index))
          if (index >= numCoders || coderUsed[index])
            return SZ_ERROR_ARCHIVE;
          coderUsed[index] = True;
        }
        
        numPackStreams = numInStreams - numBonds;
        
        if (numPackStreams != 1)
          for (i = 0; i < numPackStreams; i++)
          {
            UInt32 index;
            RINOK(SzReadNumber32(&sd, &index))
            if (index >= numInStreams || streamUsed[index])
              return SZ_ERROR_ARCHIVE;
            streamUsed[index] = True;
          }

        for (i = 0;;)
        {
          if (!coderUsed[i])
            break;
          if (++i == numCoders)
            return SZ_ERROR_ARCHIVE;
        }
        indexOfMainStream = i;
      }
      
      p->FoStartPackStreamIndex[fo] = packStreamIndex;
      p->FoToCoderUnpackSizes[fo] = numCodersOutStreams;
      p->FoToMainUnpackSizeIndex[fo] = (Byte)indexOfMainStream;
      numCodersOutStreams += numCoders;
      if (numCodersOutStreams < numCoders)
        return SZ_ERROR_UNSUPPORTED;
      if (numPackStreams > p->NumPackStreams - packStreamIndex)
        return SZ_ERROR_ARCHIVE;
      packStreamIndex += numPackStreams;
    }
  }

  {
    const size_t k_numCodersOutStreams_Limit = (size_t)1 << (sizeof(size_t) * 8 - 4);
    if (numCodersOutStreams >= k_numCodersOutStreams_Limit)
      return SZ_ERROR_UNSUPPORTED;
  }
  p->FoToCoderUnpackSizes[fo] = numCodersOutStreams;
  p->FoStartPackStreamIndex[fo] = packStreamIndex;
  {
    const size_t dataSize = (size_t)(sd.Data - startBufPtr);
    p->FoCodersOffsets[fo] = dataSize;
    MY_ALLOC_ZE_AND_CPY(p->CodersData, dataSize, startBufPtr, alloc)
  }
  
  if (external)
  {
    if (sd.Size)
      return SZ_ERROR_ARCHIVE;
    sd = *sd2;
  }
  
  RINOK(WaitId(&sd, k7zIdCodersUnpackSize))
  {
    UInt64 *sizes;
    MY_ALLOC_ZE(UInt64, sizes, (size_t)numCodersOutStreams, alloc)
    p->CoderUnpackSizes = sizes;
    if (numCodersOutStreams)
      do
      {
        RINOK(ReadNumber(&sd, sizes))
        sizes++;
      }
      while (--numCodersOutStreams);
  }

  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(&sd, &type))
    if (type == k7zIdEnd)
      break;
    if (type == k7zIdCRC)
    {
      RINOK(ReadBitUi32s(&sd, numFolders, &p->FolderCRCs, alloc))
      continue;
    }
    RINOK(SkipData(&sd))
  }
  *sd2 = sd;
  return SZ_OK;
}


UInt64 SzAr_GetFolderUnpackSize(const CSzAr *p, UInt32 folderIndex)
{
  return p->CoderUnpackSizes[p->FoToCoderUnpackSizes[folderIndex] + p->FoToMainUnpackSizeIndex[folderIndex]];
}


typedef struct
{
  UInt32 NumTotalSubStreams;
  UInt32 NumSubDigests;
  CSzData sdNumSubStreams;
  CSzData sdSizes;
  CSzData sdCRCs;
} CSubStreamInfo;


static SRes ReadSubStreamsInfo(const CSzAr *p, CSzData *sd, CSubStreamInfo *ssi)
{
  UInt64 type = 0;
  const UInt32 numFolders = p->NumFolders;
  UInt32 numUnpackStreams = numFolders;
  UInt32 numSubDigests = numFolders;
  UInt32 numUnpackSizesInData = 0;

  for (;;)
  {
    RINOK(ReadID(sd, &type))
    if (type == k7zIdNumUnpackStream)
    {
      UInt32 i;
      if (ssi->sdNumSubStreams.Data)
        return SZ_ERROR_UNSUPPORTED;
      ssi->sdNumSubStreams.Data = sd->Data;
      numUnpackStreams = 0;
      numSubDigests = 0;
      for (i = 0; i < numFolders; i++)
      {
        UInt32 numStreams;
        RINOK(SzReadNumber32(sd, &numStreams))
        numUnpackStreams += numStreams;
        if (numUnpackStreams < numStreams)
          return SZ_ERROR_UNSUPPORTED;
        if (numStreams)
          numUnpackSizesInData += (numStreams - 1);
        if (numStreams != 1 || !SzBitWithVals_Check(&p->FolderCRCs, i))
          numSubDigests += numStreams;
      }
      ssi->sdNumSubStreams.Size = (size_t)(sd->Data - ssi->sdNumSubStreams.Data);
      continue;
    }
    if (type == k7zIdCRC || type == k7zIdSize || type == k7zIdEnd)
      break;
    RINOK(SkipData(sd))
  }

  if (!ssi->sdNumSubStreams.Data && p->FolderCRCs.Defs)
    numSubDigests = numFolders - CountDefinedBits(p->FolderCRCs.Defs, numFolders);
  
  ssi->NumTotalSubStreams = numUnpackStreams;
  ssi->NumSubDigests = numSubDigests;

  if (type == k7zIdSize)
  {
    ssi->sdSizes.Data = sd->Data;
    RINOK(SkipNumbers(sd, numUnpackSizesInData))
    ssi->sdSizes.Size = (size_t)(sd->Data - ssi->sdSizes.Data);
    RINOK(ReadID(sd, &type))
  }

  for (;;)
  {
    if (type == k7zIdEnd)
      return SZ_OK;
    if (type == k7zIdCRC)
    {
      if (ssi->sdCRCs.Data)
        return SZ_ERROR_UNSUPPORTED;
      ssi->sdCRCs.Data = sd->Data;
      RINOK(SkipBitUi32s(sd, numSubDigests))
      ssi->sdCRCs.Size = (size_t)(sd->Data - ssi->sdCRCs.Data);
    }
    else
    {
      RINOK(SkipData(sd))
    }
    RINOK(ReadID(sd, &type))
  }
}


static SRes SzReadStreamsInfo(CSzAr *p,
    CSzData *sd,
    const UInt32 numFoldersMax,
    const CBuf * const tempBufs, const UInt32 numTempBufs,
    UInt64 *dataOffset,
    CSubStreamInfo *ssi,
    ISzAllocPtr alloc)
{
  UInt64 type;

  SzData_CLEAR(&ssi->sdSizes)
  SzData_CLEAR(&ssi->sdCRCs)
  SzData_CLEAR(&ssi->sdNumSubStreams)

  *dataOffset = 0;
  RINOK(ReadID(sd, &type))
  if (type == k7zIdPackInfo)
  {
    RINOK(ReadNumber(sd, dataOffset))
    if (*dataOffset > p->RangeLimit)
      return SZ_ERROR_ARCHIVE;
    RINOK(ReadPackInfo(p, sd, alloc))
    if (p->PackPositions[p->NumPackStreams] > p->RangeLimit - *dataOffset)
      return SZ_ERROR_ARCHIVE;
    RINOK(ReadID(sd, &type))
  }
  if (type == k7zIdUnpackInfo)
  {
    RINOK(ReadUnpackInfo(p, sd, numFoldersMax, tempBufs, numTempBufs, alloc))
    RINOK(ReadID(sd, &type))
  }
  if (type == k7zIdSubStreamsInfo)
  {
    RINOK(ReadSubStreamsInfo(p, sd, ssi))
    RINOK(ReadID(sd, &type))
  }
  else
  {
    ssi->NumTotalSubStreams = p->NumFolders;
    // ssi->NumSubDigests = 0;
  }

  return (type == k7zIdEnd ? SZ_OK : SZ_ERROR_UNSUPPORTED);
}


static SRes SzReadAndDecodePackedStreams(
    ILookInStreamPtr inStream,
    CSzData *sd,
    CBuf * const tempBufs,
    const UInt32 numFoldersMax,
    const UInt64 baseOffset,
    CSzAr *p,
    ISzAllocPtr allocTemp)
{
  UInt64 dataStartPos;
  UInt32 fo;
  CSubStreamInfo ssi;

  RINOK(SzReadStreamsInfo(p, sd, numFoldersMax, NULL, 0, &dataStartPos, &ssi, allocTemp))
  
  dataStartPos += baseOffset;
  if (p->NumFolders == 0)
    return SZ_ERROR_ARCHIVE;
 
  for (fo = 0; fo < p->NumFolders; fo++)
    Buf_Init(tempBufs + fo);
  
  for (fo = 0; fo < p->NumFolders; fo++)
  {
    CBuf *tempBuf = tempBufs + fo;
    const UInt64 unpackSize = SzAr_GetFolderUnpackSize(p, fo);
    if ((size_t)unpackSize != unpackSize)
      return SZ_ERROR_MEM;
    if (!Buf_Create(tempBuf, (size_t)unpackSize, allocTemp))
      return SZ_ERROR_MEM;
  }
  
  for (fo = 0; fo < p->NumFolders; fo++)
  {
    const CBuf *tempBuf = tempBufs + fo;
    RINOK(LookInStream_SeekTo(inStream, dataStartPos))
    RINOK(SzAr_DecodeFolder(p, fo, inStream, dataStartPos, tempBuf->data, tempBuf->size, allocTemp))
  }
  
  return SZ_OK;
}


// (size & 1) == 0
// (data) is aligned for 2-bytes
static SRes SzReadFileNames(const Byte *data, size_t size, UInt32 numFiles, size_t *offsets)
{
  const Byte *p, *lim;
  *offsets++ = 0;
  if (numFiles == 0)
    return (size == 0) ? SZ_OK : SZ_ERROR_ARCHIVE;
  if (size < 2)
    return SZ_ERROR_ARCHIVE;
  lim = data + size;
  if (*(const UInt16 *)(const void *)(lim - 2))
    return SZ_ERROR_ARCHIVE;
  p = data;
  do
  {
    if (p >= lim)
      return SZ_ERROR_ARCHIVE;
    for (; *(const UInt16 *)(const void *)p; p += 2);
    p += 2;
    *offsets++ = (size_t)(p - data) >> 1;
  }
  while (--numFiles);
  return (p == lim) ? SZ_OK : SZ_ERROR_ARCHIVE;
}


static SRes ReadTime(CSzBitUi64s *p, size_t num, CSzData *sd2,
    const CBuf *tempBufs, UInt32 numTempBufs, ISzAllocPtr alloc)
{
  const Byte *data;
  size_t size;
  size_t i;
  CNtfsFileTime *vals;
  Byte *defs;
  Byte external;
  
  if (p->Defs)
    return SZ_ERROR_ARCHIVE;
  RINOK(ReadBitVector(sd2, num, &p->Defs, alloc))
  
  SZ_READ_BYTE_SD(sd2, external)
  if (external == 0)
  {
    data = sd2->Data;
    size = sd2->Size;
  }
  else
  {
    UInt32 index;
    RINOK(SzReadNumber32(sd2, &index))
    if (index >= numTempBufs || sd2->Size)
      return SZ_ERROR_ARCHIVE;
    data = tempBufs[index].data;
    size = tempBufs[index].size;
  }
  
  MY_ALLOC_ZE(CNtfsFileTime, p->Vals, num, alloc)
  vals = p->Vals;
  defs = p->Defs;
  for (i = 0; i < num; i++)
    if (SzBitArray_Check(defs, i))
    {
      if (size < 8)
        return SZ_ERROR_ARCHIVE;
      size -= 8;
      vals[i].Low = GetUi32(data);
      vals[i].High = GetUi32(data + 4);
      data += 8;
    }
    else
      vals[i].High = vals[i].Low = 0;
  
  if (size)
    return SZ_ERROR_ARCHIVE;
  return SZ_OK;
}


#define NUM_ADDITIONAL_STREAMS_MAX 8


static SRes SzReadHeader2(CSzArEx *p, CSzData *sd, ILookInStreamPtr inStream,
    CBuf * const tempBufs, ISzAllocPtr allocMain, ISzAllocPtr allocTemp)
{
  CSubStreamInfo ssi;
  UInt32 numTempBufs = 0;

{
  UInt64 type;
  
  SzData_CLEAR(&ssi.sdSizes)
  SzData_CLEAR(&ssi.sdCRCs)
  SzData_CLEAR(&ssi.sdNumSubStreams)

  ssi.NumSubDigests = 0;
  ssi.NumTotalSubStreams = 0;

  RINOK(ReadID(sd, &type))

  if (type == k7zIdArchiveProperties)
  {
    for (;;)
    {
      UInt64 type2;
      RINOK(ReadID(sd, &type2))
      if (type2 == k7zIdEnd)
        break;
      RINOK(SkipData(sd))
    }
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdAdditionalStreamsInfo)
  {
    CSzAr tempAr;
    SRes res;
    
    SzAr_Init(&tempAr);
    tempAr.RangeLimit = p->db.RangeLimit;

    res = SzReadAndDecodePackedStreams(inStream, sd, tempBufs, NUM_ADDITIONAL_STREAMS_MAX,
        p->startPosAfterHeader, &tempAr, allocTemp);
    numTempBufs = tempAr.NumFolders;
    SzAr_Free(&tempAr, allocTemp);
    
    if (res != SZ_OK)
      return res;
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdMainStreamsInfo)
  {
    static const UInt32 k_numFoldersMax = (UInt32)1 << 30;
    RINOK(SzReadStreamsInfo(&p->db, sd, k_numFoldersMax, tempBufs, numTempBufs,
        &p->dataPos, &ssi, allocMain))
    p->dataPos += p->startPosAfterHeader;
    RINOK(ReadID(sd, &type))
  }

  if (type == k7zIdEnd)
    return SZ_OK;
  if (type != k7zIdFilesInfo)
    return SZ_ERROR_ARCHIVE;
}

{
  UInt32 numFiles, numEmptyStreams = 0;
  const Byte *emptyStreams = NULL;
  const Byte *emptyFiles = NULL;
  
  RINOK(SzReadNumber32(sd, &numFiles))
  p->NumFiles = numFiles;

  for (;;)
  {
    CSzData sdSwitch;
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      break;
    {
      UInt64 size;
      RINOK(ReadNumber(sd, &size))
      if (size > sd->Size)
        return SZ_ERROR_ARCHIVE;
      sdSwitch.Data = sd->Data;
      sdSwitch.Size = (size_t)size;
      SKIP_DATA(sd, size)
    }
    if (type >= 64)
      continue;
    
    switch ((unsigned)type)
    {
      case k7zIdEmptyStream:
      {
        if (emptyStreams || emptyFiles)
          return SZ_ERROR_ARCHIVE;
        if ((numFiles + 7) >> 3 != sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        emptyStreams = sdSwitch.Data;
        numEmptyStreams = CountDefinedBits(emptyStreams, numFiles);
        break;
      }
      
      case k7zIdEmptyFile:
      {
        if (emptyFiles)
          return SZ_ERROR_ARCHIVE;
        if ((numEmptyStreams + 7) >> 3 != sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        emptyFiles = sdSwitch.Data;
        break;
      }
      
      case k7zIdName:
      {
        size_t namesSize;
        const Byte *namesData;
        Byte external;
        if (p->FileNameOffsets)
          return SZ_ERROR_ARCHIVE;
        SZ_READ_BYTE_SD(&sdSwitch, external)
        if (external == 0)
        {
          namesData = sdSwitch.Data;
          namesSize = sdSwitch.Size;
        }
        else
        {
          UInt32 index;
          RINOK(SzReadNumber32(&sdSwitch, &index))
          if (index >= numTempBufs || sdSwitch.Size)
            return SZ_ERROR_ARCHIVE;
          namesData = tempBufs[index].data;
          namesSize = tempBufs[index].size;
        }
        if (namesSize & 1)
          return SZ_ERROR_ARCHIVE;
        MY_ALLOC(size_t, p->FileNameOffsets, numFiles + 1, allocMain)
        MY_ALLOC_ZE_AND_CPY(p->FileNames, namesSize, namesData, allocMain)
        RINOK(SzReadFileNames(p->FileNames, namesSize, numFiles, p->FileNameOffsets))
        break;
      }
      
      case k7zIdWinAttrib:
      {
        Byte external;
        if (p->Attribs.Defs)
          return SZ_ERROR_ARCHIVE;
        RINOK(ReadBitVector(&sdSwitch, numFiles, &p->Attribs.Defs, allocMain))
        SZ_READ_BYTE_SD(&sdSwitch, external)
        if (external)
        {
          UInt32 index;
          RINOK(SzReadNumber32(&sdSwitch, &index))
          if (index >= numTempBufs || sdSwitch.Size)
            return SZ_ERROR_ARCHIVE;
          sdSwitch.Data = tempBufs[index].data;
          sdSwitch.Size = tempBufs[index].size;
        }
        RINOK(ReadUi32s(&sdSwitch, numFiles, &p->Attribs, allocMain))
        if (sdSwitch.Size)
          return SZ_ERROR_ARCHIVE;
        break;
      }
      
      case k7zIdMTime:
      case k7zIdCTime:
      {
        RINOK(ReadTime((unsigned)type == k7zIdMTime ? &p->MTime: &p->CTime,
            numFiles, &sdSwitch, tempBufs, numTempBufs, allocMain))
        break;
      }
      
      default: break;
    }
  }

  if (numFiles - numEmptyStreams != ssi.NumTotalSubStreams)
    return SZ_ERROR_ARCHIVE;

  for (;;)
  {
    UInt64 type;
    RINOK(ReadID(sd, &type))
    if (type == k7zIdEnd)
      break;
    RINOK(SkipData(sd))
  }

  {
    UInt32 i;
    UInt32 emptyFileIndex = 0;
    UInt32 folderIndex = 0;
    UInt32 remSubStreams = 0;
    UInt32 numSubStreams = 0;
    UInt64 unpackPos = 0;
    const Byte *digestsDefs = NULL;
    const Byte *digestsVals = NULL;
    UInt32 digestIndex = 0;
    Byte isDirMask = 0;
    Byte crcMask = 0;
    Byte mask = 0x80;
    
    MY_ALLOC(UInt32, p->FolderToFile, p->db.NumFolders + 1, allocMain)
    MY_ALLOC_ZE(UInt32, p->FileToFolder, p->NumFiles, allocMain)
    MY_ALLOC(UInt64, p->UnpackPositions, p->NumFiles + 1, allocMain)
    MY_ALLOC_ZE(Byte, p->IsDirs, (p->NumFiles + 7) >> 3, allocMain)

    RINOK(SzBitUi32s_Alloc(&p->CRCs, p->NumFiles, allocMain))

    if (ssi.sdCRCs.Size != 0)
    {
      Byte allDigestsDefined = 0;
      SZ_READ_BYTE_SD_NOCHECK(&ssi.sdCRCs, allDigestsDefined)
      if (allDigestsDefined)
        digestsVals = ssi.sdCRCs.Data;
      else
      {
        const size_t numBytes = (ssi.NumSubDigests + 7) >> 3;
        digestsDefs = ssi.sdCRCs.Data;
        digestsVals = digestsDefs + numBytes;
      }
    }

    for (i = 0; i < numFiles; i++, mask >>= 1)
    {
      if (mask == 0)
      {
        const UInt32 byteIndex = (i - 1) >> 3;
        p->IsDirs[byteIndex] = isDirMask;
        p->CRCs.Defs[byteIndex] = crcMask;
        isDirMask = 0;
        crcMask = 0;
        mask = 0x80;
      }

      p->UnpackPositions[i] = unpackPos;
      p->CRCs.Vals[i] = 0;
      
      if (emptyStreams && SzBitArray_Check(emptyStreams, i))
      {
        if (emptyFiles)
        {
          if (!SzBitArray_Check(emptyFiles, emptyFileIndex))
            isDirMask |= mask;
          emptyFileIndex++;
        }
        else
          isDirMask |= mask;
        if (remSubStreams == 0)
        {
          p->FileToFolder[i] = (UInt32)-1;
          continue;
        }
      }
      
      if (remSubStreams == 0)
      {
        for (;;)
        {
          if (folderIndex >= p->db.NumFolders)
            return SZ_ERROR_ARCHIVE;
          p->FolderToFile[folderIndex] = i;
          numSubStreams = 1;
          if (ssi.sdNumSubStreams.Data)
          {
            RINOK(SzReadNumber32(&ssi.sdNumSubStreams, &numSubStreams))
          }
          remSubStreams = numSubStreams;
          if (numSubStreams != 0)
            break;
          {
            const UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
            unpackPos += folderUnpackSize;
            if (unpackPos < folderUnpackSize)
              return SZ_ERROR_ARCHIVE;
          }
          folderIndex++;
        }
      }
      
      p->FileToFolder[i] = folderIndex;
      
      if (emptyStreams && SzBitArray_Check(emptyStreams, i))
        continue;
      
      if (--remSubStreams == 0)
      {
        const UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
        const UInt64 startFolderUnpackPos = p->UnpackPositions[p->FolderToFile[folderIndex]];
        if (folderUnpackSize < unpackPos - startFolderUnpackPos)
          return SZ_ERROR_ARCHIVE;
        unpackPos = startFolderUnpackPos + folderUnpackSize;
        if (unpackPos < folderUnpackSize)
          return SZ_ERROR_ARCHIVE;

        if (numSubStreams == 1 && SzBitWithVals_Check(&p->db.FolderCRCs, folderIndex))
        {
          p->CRCs.Vals[i] = p->db.FolderCRCs.Vals[folderIndex];
          crcMask |= mask;
        }
        folderIndex++;
      }
      else
      {
        UInt64 v;
        RINOK(ReadNumber(&ssi.sdSizes, &v))
        unpackPos += v;
        if (unpackPos < v)
          return SZ_ERROR_ARCHIVE;
      }
      if ((crcMask & mask) == 0 && digestsVals)
      {
        if (!digestsDefs || SzBitArray_Check(digestsDefs, digestIndex))
        {
          p->CRCs.Vals[i] = GetUi32(digestsVals);
          digestsVals += 4;
          crcMask |= mask;
        }
        digestIndex++;
      }
    }

    if (mask != 0x80)
    {
      const UInt32 byteIndex = (i - 1) >> 3;
      p->IsDirs[byteIndex] = isDirMask;
      p->CRCs.Defs[byteIndex] = crcMask;
    }
    
    p->UnpackPositions[i] = unpackPos;

    if (remSubStreams != 0)
      return SZ_ERROR_ARCHIVE;

    for (;;)
    {
      p->FolderToFile[folderIndex] = i;
      if (folderIndex >= p->db.NumFolders)
        break;
      if (!ssi.sdNumSubStreams.Data)
        return SZ_ERROR_ARCHIVE;
      RINOK(SzReadNumber32(&ssi.sdNumSubStreams, &numSubStreams))
      if (numSubStreams != 0)
        return SZ_ERROR_ARCHIVE;
      /*
      {
        UInt64 folderUnpackSize = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
        unpackPos += folderUnpackSize;
        if (unpackPos < folderUnpackSize)
          return SZ_ERROR_ARCHIVE;
      }
      */
      folderIndex++;
    }

    if (ssi.sdNumSubStreams.Data && ssi.sdNumSubStreams.Size != 0)
      return SZ_ERROR_ARCHIVE;
  }
}
  return SZ_OK;
}


static SRes SzReadHeader(
    CSzArEx *p,
    CSzData *sd,
    ILookInStreamPtr inStream,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  UInt32 i;
  SRes res;
  CBuf tempBufs[NUM_ADDITIONAL_STREAMS_MAX];

  for (i = 0; i < NUM_ADDITIONAL_STREAMS_MAX; i++)
    Buf_Init(tempBufs + i);
  
  res = SzReadHeader2(p, sd, inStream, tempBufs, allocMain, allocTemp);
  
  for (i = 0; i < NUM_ADDITIONAL_STREAMS_MAX; i++)
    Buf_Free(tempBufs + i, allocTemp);

  if (res == SZ_OK && sd->Size != 0)
    return SZ_ERROR_ARCHIVE;
  return res;
}

static SRes SzArEx_Open2(
    CSzArEx *p,
    ILookInStreamPtr inStream,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  Byte header[k7zStartHeaderSize];
  Int64 startPosAfterHeader;
  UInt64 nextHeaderOffset, nextHeaderSize;
  size_t nextHeaderSizeT;
  UInt32 nextHeaderCRC;
  CBuf buf;
  SRes res;

  RINOK(LookInStream_Read2(inStream, header, k7zStartHeaderSize, SZ_ERROR_NO_ARCHIVE))
  startPosAfterHeader = 0; // k7zStartHeaderSize
  RINOK(ILookInStream_Seek(inStream, &startPosAfterHeader, SZ_SEEK_CUR))
  p->startPosAfterHeader = (UInt64)startPosAfterHeader;
  {
    unsigned i;
    for (i = 0; i < k7zSignatureSize; i++)
      if (header[i] != k7zSignature[i])
        return SZ_ERROR_NO_ARCHIVE;
    if (header[6] != 0) // k7zMajorVersion
      return SZ_ERROR_UNSUPPORTED;
  }
  if (CrcCalc(header + 12, 20) != GetUi32(header + 8))
    return SZ_ERROR_CRC;

  nextHeaderOffset = GetUi64(header + 12);
  nextHeaderSize = GetUi64(header + 20);
  nextHeaderCRC = GetUi32(header + 28);

  p->db.RangeLimit = nextHeaderOffset;
  if (nextHeaderOffset >= (UInt64)1 << 62 ||
      nextHeaderSize >= (UInt64)1 << 48)
    return SZ_ERROR_NO_ARCHIVE;
  nextHeaderSizeT = (size_t)nextHeaderSize;
  if (nextHeaderSizeT != nextHeaderSize)
    return SZ_ERROR_MEM;
  if (nextHeaderSizeT == 0)
  {
    if (nextHeaderOffset != 0 || nextHeaderCRC != 0)
      return SZ_ERROR_NO_ARCHIVE;
    return SZ_OK;
  }
  {
    Int64 pos = 0;
    RINOK(ILookInStream_Seek(inStream, &pos, SZ_SEEK_END))
    if ((UInt64)(pos - startPosAfterHeader) < nextHeaderOffset + nextHeaderSize)
      return SZ_ERROR_INPUT_EOF;
  }
  RINOK(LookInStream_SeekTo(inStream, (UInt64)startPosAfterHeader + nextHeaderOffset))

  if (!Buf_Create(&buf, nextHeaderSizeT, allocTemp))
    return SZ_ERROR_MEM;

  res = LookInStream_Read(inStream, buf.data, nextHeaderSizeT);
  
  if (res == SZ_OK)
  {
    res = SZ_ERROR_ARCHIVE;
    if (CrcCalc(buf.data, nextHeaderSizeT) == nextHeaderCRC)
    {
      CSzData sd;
      UInt64 type;
      sd.Data = buf.data;
      sd.Size = buf.size;
      
      res = ReadID(&sd, &type);
      
      if (res == SZ_OK && type == k7zIdEncodedHeader)
      {
        CSzAr tempAr;
        CBuf tempBuf;
        Buf_Init(&tempBuf);
        
        SzAr_Init(&tempAr);
        tempAr.RangeLimit = p->db.RangeLimit;

        res = SzReadAndDecodePackedStreams(inStream, &sd, &tempBuf, 1, p->startPosAfterHeader, &tempAr, allocTemp);
        SzAr_Free(&tempAr, allocTemp);
       
        if (res != SZ_OK)
        {
          Buf_Free(&tempBuf, allocTemp);
        }
        else
        {
          Buf_Free(&buf, allocTemp);
          buf.data = tempBuf.data;
          buf.size = tempBuf.size;
          sd.Data = buf.data;
          sd.Size = buf.size;
          res = ReadID(&sd, &type);
        }
      }
  
      if (res == SZ_OK)
      {
        if (type == k7zIdHeader)
        {
          /*
          CSzData sd2;
          unsigned ttt;
          for (ttt = 0; ttt < 40000; ttt++)
          {
            SzArEx_Free(p, allocMain);
            sd2 = sd;
            res = SzReadHeader(p, &sd2, inStream, allocMain, allocTemp);
            if (res != SZ_OK)
              break;
          }
          */
          res = SzReadHeader(p, &sd, inStream, allocMain, allocTemp);
        }
        else
          res = SZ_ERROR_UNSUPPORTED;
      }
    }
  }
 
  Buf_Free(&buf, allocTemp);
  return res;
}


SRes SzArEx_Open(CSzArEx *p, ILookInStreamPtr inStream,
    ISzAllocPtr allocMain, ISzAllocPtr allocTemp)
{
  const SRes res = SzArEx_Open2(p, inStream, allocMain, allocTemp);
  if (res != SZ_OK)
    SzArEx_Free(p, allocMain);
  return res;
}


SRes SzArEx_Extract(
    const CSzArEx *p,
    ILookInStreamPtr inStream,
    UInt32 fileIndex,
    UInt32 *blockIndex,
    Byte **tempBuf,
    size_t *outBufferSize,
    size_t *offset,
    size_t *outSizeProcessed,
    ISzAllocPtr allocMain,
    ISzAllocPtr allocTemp)
{
  const UInt32 folderIndex = p->FileToFolder[fileIndex];
  SRes res = SZ_OK;
  
  *offset = 0;
  *outSizeProcessed = 0;
  
  if (folderIndex == (UInt32)-1)
  {
    ISzAlloc_Free(allocMain, *tempBuf);
    *blockIndex = folderIndex;
    *tempBuf = NULL;
    *outBufferSize = 0;
    return SZ_OK;
  }

  if (*tempBuf == NULL || *blockIndex != folderIndex)
  {
    const UInt64 unpackSizeSpec = SzAr_GetFolderUnpackSize(&p->db, folderIndex);
    /*
    UInt64 unpackSizeSpec =
        p->UnpackPositions[p->FolderToFile[(size_t)folderIndex + 1]] -
        p->UnpackPositions[p->FolderToFile[folderIndex]];
    */
    const size_t unpackSize = (size_t)unpackSizeSpec;

    if (unpackSize != unpackSizeSpec)
      return SZ_ERROR_MEM;
    *blockIndex = folderIndex;
    ISzAlloc_Free(allocMain, *tempBuf);
    *tempBuf = NULL;
    
    if (res == SZ_OK)
    {
      *outBufferSize = unpackSize;
      if (unpackSize != 0)
      {
        *tempBuf = (Byte *)ISzAlloc_Alloc(allocMain, unpackSize);
        if (*tempBuf == NULL)
          res = SZ_ERROR_MEM;
      }
  
      if (res == SZ_OK)
      {
        res = SzAr_DecodeFolder(&p->db, folderIndex,
            inStream, p->dataPos, *tempBuf, unpackSize, allocTemp);
      }
    }
  }

  if (res == SZ_OK)
  {
    const UInt64 unpackPos = p->UnpackPositions[fileIndex];
    *offset = (size_t)(unpackPos - p->UnpackPositions[p->FolderToFile[folderIndex]]);
    *outSizeProcessed = (size_t)(p->UnpackPositions[(size_t)fileIndex + 1] - unpackPos);
    if (*offset + *outSizeProcessed > *outBufferSize)
      return SZ_ERROR_FAIL;
    if (SzBitWithVals_Check(&p->CRCs, fileIndex))
      if (CrcCalc(*tempBuf + *offset, *outSizeProcessed) != p->CRCs.Vals[fileIndex])
        res = SZ_ERROR_CRC;
  }

  return res;
}


size_t SzArEx_GetFileNameUtf16(const CSzArEx *p, size_t fileIndex, UInt16 *dest)
{
  const size_t * const offsets = p->FileNameOffsets;
  if (!offsets)
  {
    if (dest)
      *dest = 0;
    return 1;
  }
  {
    const size_t offs = offsets[fileIndex];
    const size_t len = offsets[fileIndex + 1] - offs;
    if (dest)
    {
      size_t i;
      const Byte *src = p->FileNames + offs * 2;
      for (i = 0; i < len; i++)
        dest[i] = GetUi16a(src + i * 2);
    }
    return len;
  }
}
