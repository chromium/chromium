#include "rar.hpp"

CmdExtract::CmdExtract(CommandData *Cmd)
{
  CmdExtract::Cmd=Cmd;

  ArcAnalyzed=false;
  Analyze={};

  TotalFileCount=0;

  // Common for all archives involved. Set here instead of DoExtract()
  // to use in unrar.dll too.
  // We enable it by default in Unix to care about the case when several
  // archives are unpacked to same directory with several independent RAR runs.
  // Worst case performance penalty for a lot of small files seems to be ~3%.
  // 2023.09.15: Windows performance impact seems to be negligible,
  // less than 0.5% when extracting mix of small files and folders.
  // So for extra security we enabled it for Windows too, even though
  // unlike Unix, Windows doesn't expand lnk1 in symlink targets like
  // "lnk1/../dir", but converts such path to "dir".
  ConvertSymlinkPaths=true;

  Unp=new Unpack(&DataIO);
#ifdef RAR_SMP
  Unp->SetThreads(Cmd->Threads);
#endif
}


CmdExtract::~CmdExtract()
{
  FreeAnalyzeData();
  delete Unp;
}


void CmdExtract::FreeAnalyzeData()
{
  for (size_t I=0;I<RefList.size();I++)
  {
    // We can have undeleted temporary reference source here if extraction
    // was interrupted early or if user refused to overwrite prompt.
    if (!RefList[I].TmpName.empty())
      DelFile(RefList[I].TmpName);
  }
  RefList.clear();

  Analyze={};
}


void CmdExtract::DoExtract()
{
#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
  Fat32=NotFat32=false;
#endif
  SuppressNoFilesMessage=false;
  DataIO.SetCurrentCommand(Cmd->Command[0]);

  if (Cmd->UseStdin.empty())
  {
    FindData FD;
    while (Cmd->GetArcName(ArcName))
      if (FindFile::FastFind(ArcName,&FD))
        DataIO.TotalArcSize+=FD.Size;
  }

  Cmd->ArcNames.Rewind();
  while (Cmd->GetArcName(ArcName))
  {
    if (Cmd->ManualPassword)
      Cmd->Password.Clean(); // Clean user entered password before processing next archive.
  
    ReconstructDone=false; // Must be reset here, not in ExtractArchiveInit().
    UseExactVolName=false; // Must be reset here, not in ExtractArchiveInit().
    while (true)
    {
      EXTRACT_ARC_CODE Code=ExtractArchive();
      if (Code!=EXTRACT_ARC_REPEAT)
        break;
    }
    DataIO.ProcessedArcSize+=DataIO.LastArcSize;
  }

  // Clean user entered password. Not really required, just for extra safety.
  if (Cmd->ManualPassword)
    Cmd->Password.Clean();

  if (TotalFileCount==0 && Cmd->Command[0]!='I' && 
      ErrHandler.GetErrorCode()!=RARX_BADPWD) // Not in case of wrong archive password.
  {
    if (!SuppressNoFilesMessage)
      uiMsg(UIERROR_NOFILESTOEXTRACT,ArcName);

    // Other error codes may explain a reason of "no files extracted" clearer,
    // so set it only if no other errors found (wrong mask set by user).
    if (ErrHandler.GetErrorCode()==RARX_SUCCESS)
      ErrHandler.SetErrorCode(RARX_NOFILES);
  }
  else
    if (!Cmd->DisableDone)
      if (Cmd->Command[0]=='I')
        mprintf(St(MDone));
      else
        if (ErrHandler.GetErrorCount()==0)
          mprintf(St(MExtrAllOk));
        else
          mprintf(St(MExtrTotalErr),ErrHandler.GetErrorCount());
}


void CmdExtract::ExtractArchiveInit(Archive &Arc)
{
  DataIO.AdjustTotalArcSize(&Arc);

  FileCount=0;
  MatchedArgs=0;
#ifndef SFX_MODULE
  FirstFile=true;
#endif

  GlobalPassword=Cmd->Password.IsSet() || uiIsGlobalPasswordSet();

  DataIO.UnpVolume=false;

  PrevProcessed=false;
  AllMatchesExact=true;
  AnySolidDataUnpackedWell=false;

  ArcAnalyzed=false;

  StartTime.SetCurrentTime();

  LastCheckedSymlink.clear();
}


EXTRACT_ARC_CODE CmdExtract::ExtractArchive()
{
  Archive Arc(Cmd);
  if (!Cmd->UseStdin.empty())
  {
    Arc.SetHandleType(FILE_HANDLESTD);
#ifdef USE_QOPEN
    Arc.SetProhibitQOpen(true);
#endif
  }
  else
  {
    // We commented out "&& !defined(WINRAR)", because WinRAR GUI code resets
    // the cache for usual test command, but not for test after archiving.
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    if (Cmd->Command[0]=='T' || Cmd->Test)
      ResetFileCache(ArcName); // Reset the file cache when testing an archive.
#endif
    if (!Arc.WOpen(ArcName))
      return EXTRACT_ARC_NEXT;
  }

  if (!Arc.IsArchive(true))
  {
#if !defined(SFX_MODULE) && !defined(RARDLL)
    if (CmpExt(ArcName,L"rev"))
    {
      std::wstring FirstVolName;
      VolNameToFirstName(ArcName,FirstVolName,true);

      // If several volume names from same volume set are specified
      // and current volume is not first in set and first volume is present
      // and specified too, let's skip the current volume.
      if (wcsicomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
          Cmd->ArcNames.Search(FirstVolName,false))
        return EXTRACT_ARC_NEXT;
      RecVolumesTest(Cmd,NULL,ArcName);
      TotalFileCount++; // Suppress "No files to extract" message.
      return EXTRACT_ARC_NEXT;
    }
#endif

    mprintf(St(MNotRAR),ArcName.c_str());

#ifndef SFX_MODULE
    if (CmpExt(ArcName,L"rar"))
#endif
      ErrHandler.SetErrorCode(RARX_WARNING);
    return EXTRACT_ARC_NEXT;
  }

  if (Arc.FailedHeaderDecryption) // Bad archive password.
    return EXTRACT_ARC_NEXT;

#ifndef SFX_MODULE
  if (Arc.Volume && !Arc.FirstVolume && !UseExactVolName)
  {
    std::wstring FirstVolName;
    VolNameToFirstName(ArcName,FirstVolName,Arc.NewNumbering);

    // If several volume names from same volume set are specified
    // and current volume is not first in set and first volume is present
    // and specified too, let's skip the current volume.
    if (wcsicomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
        Cmd->ArcNames.Search(FirstVolName,false))
      return EXTRACT_ARC_NEXT;
  }
#endif

  Arc.ViewComment(); // Must be before possible EXTRACT_ARC_REPEAT.

  int64 VolumeSetSize=0; // Total size of volumes after the current volume.

#ifndef SFX_MODULE
  if (!ArcAnalyzed && Cmd->UseStdin.empty())
  {
    AnalyzeArchive(Arc.FileName,Arc.Volume,Arc.NewNumbering);
    ArcAnalyzed=true; // Avoid repeated analysis on EXTRACT_ARC_REPEAT.
  }
#endif

  if (Arc.Volume)
  {
#ifndef SFX_MODULE
    // Try to speed up extraction for independent solid volumes by starting
    // extraction from non-first volume if we can.
    if (!Analyze.StartName.empty())
    {
      ArcName=Analyze.StartName;
      Analyze.StartName.clear();

      UseExactVolName=true;
      return EXTRACT_ARC_REPEAT;
    }
#endif
    
    // Calculate the total size of all accessible volumes.
    // This size is necessary to display the correct total progress indicator.

    std::wstring NextName=Arc.FileName;

    while (true)
    {
      // First volume is already added to DataIO.TotalArcSize 
      // in initial TotalArcSize calculation in DoExtract.
      // So we skip it and start from second volume.
      NextVolumeName(NextName,!Arc.NewNumbering);
      FindData FD;
      if (FindFile::FastFind(NextName,&FD))
        VolumeSetSize+=FD.Size;
      else
        break;
    }
    DataIO.TotalArcSize+=VolumeSetSize;
  }

  ExtractArchiveInit(Arc);

  if (Cmd->Command[0]=='T' || Cmd->Command[0]=='I')
    Cmd->Test=true;


  if (Cmd->Command[0]=='I')
  {
    Cmd->DisablePercentage=true;
  }
  else
    uiStartArchiveExtract(!Cmd->Test,ArcName);

#ifndef SFX_MODULE
  if (Analyze.StartPos!=0)
  {
    Arc.Seek(Analyze.StartPos,SEEK_SET);
    Analyze.StartPos=0;
  }
#endif


  while (1)
  {
    size_t Size=Arc.ReadHeader();


    bool Repeat=false;
    if (!ExtractCurrentFile(Arc,Size,Repeat))
      if (Repeat)
      {
        // If we started extraction from not first volume and need to
        // restart it from first, we must set DataIO.TotalArcSize to size
        // of new first volume to display the total progress correctly.
        FindData NewArc;
        if (FindFile::FastFind(ArcName,&NewArc))
          DataIO.TotalArcSize=NewArc.Size;
        return EXTRACT_ARC_REPEAT;
      }
      else
        break;
  }


#if !defined(SFX_MODULE) && !defined(RARDLL)
  if (Cmd->Test && Arc.Volume)
    RecVolumesTest(Cmd,&Arc,ArcName);
#endif

  return EXTRACT_ARC_NEXT;
}


