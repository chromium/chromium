#include "rar.hpp"

void HashValue::Init(HASH_TYPE Type)
{
  HashValue::Type=Type;

  // Zero length data CRC32 is 0. It is important to set it when creating
  // headers with no following data like directories or symlinks.
  if (Type==HASH_RAR14 || Type==HASH_CRC32)
    CRC32=0;
  if (Type==HASH_BLAKE2)
  {
    // dd0e891776933f43c7d032b08a917e25741f8aa9a12c12e1cac8801500f2ca4f
    // is BLAKE2sp hash of empty data. We init the structure to this value,
    // so if we create a file or service header with no following data like
    // "file copy" or "symlink", we set the checksum to proper value avoiding
    // additional header type or size checks when extracting.
    static byte EmptyHash[32]={
      0xdd, 0x0e, 0x89, 0x17, 0x76, 0x93, 0x3f, 0x43,
      0xc7, 0xd0, 0x32, 0xb0, 0x8a, 0x91, 0x7e, 0x25,
      0x74, 0x1f, 0x8a, 0xa9, 0xa1, 0x2c, 0x12, 0xe1,
      0xca, 0xc8, 0x80, 0x15, 0x00, 0xf2, 0xca, 0x4f
    };
    memcpy(Digest,EmptyHash,sizeof(Digest));
  }
}


bool HashValue::operator == (const HashValue &cmp) const
{
  if (Type==HASH_NONE || cmp.Type==HASH_NONE)
    return true;
  if (Type==HASH_RAR14 && cmp.Type==HASH_RAR14 || 
      Type==HASH_CRC32 && cmp.Type==HASH_CRC32)
    return CRC32==cmp.CRC32;
  if (Type==HASH_BLAKE2 && cmp.Type==HASH_BLAKE2)
    return memcmp(Digest,cmp.Digest,sizeof(Digest))==0;
  return false;
}


DataHash::DataHash()
{
  blake2ctx=NULL;
  HashType=HASH_NONE;
#ifdef RAR_SMP
  ThPool=NULL;
  MaxThreads=0;
#endif
}


DataHash::~DataHash()
{
#ifdef RAR_SMP
  delete ThPool;
#endif
  cleandata(&CurCRC32, sizeof(CurCRC32));
  if (blake2ctx!=NULL)
  {
    cleandata(blake2ctx, sizeof(blake2sp_state));
    delete blake2ctx;
  }
}


void DataHash::Init(HASH_TYPE Type,uint MaxThreads)
{
  if (blake2ctx==NULL)
    blake2ctx=new blake2sp_state;
  HashType=Type;
  if (Type==HASH_RAR14)
    CurCRC32=0;
  if (Type==HASH_CRC32)
    CurCRC32=0xffffffff; // Initial CRC32 value.
  if (Type==HASH_BLAKE2)
    blake2sp_init(blake2ctx);
#ifdef RAR_SMP
  DataHash::MaxThreads=Min(MaxThreads,HASH_POOL_THREADS);
#endif
}


void DataHash::Update(const void *Data,size_t DataSize)
{
#ifndef SFX_MODULE
  if (HashType==HASH_RAR14)
    CurCRC32=Checksum14((ushort)CurCRC32,Data,DataSize);
#endif
  if (HashType==HASH_CRC32)
  {
#ifdef RAR_SMP
    UpdateCRC32MT(Data,DataSize);
#else
    CurCRC32=CRC32(CurCRC32,Data,DataSize);
#endif
  }

  if (HashType==HASH_BLAKE2)
  {
#ifdef RAR_SMP
    if (MaxThreads>1 && ThPool==nullptr)
      ThPool=new ThreadPool(HASH_POOL_THREADS);
    blake2ctx->ThPool=ThPool;
    blake2ctx->MaxThreads=MaxThreads;
#endif
    blake2sp_update( blake2ctx, (byte *)Data, DataSize);
  }
}


#ifdef RAR_SMP
THREAD_PROC(BuildCRC32Thread)
{
  DataHash::CRC32ThreadData *td=(DataHash::CRC32ThreadData *)Data;

  // Use 0 initial value to simplify combining the result with existing CRC32.
  // It doesn't affect the first initial 0xffffffff in the data beginning.
  // If we used 0xffffffff here, we would need to shift 0xffffffff left to
  // block width and XOR it with block CRC32 to reset its initial value to 0.
  td->DataCRC=CRC32(0,td->Data,td->DataSize);
}


