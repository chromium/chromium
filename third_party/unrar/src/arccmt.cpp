static bool IsAnsiEscComment(const wchar *Data,size_t Size);

bool Archive::GetComment(std::wstring &CmtData)
{
  if (!MainComment)
    return false;
  int64 SavePos=Tell();
  bool Success=DoGetComment(CmtData);
  Seek(SavePos,SEEK_SET);
  return Success;
}


bool Archive::DoGetComment(std::wstring &CmtData)
{
#ifndef SFX_MODULE
  uint CmtLength;
  if (Format==RARFMT14)
  {
    Seek(SFXSize+SIZEOF_MAINHEAD14,SEEK_SET);
    CmtLength=GetByte();
    CmtLength+=(GetByte()<<8);
  }
  else
#endif
  {
    if (MainHead.CommentInHeader)
    {
      // Old style (RAR 2.9) archive comment embedded into the main 
      // archive header.
      Seek(SFXSize+SIZEOF_MARKHEAD3+SIZEOF_MAINHEAD3,SEEK_SET);
      if (!ReadHeader() || GetHeaderType()!=HEAD3_CMT)
        return false;
    }
    else
    {
      // Current (RAR 3.0+) version of archive comment.
      Seek(GetStartPos(),SEEK_SET);
      return SearchSubBlock(SUBHEAD_TYPE_CMT)!=0 && ReadCommentData(CmtData);
    }
#ifndef SFX_MODULE
    // Old style (RAR 2.9) comment header embedded into the main 
    // archive header.
    if (BrokenHeader || CommHead.HeadSize<SIZEOF_COMMHEAD)
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
    CmtLength=CommHead.HeadSize-SIZEOF_COMMHEAD;
#endif
  }
#ifndef SFX_MODULE
  if (Format==RARFMT14 && MainHead.PackComment || Format!=RARFMT14 && CommHead.Method!=0x30)
  {
    if (Format!=RARFMT14 && (CommHead.UnpVer < 15 || CommHead.UnpVer > VER_UNPACK || CommHead.Method > 0x35))
      return false;
    ComprDataIO DataIO;
    DataIO.SetTestMode(true);
    uint UnpCmtLength;
    if (Format==RARFMT14)
    {
#ifdef RAR_NOCRYPT
      return false;
#else
      UnpCmtLength=GetByte();
      UnpCmtLength+=(GetByte()<<8);
      if (CmtLength<2)
        return false;
      CmtLength-=2;
      DataIO.SetCmt13Encryption();
      CommHead.UnpVer=15;
#endif
    }
    else
      UnpCmtLength=CommHead.UnpSize;
    DataIO.SetFiles(this,NULL);
    DataIO.EnableShowProgress(false);
    DataIO.SetPackedSizeToRead(CmtLength);
    DataIO.UnpHash.Init(HASH_CRC32,1);
    DataIO.SetNoFileHeader(true); // this->FileHead is not filled yet.

    Unpack CmtUnpack(&DataIO);
    CmtUnpack.Init(0x10000,false);
    CmtUnpack.SetDestSize(UnpCmtLength);
    CmtUnpack.DoUnpack(CommHead.UnpVer,false);

    if (Format!=RARFMT14 && (DataIO.UnpHash.GetCRC32()&0xffff)!=CommHead.CommCRC)
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
    else
    {
      byte *UnpData;
      size_t UnpDataSize;
      DataIO.GetUnpackedData(&UnpData,&UnpDataSize);
      if (UnpDataSize>0)
      {
#ifdef _WIN_ALL
        // If we ever decide to extend it to Android, we'll need to alloc
        // 4x memory for OEM to UTF-8 output here.
        OemToCharBuffA((char *)UnpData,(char *)UnpData,(DWORD)UnpDataSize);
#endif
        std::string UnpStr((char*)UnpData,UnpDataSize);
        CharToWide(UnpStr,CmtData);
      }
    }
  }
  else
  {
    if (CmtLength==0)
      return false;
    std::vector<byte> CmtRaw(CmtLength);
    int ReadSize=Read(CmtRaw.data(),CmtLength);
    if (ReadSize>=0 && (uint)ReadSize<CmtLength) // Comment is shorter than declared.
    {
      CmtLength=ReadSize;
      CmtRaw.resize(CmtLength);
    }

    if (Format!=RARFMT14 && CommHead.CommCRC!=(~CRC32(0xffffffff,&CmtRaw[0],CmtLength)&0xffff))
    {
      uiMsg(UIERROR_CMTBROKEN,FileName);
      return false;
    }
//    CmtData.resize(CmtLength+1);
    CmtRaw.push_back(0);
#ifdef _WIN_ALL
    // If we ever decide to extend it to Android, we'll need to alloc
    // 4x memory for OEM to UTF-8 output here.
    OemToCharA((char *)CmtRaw.data(),(char *)CmtRaw.data());
#endif
    CharToWide((const char *)CmtRaw.data(),CmtData);
//    CmtData->resize(wcslen(CmtData->data()));
  }
#endif
  return CmtData.size() > 0;
}


bool Archive::ReadCommentData(std::wstring &CmtData)
{
  std::vector<byte> CmtRaw;
  if (!ReadSubData(&CmtRaw,NULL,false))
    return false;
  size_t CmtSize=CmtRaw.size();
  CmtRaw.push_back(0);
//  CmtData->resize(CmtSize+1);
  if (Format==RARFMT50)
    UtfToWide((char *)CmtRaw.data(),CmtData);
  else
    if ((SubHead.SubFlags & SUBHEAD_FLAGS_CMT_UNICODE)!=0)
    {
      CmtData=RawToWide(CmtRaw);
    }
    else
    {
      CharToWide((const char *)CmtRaw.data(),CmtData);
    }
//  CmtData->resize(wcslen(CmtData->data())); // Set buffer size to actual comment length.
  return true;
}


void Archive::ViewComment()
{
  if (Cmd->DisableComment)
    return;
  std::wstring CmtBuf;
  if (GetComment(CmtBuf)) // In GUI too, so "Test" command detects broken comments.
  {
    size_t CmtSize=CmtBuf.size();
    auto EndPos=CmtBuf.find(0x1A);
    if (EndPos!=std::wstring::npos)
      CmtSize=EndPos;
    mprintf(St(MArcComment));
    mprintf(L":\n");
    OutComment(CmtBuf);
  }
}