bool CmdExtract::ExtractCurrentFile(Archive &Arc,size_t HeaderSize,bool &Repeat)
{
  wchar Command=Cmd->Command[0];
  if (HeaderSize==0)
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return false;
#else
      // Supposing we unpack an old RAR volume without the end of archive
      // record and last file is not split between volumes.
      if (!MergeArchive(Arc,&DataIO,false,Command))
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
        return false;
      }
#endif
    }
    else
      return false;

  HEADER_TYPE HeaderType=Arc.GetHeaderType();
  if (HeaderType==HEAD_FILE)
  {
    // Unlike Arc.FileName, ArcName might store an old volume name here.
    if (Analyze.EndPos!=0 && Analyze.EndPos==Arc.CurBlockPos &&
        (Analyze.EndName.empty() || Analyze.EndName==Arc.FileName))
      return false;
  }
  else
  {
#ifndef SFX_MODULE
    if (Arc.Format==RARFMT15 && HeaderType==HEAD3_OLDSERVICE && PrevProcessed)
      SetExtraInfo20(Cmd,Arc,DestFileName);
#endif
    if (HeaderType==HEAD_SERVICE && PrevProcessed)
      SetExtraInfo(Cmd,Arc,DestFileName);
    if (HeaderType==HEAD_ENDARC)
      if (Arc.EndArcHead.NextVolume)
      {
#ifdef NOVOLUME
        return false;
#else
        if (!MergeArchive(Arc,&DataIO,false,Command))
        {
          ErrHandler.SetErrorCode(RARX_WARNING);
          return false;
        }
        Arc.Seek(Arc.CurBlockPos,SEEK_SET);
        return true;
#endif
      }
      else
        return false;
    Arc.SeekToNext();
    return true;
  }
  PrevProcessed=false;

  // We can get negative sizes in corrupt archive and it is unacceptable
  // for size comparisons in ComprDataIO::UnpRead, where we cast sizes
  // to size_t and can exceed another read or available size. We could fix it
  // when reading an archive. But we prefer to do it here, because this
  // function is called directly in unrar.dll, so we fix bad parameters
  // passed to dll. Also we want to see real negative sizes in the listing
  // of corrupt archive. To prevent the uninitialized data access, perform
  // these checks after rejecting zero length and non-file headers above.
  if (Arc.FileHead.PackSize<0)
    Arc.FileHead.PackSize=0;
  if (Arc.FileHead.UnpSize<0)
    Arc.FileHead.UnpSize=0;

  // This check duplicates Analyze.EndPos and Analyze.EndName
  // in all cases except volumes on removable media.
  if (!Cmd->Recurse && MatchedArgs>=Cmd->FileArgs.ItemsCount() && AllMatchesExact)
    return false;

  int MatchType=MATCH_WILDSUBPATH;

  bool EqualNames=false;
  std::wstring MatchedArg;
  bool MatchFound=Cmd->IsProcessFile(Arc.FileHead,&EqualNames,MatchType,0,&MatchedArg)!=0;
#ifndef SFX_MODULE
  if (Cmd->ExclPath==EXCL_BASEPATH)
  {
    Cmd->ArcPath=MatchedArg;
    GetPathWithSep(Cmd->ArcPath,Cmd->ArcPath);
    if (IsWildcard(Cmd->ArcPath)) // Cannot correctly process path*\* masks here.
      Cmd->ArcPath.clear();
  }
#endif
  if (MatchFound && !EqualNames)
    AllMatchesExact=false;

  Arc.ConvertAttributes();

#if !defined(SFX_MODULE) && !defined(RARDLL)
  if (Arc.FileHead.SplitBefore && FirstFile && !UseExactVolName)
  {
    std::wstring StartVolName;
    GetFirstVolIfFullSet(ArcName,Arc.NewNumbering,StartVolName);

    if (StartVolName!=ArcName && FileExist(StartVolName))
    {
      ArcName=StartVolName;
      Cmd->ArcName=ArcName; // For GUI "Delete archive after extraction".
      // If first volume name does not match the current name and if such
      // volume name really exists, let's unpack from this first volume.
      Repeat=true;
      return false;
    }
#ifndef RARDLL
    if (!ReconstructDone)
    {
      ReconstructDone=true;
      if (RecVolumesRestore(Cmd,Arc.FileName,true))
      {
        Repeat=true;
        return false;
      }
    }
#endif
  }
#endif

  std::wstring ArcFileName;
  ConvertPath(&Arc.FileHead.FileName,&ArcFileName);

  if (Arc.FileHead.Version)
  {
    if (Cmd->VersionControl!=1 && !EqualNames)
    {
      if (Cmd->VersionControl==0)
        MatchFound=false;
      int Version=ParseVersionFileName(ArcFileName,false);
      if (Cmd->VersionControl-1==Version)
        ParseVersionFileName(ArcFileName,true);
      else
        MatchFound=false;
    }
  }
  else
    if (!Arc.IsArcDir() && Cmd->VersionControl>1)
      MatchFound=false;

  DataIO.UnpVolume=Arc.FileHead.SplitAfter;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);

  bool ExtrFile=false;
  bool SkipSolid=false;

#ifndef SFX_MODULE
  if (FirstFile && (MatchFound || Arc.Solid) && Arc.FileHead.SplitBefore)
  {
    if (MatchFound)
    {
      uiMsg(UIERROR_NEEDPREVVOL,Arc.FileName,ArcFileName);
#ifdef RARDLL
      Cmd->DllError=ERAR_BAD_DATA;
#endif
      ErrHandler.SetErrorCode(RARX_OPEN);
    }
    MatchFound=false;
  }

  FirstFile=false;
