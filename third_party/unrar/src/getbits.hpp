#ifndef _RAR_GETBITS_
#define _RAR_GETBITS_

class BitInput
{
  public:
    enum BufferSize {MAX_SIZE=0x8000}; // Size of input buffer.

    int InAddr; // Curent byte position in the buffer.
    int InBit;  // Current bit position in the current byte.

    bool ExternalBuffer;
  public:
    BitInput(bool AllocBuffer);
    ~BitInput();

    byte *InBuf; // Dynamically allocated input buffer.

    void InitBitInput()
    {
      InAddr=InBit=0;
    }
    
    // Move forward by 'Bits' bits.
    void addbits(uint Bits)
    {
      Bits+=InBit;
      InAddr+=Bits>>3;
      InBit=Bits&7;
    }

    // Return 16 bits from current position in the buffer.
    // Bit at (InAddr,InBit) has the highest position in returning data.
    uint getbits()
    {
#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
      uint32 BitField=RawGetBE4(InBuf+InAddr);
      BitField >>= (16-InBit);
#else
      uint BitField=(uint)InBuf[InAddr] << 16;
      BitField|=(uint)InBuf[InAddr+1] << 8;
      BitField|=(uint)InBuf[InAddr+2];
      BitField >>= (8-InBit);
#endif
      return BitField & 0xffff;
    }


    // Return 32 bits from current position in the buffer.
    // Bit at (InAddr,InBit) has the highest position in returning data.
    uint getbits32()
    {
      uint BitField=RawGetBE4(InBuf+InAddr);
      BitField <<= InBit;
      BitField|=(uint)InBuf[InAddr+4] >> (8-InBit);
      return BitField & 0xffffffff;
    }

    // Return 64 bits from current position in the buffer.
    // Bit at (InAddr,InBit) has the highest position in returning data.
    uint64 getbits64()
    {
      uint64 BitField=RawGetBE8(InBuf+InAddr);
      BitField <<= InBit;
      BitField|=(uint)InBuf[InAddr+8] >> (8-InBit);
      return BitField;
    }
    
    void faddbits(uint Bits);
    uint fgetbits();
    
    // Check if buffer has enough space for IncPtr bytes. Returns 'true'
    // if buffer will be overflown.
    bool Overflow(uint IncPtr) 
    {
      return InAddr+IncPtr>=MAX_SIZE;
    }

    void SetExternalBuffer(byte *Buf);
};
#endif
