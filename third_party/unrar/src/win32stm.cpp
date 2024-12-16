

#ifdef _WIN_ALL
// StreamName must include the leading ':'.
static bool IsNtfsProhibitedStream(const std::wstring &StreamName)
{
  // 2024.03.14: We replaced the predefined names check with simpler
  // "no more than a single colon" check. Second colon could be used to
  // define the type of alternate stream, but RAR archives work only with
  // data streams and do not store :$DATA type in archive. It is assumed.
  // So there is no legitimate use for stream type inside of archive,
  // but it can be abused to hide the actual file data in file::$DATA
  // or hide the actual MOTW data in Zone.Identifier:$DATA.
  uint ColonCount=0;
  for (wchar Ch:StreamName)
    if (Ch==':' && ++ColonCount>1)
      return true;
  return false;
/*
  const wchar *Reserved[]{
    L"::$ATTRIBUTE_LIST",L"::$BITMAP",L"::$DATA",L"::$EA",L"::$EA_INFORMATION",
    L"::$FILE_NAME",L"::$INDEX_ALLOCATION",L":$I30:$INDEX_ALLOCATION",
    L"::$INDEX_ROOT",L"::$LOGGED_UTILITY_STREAM",L":$EFS:$LOGGED_UTILITY_STREAM",
    L":$TXF_DATA:$LOGGED_UTILITY_STREAM",L"::$OBJECT_ID",L"::$REPARSE_POINT"
  };
  for (const wchar *Name : Reserved)
    if (wcsicomp(StreamName,Name)==0)
      return true;
  return false;
*/
}
#endif


#if !defined(SFX_MODULE) && defined(_WIN_ALL)
void ExtractStreams20(Archive &Arc,const std::wstring &FileName)
{
  if (Arc.BrokenHeader)
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (Arc.StreamHead.Method<0x31 || Arc.StreamHead.Method>0x35 || Arc.StreamHead.UnpVer>VER_PACK)
  {
    uiMsg(UIERROR_STREAMUNKNOWN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }

  std::wstring StreamName;
  CharToWide(Arc.StreamHead.StreamName,StreamName);

  if (StreamName[0]!=':')
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  // Convert single character names like f:stream to .\f:stream to
  // resolve the ambiguity with drive letters.
  std::wstring FullName=FileName.size()==1 ? L".\\"+FileName:FileName;
  FullName+=StreamName;

#ifdef PROPAGATE_MOTW
  // 2022.10.31: Can't easily read RAR 2.0 stream data here, so if we already
  // propagated the archive Zone.Identifier stream, also known as Mark of
  // the Web, to extracted file, we do not overwrite it here.
  if (Arc.Motw.IsNameConflicting(StreamName))
    return;

  // 2024.02.03: Prevent using Zone.Identifier:$DATA to overwrite Zone.Identifier
  // according to ZDI-CAN-23156 Trend Micro report.
  // 2024.03.14: Not needed after adding check for 2+ ':' in IsNtfsProhibitedStream(().
  // if (wcsnicomp(StreamName,L":Zone.Identifier:",17)==0)
  //  return;
#endif

  if (IsNtfsProhibitedStream(StreamName))
    return;

  FindData FD;
  bool HostFound=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);

  File CurFile;
  if (CurFile.WCreate(FullName))
  {
    ComprDataIO DataIO;
    Unpack Unpack(&DataIO);
    Unpack.Init(0x10000,false);

    DataIO.SetPackedSizeToRead(Arc.StreamHead.DataSize);
    DataIO.EnableShowProgress(false);
    DataIO.SetFiles(&Arc,&CurFile);
    DataIO.UnpHash.Init(HASH_CRC32,1);
    Unpack.SetDestSize(Arc.StreamHead.UnpSize);
    Unpack.DoUnpack(Arc.StreamHead.UnpVer,false);

    if (Arc.StreamHead.StreamCRC!=DataIO.UnpHash.GetCRC32())
    {
      uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,StreamName);
      ErrHandler.SetErrorCode(RARX_CRC);
    }
    else
      CurFile.Close();
  }

  // Restoring original file timestamps.
  File HostFile;
  if (HostFound && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);

  // Restoring original file attributes.
  // Important if file was read only or did not have "Archive" attribute.
  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr);
}
#endif


#ifdef _WIN_ALL
void ExtractStreams(Archive &Arc,const std::wstring &FileName,bool TestMode)
{
  std::wstring StreamName=GetStreamNameNTFS(Arc);
  if (StreamName[0]!=':')
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (TestMode)
  {
    File CurFile;
    Arc.ReadSubData(nullptr,&CurFile,true);
    return;
  }

  // Convert single character names like f:stream to .\f:stream to
  // resolve the ambiguity with drive letters.
  std::wstring FullName=FileName.size()==1 ? L".\\"+FileName:FileName;
  FullName+=StreamName;

#ifdef PROPAGATE_MOTW
  // 2022.10.31: If we already propagated the archive Zone.Identifier stream,
  // also known as Mark of the Web, to extracted file, we overwrite it here
  // only if file zone is stricter. Received a user request for such behavior.

  std::string ParsedMotw;
  if (Arc.Motw.IsNameConflicting(StreamName))
  {
    // Do not worry about excessive memory allocation, ReadSubData prevents it.
    std::vector<byte> FileMotw;
    if (!Arc.ReadSubData(&FileMotw,nullptr,false))
      return;
    ParsedMotw.assign(FileMotw.begin(),FileMotw.end());

    // We already set the archive stream. If file stream value isn't more
    // restricted, we do not want to write it over the existing archive stream.
    if (!Arc.Motw.IsFileStreamMoreSecure(ParsedMotw))
      return;
  }

  // 2024.02.03: Prevent using :Zone.Identifier:$DATA to overwrite :Zone.Identifier
  // according to ZDI-CAN-23156 Trend Micro report.
  // 2024.03.14: Not needed after adding check for 2+ ':' in IsNtfsProhibitedStream(().
  // if (wcsnicomp(StreamName,L":Zone.Identifier:",17)==0)
  //  return;
#endif

  if (IsNtfsProhibitedStream(StreamName))
    return;

  FindData FD;
  bool HostFound=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);
  File CurFile;

  if (CurFile.WCreate(FullName))
  {
#ifdef PROPAGATE_MOTW
    if (!ParsedMotw.empty())
    {
      // The archive propagated security zone is either missing
      // or less strict than file one. Write the file security zone here.
      CurFile.Write(ParsedMotw.data(),ParsedMotw.size());
      CurFile.Close();
    }
    else
#endif
    if (Arc.ReadSubData(nullptr,&CurFile,false))
      CurFile.Close();
  }

  // Restoring original file timestamps.
  File HostFile;
  if (HostFound && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);

  // Restoring original file attributes.
  // Important if file was read only or did not have "Archive" attribute.
  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr);
}
#endif


std::wstring GetStreamNameNTFS(Archive &Arc)
{
  std::wstring Dest;
  if (Arc.Format==RARFMT15)
    Dest=RawToWide(Arc.SubHead.SubData);
  else
  {
    std::string Src(Arc.SubHead.SubData.begin(),Arc.SubHead.SubData.end());
    UtfToWide(Src.data(),Dest);
  }
  return Dest;
}