#endif

  bool RefTarget=false;
  if (!MatchFound)
    for (size_t I=0;I<RefList.size();I++)
      if (ArcFileName == RefList[I].RefName)
      {
        ExtractRef &MatchedRef=RefList[I];
      
        if (!Cmd->Test) // While harmless, it is useless for 't'.
        {
          // If reference source isn't selected, but target is selected,
          // we unpack the source under the temporary name and then rename
          // or copy it to target name. We do not unpack it under the target
          // name immediately, because the same source can be used by multiple
          // targets and it is possible that first target isn't unpacked
          // for some reason. Also targets might have associated service blocks
          // like ACLs. All this would complicate processing a lot.
          DestFileName=!Cmd->TempPath.empty() ? Cmd->TempPath:Cmd->ExtrPath;
          AddEndSlash(DestFileName);
          DestFileName+=L"__tmp_reference_source_";
          MkTemp(DestFileName);
          MatchedRef.TmpName=DestFileName;
        }
        RefTarget=true; // Need it even for 't' to test the reference source.
        break;
      }
  
  if (Arc.FileHead.Encrypted && Cmd->SkipEncrypted)
    if (Arc.Solid)
      return false; // Abort the entire extraction for solid archive.
    else
      MatchFound=false; // Skip only the current file for non-solid archive.
  
  if (MatchFound || RefTarget || (SkipSolid=Arc.Solid)!=false)
  {
    // First common call of uiStartFileExtract. It is done before overwrite
    // prompts, so if SkipSolid state is changed below, we'll need to make
    // additional uiStartFileExtract calls with updated parameters.
    if (!uiStartFileExtract(ArcFileName,!Cmd->Test,Cmd->Test && Command!='I',SkipSolid))
      return false;

    if (!RefTarget)
      ExtrPrepareName(Arc,ArcFileName,DestFileName);

    // DestFileName can be set empty in case of excessive -ap switch.
    ExtrFile=!SkipSolid && !DestFileName.empty() && !Arc.FileHead.SplitBefore;

    if ((Cmd->FreshFiles || Cmd->UpdateFiles) && (Command=='E' || Command=='X'))
    {
      FindData FD;
      if (FindFile::FastFind(DestFileName,&FD))
      {
        if (FD.mtime >= Arc.FileHead.mtime)
        {
          // If directory already exists and its modification time is newer 
          // than start of extraction, it is likely it was created 
          // when creating a path to one of already extracted items. 
          // In such case we'll better update its time even if archived 
          // directory is older.

          if (!FD.IsDir || FD.mtime<StartTime)
            ExtrFile=false;
        }
      }
      else
        if (Cmd->FreshFiles)
          ExtrFile=false;
    }

    if (!CheckUnpVer(Arc,ArcFileName))
    {
      ErrHandler.SetErrorCode(RARX_FATAL);
#ifdef RARDLL
      Cmd->DllError=ERAR_UNKNOWN_FORMAT;
#endif
      Arc.SeekToNext();
      return !Arc.Solid; // Can try extracting next file only in non-solid archive.
    }

    if (Arc.FileHead.Encrypted)
    {
      RarCheckPassword CheckPwd;
      if (Arc.Format==RARFMT50 && Arc.FileHead.UsePswCheck && !Arc.BrokenHeader)
        CheckPwd.Set(Arc.FileHead.Salt,Arc.FileHead.InitV,Arc.FileHead.Lg2Count,Arc.FileHead.PswCheck);

      while (true) // Repeat the password prompt for wrong and empty passwords.
      {
        // Stop archive extracting if user cancelled a password prompt.
#ifdef RARDLL
        if (!ExtrDllGetPassword())
        {
          Cmd->DllError=ERAR_MISSING_PASSWORD;
          return false;
        }
#else
        if (!ExtrGetPassword(Arc,ArcFileName,CheckPwd.IsSet() ? &CheckPwd:NULL))
        {
          SuppressNoFilesMessage=true;
          return false;
        }
#endif

        // Set a password before creating the file, so we can skip creating
        // in case of wrong password.
        SecPassword FilePassword=Cmd->Password;
  #if defined(_WIN_ALL) && !defined(SFX_MODULE)
        ConvertDosPassword(Arc,FilePassword);
  #endif

        byte PswCheck[SIZE_PSWCHECK];
        bool EncSet=DataIO.SetEncryption(false,Arc.FileHead.CryptMethod,
               &FilePassword,Arc.FileHead.SaltSet ? Arc.FileHead.Salt:nullptr,
               Arc.FileHead.InitV,Arc.FileHead.Lg2Count,
               Arc.FileHead.HashKey,PswCheck);

        // If header is damaged, we cannot rely on password check value,
        // because it can be damaged too.
        if (EncSet && Arc.FileHead.UsePswCheck && !Arc.BrokenHeader &&
            memcmp(Arc.FileHead.PswCheck,PswCheck,SIZE_PSWCHECK)!=0)
        {
          if (GlobalPassword) // For -p<pwd> or Ctrl+P to avoid the infinite loop.
          {
            // This message is used by Android GUI to reset cached passwords.
            // Update appropriate code if changed.
            uiMsg(UIERROR_BADPSW,Arc.FileName,ArcFileName);
          }
          else // For passwords entered manually.
          {
            // This message is used by Android GUI and Windows GUI and SFX to
            // reset cached passwords. Update appropriate code if changed.
            uiMsg(UIWAIT_BADPSW,Arc.FileName,ArcFileName);
            Cmd->Password.Clean();

            // Avoid new requests for unrar.dll to prevent the infinite loop
            // if app always returns the same password.
  #ifndef RARDLL
            continue; // Request a password again.
  #endif
          }
  #ifdef RARDLL
          // If we already have ERAR_EOPEN as result of missing volume,
          // we should not replace it with less precise ERAR_BAD_PASSWORD.
          if (Cmd->DllError!=ERAR_EOPEN)
            Cmd->DllError=ERAR_BAD_PASSWORD;
  #endif
          ErrHandler.SetErrorCode(RARX_BADPWD);
          ExtrFile=false;
        }
        break;
      }
    }
    else
      DataIO.SetEncryption(false,CRYPT_NONE,NULL,NULL,NULL,0,NULL,NULL);

    // Per file symlink conversion flag. Can be turned off in unrar.dll.
    bool CurConvertSymlinkPaths=ConvertSymlinkPaths;

#ifdef RARDLL
    if (!Cmd->DllDestName.empty())
    {
      DestFileName=Cmd->DllDestName;

      // If unrar.dll sets the entire destination pathname, there is no
      // destination path and we can't convert symlinks, because we would
      // risk converting important user or system symlinks in this case.
      // If DllDestName is set, it turns off our path processing and app
      // invoking the library cares about everything including safety.
      CurConvertSymlinkPaths=false;
    }
#endif

    if (ExtrFile && Command!='P' && !Cmd->Test && !Cmd->AbsoluteLinks &&
        CurConvertSymlinkPaths)
      ExtrFile=LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink);

    File CurFile;
#if defined(CHROMIUM_UNRAR)
    // Since extraction is done in a sandbox, this must extract to the temp file
    // handle instead of the default.
    CurFile.SetFileHandle(Arc.GetTempFileHandle());
