#ifndef _RAR_DATAHASH_
#define _RAR_DATAHASH_

enum HASH_TYPE {HASH_NONE,HASH_RAR14,HASH_CRC32,HASH_BLAKE2};

struct HashValue
{
  void Init(HASH_TYPE Type);

  // Use the const member, so types on both sides of "==" match.
  // Otherwise clang -std=c++20 issues "ambiguity is between a regular call
  // to this operator and a call with the argument order reversed" warning.
  bool operator == (const HashValue &cmp) const;

  // Not actually used now. Const member for same reason as operator == above.
  bool operator != (const HashValue &cmp) const {return !(*this==cmp);}

  HASH_TYPE Type;
  union
  {
    uint CRC32;
    byte Digest[SHA256_DIGEST_SIZE];
  };
};


#ifdef RAR_SMP
class ThreadPool;
class DataHash;
#endif


class DataHash
{
  public:
    struct CRC32ThreadData
    {
      void *Data;
      size_t DataSize;
      uint DataCRC;
    };
  private:
    void UpdateCRC32MT(const void *Data,size_t DataSize);
    uint BitReverse32(uint N);
    uint gfMulCRC(uint A, uint B);
    uint gfExpCRC(uint N);

    // Speed gain seems to vanish above 8 CRC32 threads.
    static const uint CRC32_POOL_THREADS=8;
    // Thread pool must allow at least BLAKE2_THREADS_NUMBER threads.
    static const uint HASH_POOL_THREADS=Max(BLAKE2_THREADS_NUMBER,CRC32_POOL_THREADS);

    HASH_TYPE HashType;
    uint CurCRC32;
    blake2sp_state *blake2ctx;

#ifdef RAR_SMP
    ThreadPool *ThPool;

    uint MaxThreads;
#endif
  public:
    DataHash();
    ~DataHash();
    void Init(HASH_TYPE Type,uint MaxThreads);
    void Update(const void *Data,size_t DataSize);
    void Result(HashValue *Result);
    uint GetCRC32();
    bool Cmp(HashValue *CmpValue,byte *Key);
    HASH_TYPE Type() {return HashType;}
};

#endif
