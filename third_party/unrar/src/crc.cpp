// This CRC function is based on Intel Slicing-by-8 algorithm.
//
// Original Intel Slicing-by-8 code is available here:
//
//    http://sourceforge.net/projects/slicing-by-8/
//
// Original Intel Slicing-by-8 code is licensed as:
//    
//    Copyright (c) 2004-2006 Intel Corporation - All Rights Reserved
//    
//    This software program is licensed subject to the BSD License, 
//    available at http://www.opensource.org/licenses/bsd-license.html


#include "rar.hpp"

#ifndef SFX_MODULE
// User suggested to avoid BSD license in SFX module, so they do not need
// to include the license to SFX archive.
#define USE_SLICING
#endif

static uint crc_tables[16][256]; // Tables for Slicing-by-16.
static bool is_initialized = false;

#ifdef USE_NEON_CRC32
static bool CRC_Neon;
#endif


// Build the classic CRC32 lookup table.
// We also provide this function to legacy RAR and ZIP decryption code.
void InitCRC32(uint *CRCTab)
{
  if (CRCTab[1]!=0)
    return;
  for (uint I=0;I<256;I++)
  {
    uint C=I;
    for (uint J=0;J<8;J++)
      C=(C & 1) ? (C>>1)^0xEDB88320 : (C>>1);
    CRCTab[I]=C;
  }

#ifdef USE_NEON_CRC32
  #ifdef _APPLE
    // getauxval isn't available in OS X
    uint Value=0;
    size_t Size=sizeof(Value);
    int RetCode=sysctlbyname("hw.optional.armv8_crc32",&Value,&Size,NULL,0);
    CRC_Neon=RetCode==0 && Value!=0;
  #else
    CRC_Neon=(getauxval(AT_HWCAP) & HWCAP_CRC32)!=0;
  #endif
#endif

}


static void InitTables()
{
  InitCRC32(crc_tables[0]);

#ifdef USE_SLICING
  for (uint I=0;I<256;I++) // Build additional lookup tables.
  {
    uint C=crc_tables[0][I];
    for (uint J=1;J<16;J++)
    {
      C=crc_tables[0][(byte)C]^(C>>8);
      crc_tables[J][I]=C;
    }
  }
#endif
}


uint CRC32(uint StartCRC,const void *Addr,size_t Size)
{
  if (!is_initialized) {
    is_initialized = true;
    InitTables();
  }

  byte *Data=(byte *)Addr;

#ifdef USE_NEON_CRC32
  if (CRC_Neon)
  {
    for (;Size>=8;Size-=8,Data+=8)
#ifdef __clang__
      StartCRC = __builtin_arm_crc32d(StartCRC, RawGet8(Data));
#else
      StartCRC = __builtin_aarch64_crc32x(StartCRC, RawGet8(Data));
#endif
    for (;Size>0;Size--,Data++) // Process left data.
#ifdef __clang__
      StartCRC = __builtin_arm_crc32b(StartCRC, *Data);
#else
      StartCRC = __builtin_aarch64_crc32b(StartCRC, *Data);
#endif
    return StartCRC;
  }
#endif

#ifdef USE_SLICING
  // Align Data to 16 for better performance and to avoid ALLOW_MISALIGNED
  // check below.
  for (;Size>0 && ((size_t)Data & 15)!=0;Size--,Data++)
    StartCRC=crc_tables[0][(byte)(StartCRC^Data[0])]^(StartCRC>>8);

  // 2023.12.06: We switched to slicing-by-16, which seems to be faster than
  // slicing-by-8 on modern CPUs. Slicing-by-32 would require 32 KB for tables
  // and could be limited by L1 cache size on some CPUs.
  for (;Size>=16;Size-=16,Data+=16)
  {
#ifdef BIG_ENDIAN
    StartCRC ^= RawGet4(Data);
    uint D1 = RawGet4(Data+4);
    uint D2 = RawGet4(Data+8);
    uint D3 = RawGet4(Data+12);
#else
    // We avoid RawGet4 here for performance reason, to access uint32
    // directly even if ALLOW_MISALIGNED isn't defined. We can do it,
    // because we aligned 'Data' above.
    StartCRC ^= *(uint32 *) Data;
    uint D1 = *(uint32 *) (Data+4);
    uint D2 = *(uint32 *) (Data+8);
    uint D3 = *(uint32 *) (Data+12);
#endif
    StartCRC = crc_tables[15][(byte) StartCRC       ] ^
               crc_tables[14][(byte)(StartCRC >> 8) ] ^
               crc_tables[13][(byte)(StartCRC >> 16)] ^
               crc_tables[12][(byte)(StartCRC >> 24)] ^
               crc_tables[11][(byte) D1             ] ^
               crc_tables[10][(byte)(D1       >> 8) ] ^
               crc_tables[ 9][(byte)(D1       >> 16)] ^
               crc_tables[ 8][(byte)(D1       >> 24)] ^
               crc_tables[ 7][(byte) D2             ] ^
               crc_tables[ 6][(byte)(D2       >>  8)] ^
               crc_tables[ 5][(byte)(D2       >> 16)] ^
               crc_tables[ 4][(byte)(D2       >> 24)] ^
               crc_tables[ 3][(byte) D3             ] ^
               crc_tables[ 2][(byte)(D3       >>  8)] ^
               crc_tables[ 1][(byte)(D3       >> 16)] ^
               crc_tables[ 0][(byte)(D3       >> 24)];
  }
#endif

  for (;Size>0;Size--,Data++) // Process left data.
    StartCRC=crc_tables[0][(byte)(StartCRC^Data[0])]^(StartCRC>>8);

  return StartCRC;
}