#endif

    bool LinkEntry=Arc.FileHead.RedirType!=FSREDIR_NONE;
    if (LinkEntry && (Arc.FileHead.RedirType!=FSREDIR_FILECOPY))
    {
      if (Cmd->SkipSymLinks && (Arc.FileHead.RedirType==FSREDIR_UNIXSYMLINK ||
          Arc.FileHead.RedirType==FSREDIR_WINSYMLINK || Arc.FileHead.RedirType==FSREDIR_JUNCTION))
        ExtrFile=false;

      if (ExtrFile && Command!='P' && !Cmd->Test)
      {
        // Overwrite prompt for symbolic and hard links and when we move
        // a temporary file to the file reference instead of copying it.
        bool UserReject=false;
        if (FileExist(DestFileName))
          FileCreate(Cmd,NULL,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
        if (UserReject)
          ExtrFile=false;
      }
    }
    else
      if (Arc.IsArcDir())
      {
        if (!ExtrFile || Command=='P' || Command=='I' || Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
          return true;
        TotalFileCount++;
        ExtrCreateDir(Arc,ArcFileName);
        // It is important to not increment MatchedArgs here, so we extract
        // dir with its entire contents and not dir record only even if
        // dir record precedes files.
        return true;
      }
      else
        if (ExtrFile) // Create files and file copies (FSREDIR_FILECOPY).
        {
          // Check the dictionary size before creating a file and issuing
          // any overwrite prompts.
          if (!CheckWinLimit(Arc,ArcFileName))
            return false;
          ExtrFile=ExtrCreateFile(Arc,CurFile);
        }

    if (!ExtrFile && Arc.Solid)
    {
      SkipSolid=true;
      ExtrFile=true;

      // We changed SkipSolid, so we need to call uiStartFileExtract
      // with "Skip" parameter to change the operation status 
      // from "extracting" to "skipping". For example, it can be necessary
      // if user answered "No" to overwrite prompt when unpacking
      // a solid archive.
      if (!uiStartFileExtract(ArcFileName,false,false,true))
        return false;
      // Check the dictionary size also for skipping files.
      if (!CheckWinLimit(Arc,ArcFileName))
        return false;
    }
    if (ExtrFile)
    {
      // Set it in test mode, so we also test subheaders such as NTFS streams
      // after tested file.
      if (Cmd->Test)
        PrevProcessed=true;

      bool TestMode=Cmd->Test || SkipSolid; // Unpack to memory, not to disk.

      if (!SkipSolid)
      {
        if (!TestMode && Command!='P' && CurFile.IsDevice())
        {
          uiMsg(UIERROR_INVALIDNAME,Arc.FileName,DestFileName);
          ErrHandler.WriteError(Arc.FileName,DestFileName);
        }
        TotalFileCount++;
      }
      FileCount++;
      if (Command!='I' && !Cmd->DisableNames)
        if (SkipSolid)
          mprintf(St(MExtrSkipFile),ArcFileName.c_str());
        else
          switch(Cmd->Test ? 'T':Command) // "Test" can be also enabled by -t switch.
          {
            case 'T':
              mprintf(St(MExtrTestFile),ArcFileName.c_str());
              break;
#ifndef SFX_MODULE
            case 'P':
              mprintf(St(MExtrPrinting),ArcFileName.c_str());
              break;
#endif
            case 'X':
            case 'E':
              mprintf(St(MExtrFile),DestFileName.c_str());
              break;
          }
      if (!Cmd->DisablePercentage && !Cmd->DisableNames)
        mprintf(L"     ");
      if (Cmd->DisableNames)
        uiEolAfterMsg(); // Avoid erasing preceding messages by percentage indicator in -idn mode.

      DataIO.CurUnpRead=0;
      DataIO.CurUnpWrite=0;
      DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
      DataIO.SetFiles(&Arc,&CurFile);
      DataIO.SetTestMode(TestMode);
      DataIO.SetSkipUnpCRC(SkipSolid);

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SILENT)
      if (!TestMode && !Arc.BrokenHeader &&
          Arc.FileHead.UnpSize>0xffffffff && (Fat32 || !NotFat32))
      {
        if (!Fat32) // Not detected yet.
          NotFat32=!(Fat32=IsFAT(Cmd->ExtrPath));
        if (Fat32)
          uiMsg(UIMSG_FAT32SIZE); // Inform user about FAT32 size limit.
      }
#endif

      uint64 Preallocated=0;
      if (!TestMode && !Arc.BrokenHeader && Arc.FileHead.UnpSize>1000000 &&
          Arc.FileHead.PackSize*1024>Arc.FileHead.UnpSize && Arc.IsSeekable() &&
          (Arc.FileHead.UnpSize<100000000 || Arc.FileLength()>Arc.FileHead.PackSize))
      {
        CurFile.Prealloc(Arc.FileHead.UnpSize);
        Preallocated=Arc.FileHead.UnpSize;
      }
      CurFile.SetAllowDelete(!Cmd->KeepBroken);

      bool FileCreateMode=!TestMode && !SkipSolid && Command!='P';
      bool ShowChecksum=true; // Display checksum verification result.

      bool LinkSuccess=true; // Assume success for test mode.
      if (LinkEntry)
      {
        FILE_SYSTEM_REDIRECT Type=Arc.FileHead.RedirType;

        if (Type==FSREDIR_HARDLINK || Type==FSREDIR_FILECOPY)
        {
          std::wstring RedirName;
        
          // 2022.11.15: Might be needed when unpacking WinRAR 5.0 links with
          // Unix RAR. WinRAR 5.0 used \ path separators here, when beginning
          // from 5.10 even Windows version uses / internally and converts
          // them to \ when reading FHEXTRA_REDIR.
          // We must perform this conversion before ConvertPath call,
          // so paths mixing different slashes like \dir1/dir2\file are
          // processed correctly.
          SlashToNative(Arc.FileHead.RedirName,RedirName);

          ConvertPath(&RedirName,&RedirName);

          std::wstring NameExisting;
          ExtrPrepareName(Arc,RedirName,NameExisting);
          if (FileCreateMode && !NameExisting.empty()) // *NameExisting can be empty in case of excessive -ap switch.
            if (Type==FSREDIR_HARDLINK)
              LinkSuccess=ExtractHardlink(Cmd,DestFileName,NameExisting);
            else
              LinkSuccess=ExtractFileCopy(CurFile,Arc.FileName,RedirName,DestFileName,NameExisting,Arc.FileHead.UnpSize);
        }
        else
          if (Type==FSREDIR_UNIXSYMLINK || Type==FSREDIR_WINSYMLINK || Type==FSREDIR_JUNCTION)
          {
            if (FileCreateMode)
            {
              bool UpLink;
              LinkSuccess=ExtractSymlink(Cmd,DataIO,Arc,DestFileName,UpLink);

              ConvertSymlinkPaths|=LinkSuccess && UpLink;

              // We do not actually need to reset the cache here if we cache
              // only the single last checked path, because at this point
              // it will always contain the link own path and link can't
              // overwrite its parent folder. But if we ever decide to cache
              // several already checked paths, we'll need to reset them here.
              // Otherwise if no files were created in one of such paths,
              // let's say because of file create error, it might be possible
              // to overwrite the path with link and avoid checks. We keep this
              // code here as a reminder in case of possible modifications.
              LastCheckedSymlink.clear(); // Reset cache for safety reason.
            }
          }
          else
          {
            uiMsg(UIERROR_UNKNOWNEXTRA,Arc.FileName,ArcFileName);
            LinkSuccess=false;
          }
          
          if (!LinkSuccess || Arc.Format==RARFMT15 && !FileCreateMode)
          {
            // RAR 5.x links have a valid data checksum even in case of
            // failure, because they do not store any data.
            // We do not want to display "OK" in this case.
            // For 4.x symlinks we verify the checksum only when extracting,
            // but not when testing an archive.
            ShowChecksum=false;
          }
          PrevProcessed=FileCreateMode && LinkSuccess;
      }
      else
        if (!Arc.FileHead.SplitBefore)
          if (Arc.FileHead.Method==0)
            UnstoreFile(DataIO,Arc.FileHead.UnpSize);
          else
          {
#if defined (UNRAR_NO_EXCEPTIONS)
            Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
#else
            try
            {
              Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
            }
            catch (std::bad_alloc)
            {
              if (Arc.FileHead.WinSize>=0x40000000)
                uiMsg(UIERROR_EXTRDICTOUTMEM,Arc.FileName,uint(Arc.FileHead.WinSize/0x40000000+(Arc.FileHead.WinSize%0x40000000!=0 ? 1 : 0)));
              throw;
            }
#endif  // UNRAR_NO_EXCEPTIONS

            Unp->SetDestSize(Arc.FileHead.UnpSize);
#ifndef SFX_MODULE
            // RAR 1.3 - 1.5 archives do not set per file solid flag.
            if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
              Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
            else
#endif
              Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid);
          }

      Arc.SeekToNext();

      // We check for "split after" flag to detect partially extracted files
      // from incomplete volume sets. For them file header contains packed
      // data hash, which must not be compared against unpacked data hash
      // to prevent accidental match. Moreover, for -m0 volumes packed data
      // hash would match truncated unpacked data hash and lead to fake "OK"
      // in incomplete volume set.
      bool ValidCRC=!Arc.FileHead.SplitAfter && DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL);

      // We set AnySolidDataUnpackedWell to true if we found at least one
      // valid non-zero solid file in preceding solid stream. If it is true
      // and if current encrypted file is broken, we do not need to hint
      // about a wrong password and can report CRC error only.
      if (!Arc.FileHead.Solid)
        AnySolidDataUnpackedWell=false; // Reset the flag, because non-solid file is found.
      else
        if (Arc.FileHead.Method!=0 && Arc.FileHead.UnpSize>0 && ValidCRC)
          AnySolidDataUnpackedWell=true;
 
      bool BrokenFile=false;
      
      // Checksum is not calculated in skip solid mode for performance reason.
      if (!SkipSolid && ShowChecksum)
      {
        if (ValidCRC)
        {
          if (Command!='P' && Command!='I' && !Cmd->DisableNames)
            mprintf(L"%s%s ",Cmd->DisablePercentage ? L" ":L"\b\b\b\b\b ",
              Arc.FileHead.FileHash.Type==HASH_NONE ? L"  ?":St(MOk));
        }
        else
        {
          if (Arc.FileHead.Encrypted && (!Arc.FileHead.UsePswCheck || 
              Arc.BrokenHeader) && !AnySolidDataUnpackedWell)
            uiMsg(UIERROR_CHECKSUMENC,Arc.FileName,ArcFileName);
          else
            uiMsg(UIERROR_CHECKSUM,Arc.FileName,ArcFileName);
          BrokenFile=true;
          ErrHandler.SetErrorCode(RARX_CRC);
#ifdef RARDLL
          // If we already have ERAR_EOPEN as result of missing volume
          // or ERAR_BAD_PASSWORD for RAR5 wrong password,
          // we should not replace it with less precise ERAR_BAD_DATA.
          if (Cmd->DllError!=ERAR_EOPEN && Cmd->DllError!=ERAR_BAD_PASSWORD)
            Cmd->DllError=ERAR_BAD_DATA;
#endif
        }
      }
      else
      {
        // We check SkipSolid to remove percent for skipped solid files only.
        // We must not apply these \b to links with ShowChecksum==false
        // and their possible error messages.
        if (SkipSolid) 
          mprintf(L"\b\b\b\b\b     ");
      }
      if (!TestMode && (Command=='X' || Command=='E') &&
          (!LinkEntry || LinkSuccess) && (!BrokenFile || Cmd->KeepBroken))
      {
        // Set everything for usual files and file references.
        bool SetAll=!LinkEntry || Arc.FileHead.RedirType==FSREDIR_FILECOPY;

        // Set time and adjust size for usual files and references.
        // Symlink time requires the special treatment and it is set directly
        // after creating a symlink.
        bool SetTimeAndSize=SetAll;

        // Set file attributes for usual files, references and hard links.
        // Hard link shares the file metadata with link target, so we do not
        // need to set link time or owner. But when we overwrite an existing
        // link, we can call PrepareToDelete(), which affects link target
        // attributes too. So we set link attributes to restore both target
        // and link attributes if PrepareToDelete() has changed them.
        bool SetAttr=SetAll || Arc.FileHead.RedirType==FSREDIR_HARDLINK;

        // Call SetFileHeaderExtra to set Unix user and group for usual files,
        // references and symlinks. Unix symlink can have its own owner data.
        bool SetExtra=SetAll || Arc.FileHead.RedirType==FSREDIR_UNIXSYMLINK;

        // Below we use DestFileName instead of CurFile.FileName,
        // so we can set file attributes also for hard links, which do not
        // have the open CurFile. These strings are the same for other items.

        if (SetTimeAndSize)
        {
          // We could preallocate more space than really written to broken file
          // or file with crafted header.
          if (Preallocated>0 && (BrokenFile || DataIO.CurUnpWrite!=Preallocated))
            CurFile.Truncate();


          CurFile.SetOpenFileTime(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
            Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
          CurFile.Close();
        }

        if (SetExtra)
          SetFileHeaderExtra(Cmd,Arc,DestFileName);

        if (SetTimeAndSize)
          CurFile.SetCloseFileTime(
            Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
            Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
        
        if (SetAttr)
        {
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
          if (Cmd->SetCompressedAttr &&
              (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0)
            SetFileCompression(DestFileName,true);
          if (Cmd->ClearArc)
            Arc.FileHead.FileAttr&=~FILE_ATTRIBUTE_ARCHIVE;
#endif
          if (!Cmd->IgnoreGeneralAttr && !SetFileAttr(DestFileName,Arc.FileHead.FileAttr))
          {
            uiMsg(UIERROR_FILEATTR,Arc.FileName,DestFileName);
            // Android cannot set file attributes and while UIERROR_FILEATTR
            // above is handled by Android RAR silently, this call would cause
            // "Operation not permitted" message for every unpacked file.
            ErrHandler.SysErrMsg();
          }
        }

        PrevProcessed=true;
      }
    }
  }
  // It is important to increment it for files, but not dirs. So we extract
  // dir with its entire contents, not just dir record only even if dir
  // record precedes files.
  if (MatchFound)
    MatchedArgs++;
  if (DataIO.NextVolumeMissing)
    return false;
  if (!ExtrFile)
    if (!Arc.Solid)
      Arc.SeekToNext();
    else
      if (!SkipSolid)
        return false;
  return true;
}