// CRC is linear and distributive over addition, so CRC(a+b)=CRC(a)+CRC(b).
// Since addition in finite field is XOR, we have CRC(a^b)=CRC(a)^CRC(b).
// So CRC(aaabbb) = CRC(aaa000) ^ CRC(000bbb) = CRC(aaa000) ^ CRC(bbb),
// because CRC ignores leading zeroes. Thus to split CRC calculations
// to "aaa" and "bbb" blocks and then to threads we need to be able to
// find CRC(aaa000) knowing "aaa" quickly. We use Galois finite field to
// calculate the power of 2 to get "1000" and multiply it by "aaa".
void DataHash::UpdateCRC32MT(const void *Data,size_t DataSize)
{
  const size_t MinBlock=0x4000;
  if (DataSize<2*MinBlock || MaxThreads<2)
  {
    CurCRC32=CRC32(CurCRC32,Data,DataSize);
    return;
  }

  if (ThPool==nullptr)
    ThPool=new ThreadPool(HASH_POOL_THREADS);

  size_t Threads=MaxThreads;
  size_t BlockSize=DataSize/Threads;
  if (BlockSize<MinBlock)
  {
    BlockSize=MinBlock;
    Threads=DataSize/BlockSize;
  }

  CRC32ThreadData td[MaxPoolThreads];

//#undef USE_THREADS
  for (size_t I=0;I<Threads;I++)
  {
    td[I].Data=(byte*)Data+I*BlockSize;
    td[I].DataSize=(I+1==Threads) ? DataSize-I*BlockSize : BlockSize;
#ifdef USE_THREADS
    ThPool->AddTask(BuildCRC32Thread,(void*)&td[I]);
#else
    BuildCRC32Thread((void*)&td[I]);
#endif
  }

#ifdef USE_THREADS
  ThPool->WaitDone();
#endif // USE_THREADS

  uint StdShift=gfExpCRC(uint(8*td[0].DataSize));
  for (size_t I=0;I<Threads;I++)
  {
    // Prepare the multiplier to shift CRC to proper position.
    uint ShiftMult;
    if (td[I].DataSize==td[0].DataSize)
      ShiftMult=StdShift; // Reuse the shift value for typical block size.
    else
      ShiftMult=gfExpCRC(uint(8*td[I].DataSize)); // 2 power "shift bits".

    // To combine the cumulative total and current block CRC32, we multiply
    // the total data CRC32 to shift value to place it to proper position.
    // Invoke BitReverse32(), because 0xEDB88320 is the reversed polynomial.
    // Alternatively we could adjust the multiplication function for reversed
    // polynomials, but it would make it less readable without real speed gain.
    // If CRC32 threads used 0xffffffff initial value, we would need
    // to XOR the total data CRC32 with 0xffffffff before multiplication,
    // so 0xffffffff is also shifted left to current block width and replaces
    // the initial 0xffffffff CRC32 value with 0 in the current block CRC32
    // after XOR'ing it with total data CRC32. Since now CRC32 threads use 0
    // initial value, this is not necessary.
    CurCRC32=BitReverse32(gfMulCRC(BitReverse32(CurCRC32), ShiftMult));

    // Combine the total data and current block CRC32.
    CurCRC32^=td[I].DataCRC;
  }
}
#endif


uint DataHash::BitReverse32(uint N)
{
  uint Reversed=0;
  for (uint I=0;I<32;I++,N>>=1)
    Reversed|=(N & 1)<<(31-I);
  return Reversed;
}


// Galois field multiplication modulo POLY.
uint DataHash::gfMulCRC(uint A, uint B)
{
  // For reversed 0xEDB88320 polynomial we bit reverse CRC32 before passing
  // to this function, so we must use the normal polynomial here.
  // We set the highest polynomial bit 33 for proper multiplication
  // in case uint is larger than 32-bit.
  const uint POLY=uint(0x104c11db7);

  uint R = 0 ; // Multiplication result.
  while (A != 0 && B != 0) // If any of multipliers becomes 0, quit early.
  {
    // For non-zero lowest B bit, add A to result.
    R ^= (B & 1)!=0 ? A : 0;

    // Make A twice larger before the next iteration.
    // Subtract POLY to keep it modulo POLY if high bit is set.
    A  = (A << 1) ^ ((A & 0x80000000)!=0 ? POLY : 0);

    B >>= 1; // Move next B bit to lowest position.
  }
  return R;
}


// Calculate 2 power N with square-and-multiply algorithm.
uint DataHash::gfExpCRC(uint N)
{
  uint S = 2; // Starts from base value and contains the current square.
  uint R = 1; // Exponentiation result.
  while (N > 1)
  {
    if ((N & 1)!=0)     // If N is odd.
      R = gfMulCRC(R, S);
    S = gfMulCRC(S, S); // Next square.
    N >>= 1;
  }
  // We could change the loop condition to N > 0 and return R at expense
  // of one additional gfMulCRC(S, S).
  return gfMulCRC(R, S);
}


void DataHash::Result(HashValue *Result)
{
  Result->Type=HashType;
  if (HashType==HASH_RAR14)
    Result->CRC32=CurCRC32;
  if (HashType==HASH_CRC32)
    Result->CRC32=CurCRC32^0xffffffff;
  if (HashType==HASH_BLAKE2)
  {
    // Preserve the original context, so we can continue hashing if necessary.
    blake2sp_state res=*blake2ctx;
    blake2sp_final(&res,Result->Digest);
  }
}


uint DataHash::GetCRC32()
{
  return HashType==HASH_CRC32 ? CurCRC32^0xffffffff : 0;
}


bool DataHash::Cmp(HashValue *CmpValue,byte *Key)
{
  HashValue Final;
  Result(&Final);
  if (Key!=NULL)
    ConvertHashToMAC(&Final,Key);
  return Final==*CmpValue;
}
