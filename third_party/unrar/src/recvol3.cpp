// Buffer size for all volumes involved.
static const size_t TotalBufferSize=0x4000000;

class RSEncode // Encode or decode data area, one object per one thread.
{
  private:
    RSCoder RSC;
  public:
    void EncodeBuf();
    void DecodeBuf();

    void Init(int RecVolNumber) {RSC.Init(RecVolNumber);}
    byte *Buf;
    byte *OutBuf;
    int BufStart;
    int BufEnd;
    int FileNumber;
    int RecVolNumber;
    size_t RecBufferSize;
    int *Erasures;
    int EraSize;
};


#ifdef RAR_SMP
THREAD_PROC(RSDecodeThread)
{
  RSEncode *rs=(RSEncode *)Data;
  rs->DecodeBuf();
}
#endif

RecVolumes3::RecVolumes3(CommandData *Cmd,bool TestOnly)
{
  memset(SrcFile,0,sizeof(SrcFile));
  if (TestOnly)
  {
#ifdef RAR_SMP
    RSThreadPool=NULL;
#endif
  }
  else
  {
    Buf.resize(TotalBufferSize);
#ifdef RAR_SMP
    RSThreadPool=new ThreadPool(Cmd->Threads);
#endif
  }
}


RecVolumes3::~RecVolumes3()
{
  for (size_t I=0;I<ASIZE(SrcFile);I++)
    delete SrcFile[I];
#ifdef RAR_SMP
  delete RSThreadPool;
#endif
}




// Check for names like arc5_3_1.rev created by RAR 3.0.
static bool IsNewStyleRev(const std::wstring &Name)
{
  size_t ExtPos=GetExtPos(Name);
  if (ExtPos==std::wstring::npos || ExtPos==0)
    return true;
  int DigitGroup=0;
  for (ExtPos--;ExtPos>0;ExtPos--)
    if (!IsDigit(Name[ExtPos]))
      if (Name[ExtPos]=='_' && IsDigit(Name[ExtPos-1]))
        DigitGroup++;
      else
        break;
  return DigitGroup<2;
}