void CmdExtract::UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize)
{
  std::vector<byte> Buffer(File::CopyBufferSize());
  while (true)
  {
    int ReadSize=DataIO.UnpRead(Buffer.data(),Buffer.size());
    if (ReadSize<=0)
      break;
    int WriteSize=ReadSize<DestUnpSize ? ReadSize:(int)DestUnpSize;
    if (WriteSize>0)
    {
      DataIO.UnpWrite(Buffer.data(),WriteSize);
      DestUnpSize-=WriteSize;
    }
  }
}


bool CmdExtract::ExtractFileCopy(File &New,const std::wstring &ArcName,const std::wstring &RedirName,const std::wstring &NameNew,const std::wstring &NameExisting,int64 UnpSize)
{
  File Existing;
  if (!Existing.Open(NameExisting))
  {
    std::wstring TmpExisting=NameExisting; // NameExisting is 'const', so copy it here.

    bool OpenFailed=true;
    // If we couldn't find the existing file, check if match is present
    // in temporary reference sources list.
    for (size_t I=0;I<RefList.size();I++)
      if (RedirName==RefList[I].RefName && !RefList[I].TmpName.empty())
      {
        // If only one reference left targeting to this temporary file,
        // it is faster to move the file instead of copying and deleting it.
        bool RefMove=RefList[I].RefCount-- == 1;
        TmpExisting=RefList[I].TmpName;
        if (RefMove) // Only one reference left for this temporary file.
        {
          New.Delete(); // Delete the previously opened destination file.
          // Try moving the file first.
          bool MoveFailed=!RenameFile(TmpExisting,NameNew);
          if (MoveFailed)
          {
            // If move failed, re-create the destination and try coping.
            if (!New.WCreate(NameNew,FMF_WRITE|FMF_SHAREREAD))
              return false;
            RefMove=false; // Try copying below.
          }
          else
          {
            // If moved successfully, reopen the destination file and seek to
            // end for SetOpenFileTime() and possible Truncate() calls later.
            if (New.Open(NameNew))
              New.Seek(0,SEEK_END);
            // We already moved the file, so clean the name to not try
            // deleting non-existent temporary file later.
            RefList[I].TmpName.clear();
            return true;
          }
        }
        if (!RefMove)
          OpenFailed=!Existing.Open(TmpExisting);
        break;
      }

    if (OpenFailed)
    {
      ErrHandler.OpenErrorMsg(TmpExisting);
      uiMsg(UIERROR_FILECOPY,ArcName,TmpExisting,NameNew);
      uiMsg(UIERROR_FILECOPYHINT,ArcName);
#ifdef RARDLL
      Cmd->DllError=ERAR_EREFERENCE;
#endif
      return false;
    }
  }

  std::vector<byte> Buffer(0x100000);
  int64 CopySize=0;

  while (true)
  {
    Wait();
    int ReadSize=Existing.Read(Buffer.data(),Buffer.size());
    if (ReadSize==0)
      break;
    // Update only the current file progress in WinRAR, set the total to 0
    // to keep it as is. It looks better for WinRAR.
    uiExtractProgress(CopySize,UnpSize,0,0);

    New.Write(Buffer.data(),ReadSize);
    CopySize+=ReadSize;
  }

  return true;
}


