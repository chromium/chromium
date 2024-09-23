#include "rar.hpp"


#include "recvol3.cpp"
#include "recvol5.cpp"



bool RecVolumesRestore(CommandData *Cmd,const std::wstring &Name,bool Silent)
{
  Archive Arc(Cmd);
  if (!Arc.Open(Name))
  {
    if (!Silent)
      ErrHandler.OpenErrorMsg(Name);
    return false;
  }

  RARFORMAT Fmt=RARFMT15;
  if (Arc.IsArchive(true))
    Fmt=Arc.Format;
  else
  {
    byte Sign[REV5_SIGN_SIZE];
    Arc.Seek(0,SEEK_SET);
    if (Arc.Read(Sign,REV5_SIGN_SIZE)==REV5_SIGN_SIZE && memcmp(Sign,REV5_SIGN,REV5_SIGN_SIZE)==0)
      Fmt=RARFMT50;
  }
  Arc.Close();

  // We define RecVol as local variable for proper stack unwinding when
  // handling exceptions. So it can close and delete files on Cancel.
  if (Fmt==RARFMT15)
  {
    RecVolumes3 RecVol(Cmd,false);
    return RecVol.Restore(Cmd,Name,Silent);
  }
  else
  {
    RecVolumes5 RecVol(Cmd,false);
    return RecVol.Restore(Cmd,Name,Silent);
  }
}


void RecVolumesTest(CommandData *Cmd,Archive *Arc,const std::wstring &Name)
{
  std::wstring RevName;
  if (Arc==NULL)
    RevName=Name;
  else
  {
    // We received .rar or .exe volume as a parameter, trying to find
    // the matching .rev file number 1.
    bool NewNumbering=Arc->NewNumbering;

    std::wstring RecVolMask;
    size_t VolNumStart=VolNameToFirstName(Name,RecVolMask,NewNumbering);
    RecVolMask.replace(VolNumStart, std::wstring::npos, L"*.rev");

    FindFile Find;
    Find.SetMask(RecVolMask);
    FindData RecData;

    while (Find.Next(&RecData))
    {
      size_t NumPos=GetVolNumPos(RecData.Name);
      if (RecData.Name[NumPos]!='1') // Name must have "0...01" numeric part.
        continue;
      bool FirstVol=true;
      while (NumPos>0 && IsDigit(RecData.Name[--NumPos]))
        if (RecData.Name[NumPos]!='0')
        {
          FirstVol=false;
          break;
        }
      if (FirstVol)
      {
        RevName=RecData.Name;
        break;
      }
    }
    if (RevName.empty()) // First .rev file not found.
      return;
  }
  
  File RevFile;
  if (!RevFile.Open(RevName))
  {
    ErrHandler.OpenErrorMsg(RevName); // It also sets RARX_OPEN.
    return;
  }
  mprintf(L"\n");
  byte Sign[REV5_SIGN_SIZE];
  bool Rev5=RevFile.Read(Sign,REV5_SIGN_SIZE)==REV5_SIGN_SIZE && memcmp(Sign,REV5_SIGN,REV5_SIGN_SIZE)==0;
  RevFile.Close();
  if (Rev5)
  {
    RecVolumes5 RecVol(Cmd,true);
    RecVol.Test(Cmd,RevName);
  }
  else
  {
    RecVolumes3 RecVol(Cmd,true);
    RecVol.Test(Cmd,RevName);
  }
}