#ifndef SFX_MODULE
// For RAR 1.4 archives in case somebody still has them.
ushort Checksum14(ushort StartCRC,const void *Addr,size_t Size)
{
  byte *Data=(byte *)Addr;
  for (size_t I=0;I<Size;I++)
  {
    StartCRC=(StartCRC+Data[I])&0xffff;
    StartCRC=((StartCRC<<1)|(StartCRC>>15))&0xffff;
  }
  return StartCRC;
}
#endif




#if 0
static void TestCRC();
struct TestCRCStruct {TestCRCStruct() {TestCRC();exit(0);}} GlobalTesCRC;

void TestCRC()
{
  // This function is invoked from global object and _SSE_Version is global
  // and can be initialized after this function. So we explicitly initialize
  // it here to enable SSE support in Blake2sp.
  _SSE_Version=GetSSEVersion();

  const uint FirstSize=300;
  byte b[FirstSize];

  if ((CRC32(0xffffffff,(byte*)"testtesttest",12)^0xffffffff)==0x44608e84)
    mprintf(L"\nCRC32 test1 OK");
  else
    mprintf(L"\nCRC32 test1 FAILED");

  if (CRC32(0,(byte*)"te\x80st",5)==0xB2E5C5AE)
    mprintf(L"\nCRC32 test2 OK");
  else
    mprintf(L"\nCRC32 test2 FAILED");

  for (uint I=0;I<14;I++) // Check for possible int sign extension.
    b[I]=(byte)0x7f+I;
  if ((CRC32(0xffffffff,b,14)^0xffffffff)==0x1DFA75DA)
    mprintf(L"\nCRC32 test3 OK");
  else
    mprintf(L"\nCRC32 test3 FAILED");

  for (uint I=0;I<FirstSize;I++)
    b[I]=(byte)I;
  uint r32=CRC32(0xffffffff,b,FirstSize);
  for (uint I=FirstSize;I<1024;I++)
  {
    b[0]=(byte)I;
    r32=CRC32(r32,b,1);
  }
  if ((r32^0xffffffff)==0xB70B4C26)
    mprintf(L"\nCRC32 test4 OK");
  else
    mprintf(L"\nCRC32 test4 FAILED");

  if ((CRC64(0xffffffffffffffff,(byte*)"testtesttest",12)^0xffffffffffffffff)==0x7B1C2D230EDEB436)
    mprintf(L"\nCRC64 test1 OK");
  else
    mprintf(L"\nCRC64 test1 FAILED");

  if (CRC64(0,(byte*)"te\x80st",5)==0xB5DBF9583A6EED4A)
    mprintf(L"\nCRC64 test2 OK");
  else
    mprintf(L"\nCRC64 test2 FAILED");

  for (uint I=0;I<14;I++) // Check for possible int sign extension.
    b[I]=(byte)0x7f+I;
  if ((CRC64(0xffffffffffffffff,b,14)^0xffffffffffffffff)==0xE019941C05B2820C)
    mprintf(L"\nCRC64 test3 OK");
  else
    mprintf(L"\nCRC64 test3 FAILED");

  for (uint I=0;I<FirstSize;I++)
    b[I]=(byte)I;
  uint64 r64=CRC64(0xffffffffffffffff,b,FirstSize);
  for (uint I=FirstSize;I<1024;I++)
  {
    b[0]=(byte)I;
    r64=CRC64(r64,b,1);
  }
  if ((r64^0xffffffffffffffff)==0xD51FB58DC789C400)
    mprintf(L"\nCRC64 test4 OK");
  else
    mprintf(L"\nCRC64 test4 FAILED");

  const size_t BufSize=0x100000;
  byte *Buf=new byte[BufSize];
  GetRnd(Buf,BufSize);

  clock_t StartTime=clock();
  r32=0xffffffff;
  const uint64 BufCount=5000;
  for (uint I=0;I<BufCount;I++)
    r32=CRC32(r32,Buf,BufSize);
  if (r32!=0) // Otherwise compiler optimizer removes CRC calculation.
    mprintf(L"\nCRC32 speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  DataHash Hash;
  Hash.Init(HASH_CRC32,MaxPoolThreads);
  const uint64 BufCountMT=20000;
  for (uint I=0;I<BufCountMT;I++)
    Hash.Update(Buf,BufSize);
  HashValue Result;
  Hash.Result(&Result);
  mprintf(L"\nCRC32 MT speed: %llu MB/s",BufCountMT*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  Hash.Init(HASH_BLAKE2,MaxPoolThreads);
  for (uint I=0;I<BufCount;I++)
    Hash.Update(Buf,BufSize);
  Hash.Result(&Result);
  mprintf(L"\nBlake2sp speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));

  StartTime=clock();
  r64=0xffffffffffffffff;
  for (uint I=0;I<BufCount;I++)
    r64=CRC64(r64,Buf,BufSize);
  if (r64!=0) // Otherwise compiler optimizer removes CRC calculation.
    mprintf(L"\nCRC64 speed: %llu MB/s",BufCount*CLOCKS_PER_SEC/(clock()-StartTime));
}
#endif