void CmdExtract::ExtrPrepareName(Archive &Arc,const std::wstring &ArcFileName,std::wstring &DestName)
{
  if (Cmd->Test)
  {
    // Destination name conversion isn't needed for simple archive test.
    // This check also allows to avoid issuing "Attempting to correct...
    // Renaming..." messages in MakeNameCompatible() below for problematic
    // names like aux.txt when testing an archive.
    DestName=ArcFileName;
    return;
  }
  
  DestName=Cmd->ExtrPath;

  if (!Cmd->ExtrPath.empty())
  {
    wchar LastChar=GetLastChar(Cmd->ExtrPath);
    // We need IsPathDiv check here to correctly handle Unix forward slash
    // in the end of destination path in Windows: rar x arc dest/
    // so we call IsPathDiv first instead of just calling AddEndSlash,
    // which checks for only one type of path separator.
    // IsDriveDiv is needed for current drive dir: rar x arc d:
    if (!IsPathDiv(LastChar) && !IsDriveDiv(LastChar))
    {
      // Destination path can be without trailing slash if it come from GUI shell.
      AddEndSlash(DestName);
    }
  }

#ifndef SFX_MODULE
  if (Cmd->AppendArcNameToPath!=APPENDARCNAME_NONE)
  {
    switch(Cmd->AppendArcNameToPath)
    {
      case APPENDARCNAME_DESTPATH: // To subdir of destination path.
        DestName+=PointToName(Arc.FirstVolumeName);
        RemoveExt(DestName);
        break;
      case APPENDARCNAME_OWNSUBDIR: // To subdir of archive own dir.
        DestName=Arc.FirstVolumeName;
        RemoveExt(DestName);
        break;
      case APPENDARCNAME_OWNDIR:  // To archive own dir.
        DestName=Arc.FirstVolumeName;
        RemoveNameFromPath(DestName);
        break;
    }
    AddEndSlash(DestName);
  }
#endif
  // We need to modify the name below and ArcFileName is const.
  std::wstring CurName=ArcFileName;
#ifndef SFX_MODULE
  std::wstring &ArcPath=!Cmd->ExclArcPath.empty() ? Cmd->ExclArcPath:Cmd->ArcPath;
  size_t ArcPathLength=ArcPath.size();
  if (ArcPathLength>0)
  {
    size_t NameLength=CurName.size();
    if (NameLength>=ArcPathLength && wcsnicompc(ArcPath,CurName,ArcPathLength)==0 &&
        (IsPathDiv(ArcPath[ArcPathLength-1]) || 
         IsPathDiv(CurName[ArcPathLength]) || CurName[ArcPathLength]==0))
    {
      size_t Pos=Min(ArcPathLength,NameLength);
      while (Pos<CurName.size() && IsPathDiv(CurName[Pos]))
        Pos++;
      CurName.erase(0,Pos);
      if (CurName.empty()) // Excessive -ap switch.
      {
        DestName.clear();
        return;
      }
    }
  }
#endif

  wchar Command=Cmd->Command[0];
  // Use -ep3 only in systems, where disk letters are exist, not in Unix.
  bool AbsPaths=Cmd->ExclPath==EXCL_ABSPATH && Command=='X' && IsDriveDiv(':');

  if (AbsPaths)
  {
    // We do not use a user specified destination path when extracting
    // absolute paths in -ep3 mode.
    wchar DiskLetter=toupperw(CurName[0]);
    if (CurName[1]=='_' && IsPathDiv(CurName[2]) && DiskLetter>='A' && DiskLetter<='Z')
      DestName=CurName.substr(0,1) + L':' + CurName.substr(2);
    else
      if (CurName[0]=='_' && CurName[1]=='_')
        DestName=std::wstring(2,CPATHDIVIDER) + CurName.substr(2);
      else
        AbsPaths=false; // Apply the destination path even with -ep3 for not absolute path.
  }

  if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
    CurName=PointToName(CurName);
  if (!AbsPaths)
    DestName+=CurName;

#ifdef _WIN_ALL
  // Must do after Cmd->ArcPath processing above, so file name and arc path
  // trailing spaces are in sync.
  if (!Cmd->AllowIncompatNames)
    MakeNameCompatible(DestName);
#endif
}


#ifdef RARDLL
bool CmdExtract::ExtrDllGetPassword()
{
  if (!Cmd->Password.IsSet())
  {
    if (Cmd->Callback!=NULL)
    {
      wchar PasswordW[MAXPASSWORD];
      *PasswordW=0;
      if (Cmd->Callback(UCM_NEEDPASSWORDW,Cmd->UserData,(LPARAM)PasswordW,ASIZE(PasswordW))==-1)
        *PasswordW=0;
      if (*PasswordW==0)
      {
        char PasswordA[MAXPASSWORD];
        *PasswordA=0;
        if (Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LPARAM)PasswordA,ASIZE(PasswordA))==-1)
          *PasswordA=0;
        CharToWide(PasswordA,PasswordW,ASIZE(PasswordW));
        cleandata(PasswordA,sizeof(PasswordA));
      }
      Cmd->Password.Set(PasswordW);
      cleandata(PasswordW,sizeof(PasswordW));
      Cmd->ManualPassword=true;
    }
    if (!Cmd->Password.IsSet())
      return false;
  }
  return true;
}
#endif


