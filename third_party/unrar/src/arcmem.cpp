ArcMemory::ArcMemory()
{
  Loaded=false;
  SeekPos=0;
}


void ArcMemory::Load(const byte *Data,size_t Size)
{
  ArcData.Alloc(Size);
  memcpy(&ArcData[0],Data,Size);
  Loaded=true;
  SeekPos=0;
}


bool ArcMemory::Unload()
{
  if (!Loaded)
    return false;
  Loaded=false;
  return true;
}


bool ArcMemory::Read(void *Data,size_t Size,size_t &Result)
{
  if (!Loaded)
    return false;
  Result=(size_t)Min(Size,ArcData.Size()-SeekPos);
  memcpy(Data,&ArcData[(size_t)SeekPos],Result);
  SeekPos+=Result;
  return true;
}


bool ArcMemory::Seek(int64 Offset,int Method)
{
  if (!Loaded)
    return false;
  if (Method==SEEK_SET)
  {
    if (Offset<0)
      SeekPos=0;
    else
      SeekPos=Min((uint64)Offset,ArcData.Size());
  }
  else
    if (Method==SEEK_CUR || Method==SEEK_END)
    {
      if (Method==SEEK_END)
        SeekPos=ArcData.Size();
      SeekPos+=(uint64)Offset;
      if (SeekPos>ArcData.Size())
        SeekPos=Offset<0 ? 0 : ArcData.Size();
    }
  return true;
}


bool ArcMemory::Tell(int64 *Pos)
{
  if (!Loaded)
    return false;
  *Pos=SeekPos;
  return true;
}