bool RecVolumes3::Restore(CommandData *Cmd,const std::wstring &Name,bool Silent)
{
  std::wstring ArcName=Name;
  bool NewStyle=false; // New style .rev volumes are supported since RAR 3.10.
  bool RevName=CmpExt(ArcName,L"rev");
  if (RevName)
  {
    NewStyle=IsNewStyleRev(ArcName);
  
    size_t ExtPos=GetExtPos(ArcName);
    while (ExtPos>1 && (IsDigit(ArcName[ExtPos-1]) || ArcName[ExtPos-1]=='_'))
      ExtPos--;
    ArcName.replace(ExtPos,std::wstring::npos,L"*.*");
    
    FindFile Find;
    Find.SetMask(ArcName);
    FindData fd;
    while (Find.Next(&fd))
    {
      Archive Arc(Cmd);
      if (Arc.WOpen(fd.Name) && Arc.IsArchive(true))
      {
        ArcName=fd.Name;
        break;
      }
    }
  }

  Archive Arc(Cmd);
  if (!Arc.WCheckOpen(ArcName))
    return false;
  if (!Arc.Volume)
  {
    uiMsg(UIERROR_NOTVOLUME,ArcName);
    return false;
  }
  bool NewNumbering=Arc.NewNumbering;
  Arc.Close();

  size_t VolNumStart=VolNameToFirstName(ArcName,ArcName,NewNumbering);
  std::wstring RecVolMask=ArcName;
  RecVolMask.replace(VolNumStart,std::wstring::npos,L"*.rev");
  size_t BaseNamePartLength=VolNumStart;

  int64 RecFileSize=0;

  // We cannot display "Calculating CRC..." message here, because we do not
  // know if we'll find any recovery volumes. We'll display it after finding
  // the first recovery volume.
  bool CalcCRCMessageDone=false;

  FindFile Find;
  Find.SetMask(RecVolMask);
  FindData RecData;
  int FileNumber=0,RecVolNumber=0,FoundRecVolumes=0,MissingVolumes=0;
  std::wstring PrevName;
  while (Find.Next(&RecData))
  {
    std::wstring CurName=RecData.Name;
    int P[3];
    if (!RevName && !NewStyle)
    {
      NewStyle=true;

      size_t DotPos=GetExtPos(CurName);
      if (DotPos!=std::wstring::npos)
      {
        uint LineCount=0;
        DotPos--;
        while (DotPos>0 && CurName[DotPos]!='.')
        {
          if (CurName[DotPos]=='_')
            LineCount++;
          DotPos--;
        }
        if (LineCount==2)
          NewStyle=false;
      }
    }
    if (NewStyle)
    {
      if (!CalcCRCMessageDone)
      {
        uiMsg(UIMSG_RECVOLCALCCHECKSUM);
        CalcCRCMessageDone=true;
      }
      
      uiMsg(UIMSG_STRING,CurName);

      File CurFile;
      CurFile.TOpen(CurName);
      CurFile.Seek(0,SEEK_END);
      int64 Length=CurFile.Tell();
      CurFile.Seek(Length-7,SEEK_SET);
      for (int I=0;I<3;I++)
        P[2-I]=CurFile.GetByte()+1;
      uint FileCRC=0;
      for (int I=0;I<4;I++)
        FileCRC|=CurFile.GetByte()<<(I*8);
      uint CalcCRC;
      CalcFileSum(&CurFile,&CalcCRC,NULL,Cmd->Threads,Length-4);
      if (FileCRC!=CalcCRC)
      {
        uiMsg(UIMSG_CHECKSUM,CurName);
        continue;
      }
    }
    else
    {
      size_t DotPos=GetExtPos(CurName);
      if (DotPos==std::wstring::npos)
        continue;
      bool WrongParam=false;
      for (size_t I=0;I<ASIZE(P);I++)
      {
        do
        {
          DotPos--;
        } while (IsDigit(CurName[DotPos]) && DotPos>=BaseNamePartLength);
        P[I]=atoiw(&CurName[DotPos+1]);
        if (P[I]==0 || P[I]>255)
          WrongParam=true;
      }
      if (WrongParam)
        continue;
    }
    if (P[0]<=0 || P[1]<=0 || P[2]<=0 || P[1]+P[2]>255 || P[0]+P[2]-1>255)
      continue;
    if (RecVolNumber!=0 && RecVolNumber!=P[1] || FileNumber!=0 && FileNumber!=P[2])
    {
      uiMsg(UIERROR_RECVOLDIFFSETS,CurName,PrevName);
      return false;
    }
    RecVolNumber=P[1];
    FileNumber=P[2];
    PrevName=CurName;
    File *NewFile=new File;
    NewFile->TOpen(CurName);

    // This check is redundant taking into account P[I]>255 and P[0]+P[2]-1>255
    // checks above. Still we keep it here for better clarity and security.
    int SrcPos=FileNumber+P[0]-1;
    if (SrcPos<0 || SrcPos>=ASIZE(SrcFile))
      continue;
    SrcFile[SrcPos]=NewFile;

    FoundRecVolumes++;

    if (RecFileSize==0)
      RecFileSize=NewFile->FileLength();
  }
  if (!Silent || FoundRecVolumes!=0)
    uiMsg(UIMSG_RECVOLFOUND,FoundRecVolumes);
  if (FoundRecVolumes==0)
    return false;

  bool WriteFlags[256]{};

  std::wstring LastVolName;

  for (int CurArcNum=0;CurArcNum<FileNumber;CurArcNum++)
  {
    Archive *NewFile=new Archive(Cmd);
    bool ValidVolume=FileExist(ArcName);
    if (ValidVolume)
    {
      NewFile->TOpen(ArcName);
      ValidVolume=NewFile->IsArchive(false);
      if (ValidVolume)
      {
        while (NewFile->ReadHeader()!=0)
        {
          if (NewFile->GetHeaderType()==HEAD_ENDARC)
          {
            uiMsg(UIMSG_STRING,ArcName);

            if (NewFile->EndArcHead.DataCRC)
            {
              uint CalcCRC;
              CalcFileSum(NewFile,&CalcCRC,NULL,Cmd->Threads,NewFile->CurBlockPos);
              if (NewFile->EndArcHead.ArcDataCRC!=CalcCRC)
              {
                ValidVolume=false;
                uiMsg(UIMSG_CHECKSUM,ArcName);
              }
            }
            break;
          }
          NewFile->SeekToNext();
        }
      }
      if (!ValidVolume)
      {
        NewFile->Close();
        std::wstring NewName=ArcName+L".bad";

        uiMsg(UIMSG_BADARCHIVE,ArcName);
        uiMsg(UIMSG_RENAMING,ArcName,NewName);
        RenameFile(ArcName,NewName);
      }
      NewFile->Seek(0,SEEK_SET);
    }
    if (!ValidVolume)
    {
      // It is important to return 'false' instead of aborting here,
      // so if we are called from extraction, we will be able to continue
      // extracting. It may happen if .rar and .rev are on read-only disks
      // like CDs.
      if (!NewFile->Create(ArcName,FMF_WRITE|FMF_SHAREREAD))
      {
        // We need to display the title of operation before the error message,
        // to make clear for user that create error is related to recovery 
        // volumes. This is why we cannot use WCreate call here. Title must be
        // before create error, not after that.

        uiMsg(UIERROR_RECVOLFOUND,FoundRecVolumes); // Intentionally not displayed in console mode.
        uiMsg(UIERROR_RECONSTRUCTING);
        ErrHandler.CreateErrorMsg(ArcName);
        return false;
      }

      WriteFlags[CurArcNum]=true;
      MissingVolumes++;

      if (CurArcNum==FileNumber-1)
        LastVolName=ArcName;

      uiMsg(UIMSG_MISSINGVOL,ArcName);
      uiMsg(UIEVENT_NEWARCHIVE,ArcName);
    }
    SrcFile[CurArcNum]=(File*)NewFile;
    NextVolumeName(ArcName,!NewNumbering);
  }

  uiMsg(UIMSG_RECVOLMISSING,MissingVolumes);

  if (MissingVolumes==0)
  {
    uiMsg(UIERROR_RECVOLALLEXIST);
    return false;
  }

  if (MissingVolumes>FoundRecVolumes)
  {
    uiMsg(UIERROR_RECVOLFOUND,FoundRecVolumes); // Intentionally not displayed in console mode.
    uiMsg(UIERROR_RECVOLCANNOTFIX);
    return false;
  }

  uiMsg(UIMSG_RECONSTRUCTING);

  int TotalFiles=FileNumber+RecVolNumber;
  int Erasures[256],EraSize=0;

  for (int I=0;I<TotalFiles;I++)
    if (WriteFlags[I] || SrcFile[I]==NULL)
      Erasures[EraSize++]=I;

  int64 ProcessedSize=0;
  int LastPercent=-1;
  mprintf(L"     ");
  // Size of per file buffer.
  size_t RecBufferSize=TotalBufferSize/TotalFiles;

#ifdef RAR_SMP
  uint ThreadNumber=Cmd->Threads;
#else
  uint ThreadNumber=1;
#endif
  RSEncode *rse=new RSEncode[ThreadNumber];
  for (uint I=0;I<ThreadNumber;I++)
    rse[I].Init(RecVolNumber);

  while (true)
  {
    Wait();
    int MaxRead=0;
    for (int I=0;I<TotalFiles;I++)
      if (WriteFlags[I] || SrcFile[I]==NULL)
        memset(&Buf[I*RecBufferSize],0,RecBufferSize);
      else
      {
        int ReadSize=SrcFile[I]->Read(&Buf[I*RecBufferSize],RecBufferSize);
        if ((size_t)ReadSize!=RecBufferSize)
          memset(&Buf[I*RecBufferSize+ReadSize],0,RecBufferSize-ReadSize);
        if (ReadSize>MaxRead)
          MaxRead=ReadSize;
      }
    if (MaxRead==0)
      break;

    int CurPercent=ToPercent(ProcessedSize,RecFileSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      uiProcessProgress("RC",ProcessedSize,RecFileSize);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;

    int BlockStart=0;
    int BlockSize=MaxRead/ThreadNumber;
    if (BlockSize<0x100)
      BlockSize=MaxRead;
    
    for (uint CurThread=0;BlockStart<MaxRead;CurThread++)
    {
      // Last thread processes all left data including increasement
      // from rounding error.
      if (CurThread==ThreadNumber-1)
        BlockSize=MaxRead-BlockStart;

      RSEncode *curenc=rse+CurThread;
      curenc->Buf=&Buf[0];
      curenc->BufStart=BlockStart;
      curenc->BufEnd=BlockStart+BlockSize;
      curenc->FileNumber=TotalFiles;
      curenc->RecBufferSize=RecBufferSize;
      curenc->Erasures=Erasures;
      curenc->EraSize=EraSize;

#ifdef RAR_SMP
      if (ThreadNumber>1)
        RSThreadPool->AddTask(RSDecodeThread,(void*)curenc);
      else
        curenc->DecodeBuf();
#else
      curenc->DecodeBuf();
#endif

      BlockStart+=BlockSize;
    }

#ifdef RAR_SMP
    RSThreadPool->WaitDone();
#endif // RAR_SMP
    
    for (int I=0;I<FileNumber;I++)
      if (WriteFlags[I])
        SrcFile[I]->Write(&Buf[I*RecBufferSize],MaxRead);
  }
  delete[] rse;

  for (int I=0;I<RecVolNumber+FileNumber;I++)
    if (SrcFile[I]!=NULL)
    {
      File *CurFile=SrcFile[I];
      if (NewStyle && WriteFlags[I])
      {
        int64 Length=CurFile->Tell();
        CurFile->Seek(Length-7,SEEK_SET);
        for (int J=0;J<7;J++)
          CurFile->PutByte(0);
      }
      CurFile->Close();
      SrcFile[I]=NULL;
    }
  if (!LastVolName.empty())
  {
    // Truncate the last volume to its real size.
    Archive Arc(Cmd);
    if (Arc.Open(LastVolName,FMF_UPDATE) && Arc.IsArchive(true) &&
        Arc.SearchBlock(HEAD_ENDARC))
    {
      Arc.Seek(Arc.NextBlockPos,SEEK_SET);
      char Buf[8192];
      int ReadSize=Arc.Read(Buf,sizeof(Buf));
      int ZeroCount=0;
      while (ZeroCount<ReadSize && Buf[ZeroCount]==0)
        ZeroCount++;
      if (ZeroCount==ReadSize)
      {
        Arc.Seek(Arc.NextBlockPos,SEEK_SET);
        Arc.Truncate();
      }
    }
  }
#if !defined(SILENT)
  if (!Cmd->DisablePercentage)
    mprintf(L"\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return true;
}


void RSEncode::DecodeBuf()
{
  for (int BufPos=BufStart;BufPos<BufEnd;BufPos++)
  {
    byte Data[256];
    for (int I=0;I<FileNumber;I++)
      Data[I]=Buf[I*RecBufferSize+BufPos];
    RSC.Decode(Data,FileNumber,Erasures,EraSize);
    for (int I=0;I<EraSize;I++)
      Buf[Erasures[I]*RecBufferSize+BufPos]=Data[Erasures[I]];
  }
}


void RecVolumes3::Test(CommandData *Cmd,const std::wstring &Name)
{
  if (!IsNewStyleRev(Name)) // RAR 3.0 name#_#_#.rev do not include CRC32.
  {
    ErrHandler.UnknownMethodMsg(Name,Name);
    return;
  }

  std::wstring VolName=Name;

  while (FileExist(VolName))
  {
    File CurFile;
    if (!CurFile.Open(VolName))
    {
      ErrHandler.OpenErrorMsg(VolName); // It also sets RARX_OPEN.
      continue;
    }
    if (!uiStartFileExtract(VolName,false,true,false))
      return;
    mprintf(St(MExtrTestFile),VolName.c_str());
    mprintf(L"     ");
    CurFile.Seek(0,SEEK_END);
    int64 Length=CurFile.Tell();
    CurFile.Seek(Length-4,SEEK_SET);
    uint FileCRC=0;
    for (int I=0;I<4;I++)
      FileCRC|=CurFile.GetByte()<<(I*8);

    uint CalcCRC;
    CalcFileSum(&CurFile,&CalcCRC,NULL,1,Length-4,Cmd->DisablePercentage ? 0 : CALCFSUM_SHOWPROGRESS);
    if (FileCRC==CalcCRC)
    {
      mprintf(L"%s%s ",L"\b\b\b\b\b ",St(MOk));
    }
    else
    {
      uiMsg(UIERROR_CHECKSUM,VolName,VolName);
      ErrHandler.SetErrorCode(RARX_CRC);
    }

    NextVolumeName(VolName,false);
  }
}