#ifndef RARDLL
bool CmdExtract::ExtrGetPassword(Archive &Arc,const std::wstring &ArcFileName,RarCheckPassword *CheckPwd)
{
  if (!Cmd->Password.IsSet())
  {
    if (!uiGetPassword(UIPASSWORD_FILE,ArcFileName,&Cmd->Password,CheckPwd)/* || !Cmd->Password.IsSet()*/)
    {
      // Suppress "test is ok" message if user cancelled the password prompt.
      uiMsg(UIERROR_INCERRCOUNT);
      return false;
    }
    Cmd->ManualPassword=true;
  }
#if !defined(SILENT)
  else
    if (!GlobalPassword && !Arc.FileHead.Solid)
    {
      eprintf(St(MUseCurPsw),ArcFileName.c_str());
      switch(Cmd->AllYes ? 1 : Ask(St(MYesNoAll)))
      {
        case -1:
          ErrHandler.Exit(RARX_USERBREAK);
        case 2:
          if (!uiGetPassword(UIPASSWORD_FILE,ArcFileName,&Cmd->Password,CheckPwd))
            return false;
          break;
        case 3:
          GlobalPassword=true;
          break;
      }
    }
#endif
  return true;
}
#endif


#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void CmdExtract::ConvertDosPassword(Archive &Arc,SecPassword &DestPwd)
{
  if (Arc.Format==RARFMT15 && Arc.FileHead.HostOS==HOST_MSDOS)
  {
    // We need the password in OEM encoding if file was encrypted by
    // native RAR/DOS (not extender based). Let's make the conversion.
    wchar PlainPsw[MAXPASSWORD];
    Cmd->Password.Get(PlainPsw,ASIZE(PlainPsw));
    char PswA[MAXPASSWORD];
    CharToOemBuffW(PlainPsw,PswA,ASIZE(PswA));
    PswA[ASIZE(PswA)-1]=0;
    CharToWide(PswA,PlainPsw,ASIZE(PlainPsw));
    DestPwd.Set(PlainPsw);
    cleandata(PlainPsw,sizeof(PlainPsw));
    cleandata(PswA,sizeof(PswA));
  }
}
#endif


