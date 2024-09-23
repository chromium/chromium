

#ifdef _WIN_ALL
// StreamName must include the leading ':'.
static bool IsNtfsReservedStream(const std::wstring &StreamName)
{
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
  if (FileName.size()==1)
  {
    // Convert single character names like f:stream to .\f:stream to
    // resolve the ambiguity with drive letters.
    StreamName=L".\\"+FileName;
  }
  else
    StreamName=FileName;
  if (Arc.StreamHead.StreamName[0]!=':')
  {
    uiMsg(UIERROR_STREAMBROKEN,Arc.FileName,FileName);
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  std::wstring StoredName;
  // "substr(1)" to exclude ':', so we can use ConvertPath() below.
  CharToWide(Arc.StreamHead.StreamName.substr(1),StoredName);
  ConvertPath(&StoredName,&StoredName);


  StoredName=L":"+StoredName;
  if (IsNtfsReservedStream(StoredName))
    return;

  StreamName+=StoredName;

  FindData FD;
  bool Found=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);

  File CurFile;
  if (CurFile.WCreate(StreamName))
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
  File HostFile;
  if (Found && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);
  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr);
}
#endif


#ifdef _WIN_ALL
void ExtractStreams(Archive &Arc,const std::wstring &FileName,bool TestMode)
{
  std::wstring FullName;
  if (FileName[0]!=0 && FileName[1]==0)
  {
    // Convert single character names like f:stream to .\f:stream to
    // resolve the ambiguity with drive letters.
    FullName=L".\\"+FileName;
  }
  else
    FullName=FileName;

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
    Arc.ReadSubData(NULL,&CurFile,true);
    return;
  }

  FullName+=StreamName;


  if (IsNtfsReservedStream(StreamName))
    return;

  FindData FD;
  bool HostFound=FindFile::FastFind(FileName,&FD);

  if ((FD.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,FD.FileAttr & ~FILE_ATTRIBUTE_READONLY);
  File CurFile;

  if (CurFile.WCreate(FullName))
  {
    if (Arc.ReadSubData(NULL,&CurFile,false))
      CurFile.Close();
  }

  // Restoring original file timestamps.
  File HostFile;
  if (HostFound && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&FD.ftCreationTime,&FD.ftLastAccessTime,
                &FD.ftLastWriteTime);

  // Restoring original file attributes. Important if file was read only
  // or did not have "Archive" attribute
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
    std::vector<byte> Src=Arc.SubHead.SubData;
    Src.push_back(0); // Needed for our UtfToWide.
    UtfToWide((char *)Src.data(),Dest);
  }
  return Dest;
}
