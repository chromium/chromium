_forceinline void Unpack::InsertOldDist(size_t Distance)
{
  OldDist[3]=OldDist[2];
  OldDist[2]=OldDist[1];
  OldDist[1]=OldDist[0];
  OldDist[0]=Distance;
}

#if defined(LITTLE_ENDIAN) && defined(ALLOW_MISALIGNED)
#define UNPACK_COPY8 // We can copy 8 bytes at any position as uint64.
#endif

_forceinline void Unpack::CopyString(uint Length,size_t Distance)
{
  size_t SrcPtr=UnpPtr-Distance;

  // Perform the correction here instead of "else", so matches crossing
  // the window beginning can also be processed by first "if" part.
  if (Distance>UnpPtr) // Unlike SrcPtr>=MaxWinSize, it catches invalid distances like 0xfffffff0 in 32-bit build.
  {
    // Same as WrapDown(SrcPtr), needed because of UnpPtr-Distance above.
    // We need the same condition below, so we expanded WrapDown() here.
    SrcPtr+=MaxWinSize;

    // About Distance>MaxWinSize check.
    // SrcPtr can be >=MaxWinSize if distance exceeds MaxWinSize
    // in a malformed archive. Our WrapDown replacement above might not
    // correct it, so to prevent out of bound Window read we check it here.
    // Unlike SrcPtr>=MaxWinSize check, it allows MaxWinSize>0x80000000
    // in 32-bit build, which could cause overflow in SrcPtr.
    // About !FirstWinDone check.
    // If first window hasn't filled yet and it points outside of window,
    // set data to 0 instead of copying preceding file data, so result doesn't
    // depend on previously extracted files in non-solid archive.
    if (Distance>MaxWinSize || !FirstWinDone)
    {
      // Fill area of specified length with 0 instead of returning.
      // So if only the distance is broken and rest of packed data is valid,
      // it preserves offsets and allows to continue extraction.
      // If we set SrcPtr to random offset instead, let's say, 0,
      // we still will be copying preceding file data if UnpPtr is also 0.
      while (Length-- > 0)
      {
        Window[UnpPtr]=0;
        UnpPtr=WrapUp(UnpPtr+1);
      }
      return;
    }
  }

  if (SrcPtr<MaxWinSize-MAX_INC_LZ_MATCH && UnpPtr<MaxWinSize-MAX_INC_LZ_MATCH)
  {
    // If we are not close to end of window, we do not need to waste time
    // to WrapUp and WrapDown position protection.

    byte *Src=Window+SrcPtr;
    byte *Dest=Window+UnpPtr;
    UnpPtr+=Length;

#ifdef UNPACK_COPY8
    if (Distance<Length) // Overlapping strings
#endif
      while (Length>=8)
      {
        Dest[0]=Src[0];
        Dest[1]=Src[1];
        Dest[2]=Src[2];
        Dest[3]=Src[3];
        Dest[4]=Src[4];
        Dest[5]=Src[5];
        Dest[6]=Src[6];
        Dest[7]=Src[7];

        Src+=8;
        Dest+=8;
        Length-=8;
      }
#ifdef UNPACK_COPY8
    else
      while (Length>=8)
      {
        // In theory we still could overlap here.
        // Supposing Distance == MaxWinSize - 1 we have memcpy(Src, Src + 1, 8).
        // But for real RAR archives Distance <= MaxWinSize - MAX_INC_LZ_MATCH
        // always, so overlap here is impossible.

        RawPut8(RawGet8(Src),Dest);

        Src+=8;
        Dest+=8;
        Length-=8;
      }
#endif

    // Unroll the loop for 0 - 7 bytes left. Note that we use nested "if"s.
    if (Length>0) { Dest[0]=Src[0];
    if (Length>1) { Dest[1]=Src[1];
    if (Length>2) { Dest[2]=Src[2];
    if (Length>3) { Dest[3]=Src[3];
    if (Length>4) { Dest[4]=Src[4];
    if (Length>5) { Dest[5]=Src[5];
    if (Length>6) { Dest[6]=Src[6]; } } } } } } } // Close all nested "if"s.
  }
  else
    while (Length-- > 0) // Slow copying with all possible precautions.
    {
      Window[UnpPtr]=Window[WrapUp(SrcPtr++)];
      // We need to have masked UnpPtr after quit from loop, so it must not
      // be replaced with 'Window[WrapUp(UnpPtr++)]'
      UnpPtr=WrapUp(UnpPtr+1);
    }
}


_forceinline uint Unpack::DecodeNumber(BitInput &Inp,DecodeTable *Dec)
{
  // Left aligned 15 bit length raw bit field.
  uint BitField=Inp.getbits() & 0xfffe;

  if (BitField<Dec->DecodeLen[Dec->QuickBits])
  {
    uint Code=BitField>>(16-Dec->QuickBits);
    Inp.addbits(Dec->QuickLen[Code]);
    return Dec->QuickNum[Code];
  }

  // Detect the real bit length for current code.
  uint Bits=15;
  for (uint I=Dec->QuickBits+1;I<15;I++)
    if (BitField<Dec->DecodeLen[I])
    {
      Bits=I;
      break;
    }

  Inp.addbits(Bits);
  
  // Calculate the distance from the start code for current bit length.
  uint Dist=BitField-Dec->DecodeLen[Bits-1];

  // Start codes are left aligned, but we need the normal right aligned
  // number. So we shift the distance to the right.
  Dist>>=(16-Bits);

  // Now we can calculate the position in the code list. It is the sum
  // of first position for current bit length and right aligned distance
  // between our bit field and start code for current bit length.
  uint Pos=Dec->DecodePos[Bits]+Dist;

  // Out of bounds safety check required for damaged archives.
  if (Pos>=Dec->MaxNum)
    Pos=0;

  // Convert the position in the code list to position in alphabet
  // and return it.
  return Dec->DecodeNum[Pos];
}


_forceinline uint Unpack::SlotToLength(BitInput &Inp,uint Slot)
{
  uint LBits,Length=2;
  if (Slot<8)
  {
    LBits=0;
    Length+=Slot;
  }
  else
  {
    LBits=Slot/4-1;
    Length+=(4 | (Slot & 3)) << LBits;
  }

  if (LBits>0)
  {
    Length+=Inp.getbits()>>(16-LBits);
    Inp.addbits(LBits);
  }
  return Length;
}