void CmdExtract::ExtrCreateDir(Archive &Arc,const std::wstring &ArcFileName)
{
  if (Cmd->Test)
  {
    if (!Cmd->DisableNames)
    {
      mprintf(St(MExtrTestFile),ArcFileName.c_str());
      mprintf(L" %s",St(MOk));
    }
    return;
  }

  MKDIR_CODE MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
  bool DirExist=false;
  if (MDCode!=MKDIR_SUCCESS)
  {
    DirExist=FileExist(DestFileName);
    if (DirExist && !IsDir(GetFileAttr(DestFileName)))
    {
      // File with name same as this directory exists. Propose user
      // to overwrite it.
      bool UserReject;
      FileCreate(Cmd,NULL,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
      DirExist=false;
    }
    if (!DirExist)
    {
      CreatePath(DestFileName,true,Cmd->DisableNames);
      MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
      if (MDCode!=MKDIR_SUCCESS && !IsNameUsable(DestFileName))
      {
        uiMsg(UIMSG_CORRECTINGNAME,Arc.FileName);
        std::wstring OrigName=DestFileName;
        MakeNameUsable(DestFileName,true);
#ifndef SFX_MODULE
        uiMsg(UIERROR_RENAMING,Arc.FileName,OrigName,DestFileName);
#endif
        DirExist=FileExist(DestFileName) && IsDir(GetFileAttr(DestFileName));
        if (!DirExist && (Cmd->AbsoluteLinks || !ConvertSymlinkPaths ||
            LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink)))
        {
          CreatePath(DestFileName,true,Cmd->DisableNames);
          MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
        }
      }
    }
  }
  if (MDCode==MKDIR_SUCCESS)
  {
    if (!Cmd->DisableNames)
    {
      mprintf(St(MCreatDir),DestFileName.c_str());
      mprintf(L" %s",St(MOk));
    }
    PrevProcessed=true;
  }
  else
    if (DirExist)
    {
      if (!Cmd->IgnoreGeneralAttr)
        SetFileAttr(DestFileName,Arc.FileHead.FileAttr);
      PrevProcessed=true;
    }
    else
    {
      uiMsg(UIERROR_DIRCREATE,Arc.FileName,DestFileName);
      ErrHandler.SysErrMsg();
#ifdef RARDLL
      Cmd->DllError=ERAR_ECREATE;
#endif
      ErrHandler.SetErrorCode(RARX_CREATE);
    }
  if (PrevProcessed)
  {
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    if (Cmd->SetCompressedAttr &&
        (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0 && WinNT()!=WNT_NONE)
      SetFileCompression(DestFileName,true);
#endif
    SetFileHeaderExtra(Cmd,Arc,DestFileName);
    SetDirTime(DestFileName,
      Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
      Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
      Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
  }
}


bool CmdExtract::ExtrCreateFile(Archive &Arc,File &CurFile)
{
  bool Success=true;
  wchar Command=Cmd->Command[0];
#if !defined(SFX_MODULE)
  if (Command=='P')
    CurFile.SetHandleType(FILE_HANDLESTD);
#endif
  if ((Command=='E' || Command=='X') && !Cmd->Test)
  {
    bool UserReject;
    // Specify "write only" mode to avoid OpenIndiana NAS problems
    // with SetFileTime and read+write files.
    if (!FileCreate(Cmd,&CurFile,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,true))
    {
      Success=false;
      if (!UserReject)
      {
        ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
        if (FileExist(DestFileName) && IsDir(GetFileAttr(DestFileName)))
          uiMsg(UIERROR_DIRNAMEEXISTS);

#ifdef RARDLL
        Cmd->DllError=ERAR_ECREATE;
#endif
        if (!IsNameUsable(DestFileName))
        {
          uiMsg(UIMSG_CORRECTINGNAME,Arc.FileName);

          std::wstring OrigName=DestFileName;

          MakeNameUsable(DestFileName,true);

          if (Cmd->AbsoluteLinks || !ConvertSymlinkPaths ||
              LinksToDirs(DestFileName,Cmd->ExtrPath,LastCheckedSymlink))
          {
            CreatePath(DestFileName,true,Cmd->DisableNames);
            if (FileCreate(Cmd,&CurFile,DestFileName,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,true))
            {
#ifndef SFX_MODULE
              uiMsg(UIERROR_RENAMING,Arc.FileName,OrigName,DestFileName);
#endif
              Success=true;
            }
            else
              ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
          }
        }
      }
    }
  }
  return Success;
}


bool CmdExtract::CheckUnpVer(Archive &Arc,const std::wstring &ArcFileName)
{
  bool WrongVer;
  if (Arc.Format==RARFMT50) // Both SFX and RAR can unpack RAR 5.0 and 7.0 archives.
    WrongVer=Arc.FileHead.UnpVer>VER_UNPACK7;
  else
  {
#ifdef SFX_MODULE   // SFX can unpack only RAR 2.9 archives.
    WrongVer=Arc.FileHead.UnpVer!=VER_UNPACK;
#else               // All formats since 1.3 for RAR.
    WrongVer=Arc.FileHead.UnpVer<13 || Arc.FileHead.UnpVer>VER_UNPACK;
#endif
  }

  // We can unpack stored files regardless of compression version field.
  if (Arc.FileHead.Method==0)
    WrongVer=false;

  if (WrongVer)
  {
    ErrHandler.UnknownMethodMsg(Arc.FileName,ArcFileName);
    uiMsg(UIERROR_NEWERRAR,Arc.FileName);
  }
  return !WrongVer;
}


#ifndef SFX_MODULE
// Find non-matched reference sources in solid and non-solid archives.
// Detect the optimal start position for semi-solid archives
// and optimal start volume for independent solid volumes.
// 
// Alternatively we could collect references while extracting an archive
// and perform the second extraction pass for references only.
// But it would be slower for solid archives than scaning headers
// in first pass and extracting everything in second, as implemented now.
// 
void CmdExtract::AnalyzeArchive(const std::wstring &ArcName,bool Volume,bool NewNumbering)
{
  FreeAnalyzeData(); // If processing non-first archive in multiple archives set.

  wchar *ArgName=Cmd->FileArgs.GetString();
  Cmd->FileArgs.Rewind();
  if (ArgName!=NULL && (wcscmp(ArgName,L"*")==0 || wcscmp(ArgName,L"*.*")==0))
    return; // No need to check further for * and *.* masks.

  // Start search from first volume if all volumes preceding current are available.
  std::wstring NextName;
  if (Volume)
    GetFirstVolIfFullSet(ArcName,NewNumbering,NextName);
  else
    NextName=ArcName;
  
  bool MatchFound=false;
  bool PrevMatched=false;
  bool OpenNext=false;

  bool FirstVolume=true;
  
  // We shall set FirstFile once for all volumes and not for each volume.
  // So we do not reuse the outdated Analyze.StartPos from previous volume
  // if extracted file resides completely in the beginning of current one.
  bool FirstFile=true;

  while (true)
  {
    Archive Arc(Cmd);
    if (!Arc.Open(NextName) || !Arc.IsArchive(false))
    {
      if (OpenNext)
      {
        // If we couldn't open trailing volumes, we can't set early exit
        // parameters. It is possible that some volume are on removable media
        // and will be provided by user when extracting.
        Analyze.EndName.clear();
        Analyze.EndPos=0;
      }
      break;
    }

    OpenNext=false;
    while (Arc.ReadHeader()>0)
    {
      Wait();

      HEADER_TYPE HeaderType=Arc.GetHeaderType();
      if (HeaderType==HEAD_ENDARC)
      {
        OpenNext|=Arc.EndArcHead.NextVolume; // Allow open next volume.
        break;
      }
      if (HeaderType==HEAD_FILE)
      {
        if ((Arc.Format==RARFMT14 || Arc.Format==RARFMT15) && Arc.FileHead.UnpVer<=15)
        {
          // RAR versions earlier than 2.0 do not set per file solid flag.
          // They have only the global archive solid flag, so we can't
          // reliably analyze them here.
          OpenNext=false;
          break;
        }

        if (!Arc.FileHead.SplitBefore)
        {
          if (!MatchFound && !Arc.FileHead.Solid) // Can start extraction from here.
          {
            // We would gain nothing and unnecessarily complicate extraction
            // if we set StartName for first volume or StartPos for first
            // archived file.
            if (!FirstVolume)
              Analyze.StartName=NextName;

            // We shall set FirstFile once for all volumes for this code
            // to work properly. Alternatively we could append 
            // "|| Analyze.StartPos!=0" to the condition, so we do not reuse
            // the outdated Analyze.StartPos value from previous volume.
            if (!FirstFile)
              Analyze.StartPos=Arc.CurBlockPos;
          }

          if (Cmd->IsProcessFile(Arc.FileHead,NULL,MATCH_WILDSUBPATH,0,NULL)!=0)
          {
            MatchFound = true;
            PrevMatched = true;

            // Reset the previously set early exit position, if any, because
            // we found a new matched file.
            Analyze.EndPos=0;

            // Matched file reference pointing at maybe non-matched source file.
            // Even though we know RedirName, we can't check if source file
            // is certainly non-matched, because it can be filtered out by
            // date or attributes, which we do not know here.
            if (Arc.FileHead.RedirType==FSREDIR_FILECOPY)
            {
              bool AlreadyAdded=false;
              for (size_t I=0;I<RefList.size();I++)
                if (Arc.FileHead.RedirName==RefList[I].RefName)
                {
                  // Increment the reference count if we added such reference
                  // source earlier.
                  RefList[I].RefCount++;
                  AlreadyAdded=true;
                  break;
                }

              // Limit the maximum size of reference sources list to some
              // sensible value to prevent the excessive memory allocation.
              size_t MaxListSize=1000000;

              if (!AlreadyAdded && RefList.size()<MaxListSize)
              {
                ExtractRef Ref{};
                Ref.RefName=Arc.FileHead.RedirName;
                Ref.RefCount=1;
                RefList.push_back(Ref);
              }
            }
          }
          else
          {
            if (PrevMatched) // First non-matched item after matched.
            {
              // We would perform the unnecessarily string comparison
              // when extracting if we set this value for first volume
              // or non-volume archive.
              if (!FirstVolume)
                Analyze.EndName=NextName;
              Analyze.EndPos=Arc.CurBlockPos;
            }
            PrevMatched=false;
          }
        }

        FirstFile=false;
        if (Arc.FileHead.SplitAfter)
        {
          OpenNext=true; // Allow open next volume.
          break;
        }
      }
      Arc.SeekToNext();
    }
    Arc.Close();

    if (Volume && OpenNext)
    {
      NextVolumeName(NextName,!Arc.NewNumbering);
      FirstVolume=false;

      // Needed for multivolume archives. Added in case some 'break'
      // will quit early from loop above, so we do not set it in the loop.
      // Now it can happen for hypothetical archive without file records
      // and with HEAD_ENDARC record.
      FirstFile=false;
    }
    else
      break;
  }

  // If file references are present, we can't reliably skip in semi-solid
  // archives, because reference source can be present in skipped data.
  if (RefList.size()!=0)
    Analyze={};
}
#endif


#ifndef SFX_MODULE
// Return the first volume name if all volumes preceding the specified
// are available. Otherwise return the specified volume name.
void CmdExtract::GetFirstVolIfFullSet(const std::wstring &SrcName,bool NewNumbering,std::wstring &DestName)
{
  std::wstring FirstVolName;
  VolNameToFirstName(SrcName,FirstVolName,NewNumbering);
  std::wstring NextName=FirstVolName;
  std::wstring ResultName=SrcName;
  while (true)
  {
    if (SrcName==NextName)
    {
      ResultName=FirstVolName; // Reached the specified volume starting from the first.
      break;
    }
    if (!FileExist(NextName))
      break;
    NextVolumeName(NextName,!NewNumbering);
  }
  DestName=ResultName;
}
#endif


bool CmdExtract::CheckWinLimit(Archive &Arc,std::wstring &ArcFileName)
{
  if (Arc.FileHead.WinSize<=Cmd->WinSizeLimit || Arc.FileHead.WinSize<=Cmd->WinSize)
    return true;
  if (uiDictLimit(Cmd,ArcFileName,Arc.FileHead.WinSize,Max(Cmd->WinSizeLimit,Cmd->WinSize)))
  {
    // No more prompts when extracting other files. Important for GUI versions,
    // where we might not have [Max]WinSize set permanently when extracting.
    Cmd->WinSizeLimit=Arc.FileHead.WinSize;
  }
  else
  {
    ErrHandler.SetErrorCode(RARX_FATAL);
#ifdef RARDLL
    Cmd->DllError=ERAR_LARGE_DICT;
#endif
    Arc.SeekToNext();
    return false;
  }
  return true;
}
