#include "rar.hpp"

/* Zone.Identifier stream can include the text like:

[ZoneTransfer]
ZoneId=3
HostUrl=https://site/path/file.ext
ReferrerUrl=d:\path\archive.ext

Where ZoneId can be:

    0 = My Computer
    1 = Local intranet
    2 = Trusted sites
    3 = Internet
    4 = Restricted sites
*/

MarkOfTheWeb::MarkOfTheWeb()
{
  ZoneIdValue=-1; // -1 indicates the missing MOTW.
  AllFields=false;
}


void MarkOfTheWeb::Clear()
{
  ZoneIdValue=-1;
}


void MarkOfTheWeb::ReadZoneIdStream(const std::wstring &FileName,bool AllFields)
{
  MarkOfTheWeb::AllFields=AllFields;
  ZoneIdValue=-1;
  ZoneIdStream.clear();

  std::wstring StreamName=FileName+MOTW_STREAM_NAME;

  File SrcFile;
  if (SrcFile.Open(StreamName))
  {
    ZoneIdStream.resize(MOTW_STREAM_MAX_SIZE);
    int BufSize=SrcFile.Read(&ZoneIdStream[0],ZoneIdStream.size());
    ZoneIdStream.resize(BufSize<0 ? 0:BufSize);

    if (BufSize<=0)
      return;

    ZoneIdValue=ParseZoneIdStream(ZoneIdStream);
  }
}


// 'Stream' contains the raw "Zone.Identifier" NTFS stream data on input
// and either raw or cleaned stream data on output.
int MarkOfTheWeb::ParseZoneIdStream(std::string &Stream)
{
  if (Stream.rfind("[ZoneTransfer]",0)==std::string::npos)
    return -1; // Not a valid Mark of the Web. Prefer the archive MOTW if any.

  std::string::size_type ZoneId=Stream.find("ZoneId=",0);
  if (ZoneId==std::string::npos || !IsDigit(Stream[ZoneId+7]))
    return -1; // Not a valid Mark of the Web.
  int ZoneIdValue=atoi(&Stream[ZoneId+7]);
  if (ZoneIdValue<0 || ZoneIdValue>4)
    return -1; // Not a valid Mark of the Web.

  if (!AllFields)
    Stream="[ZoneTransfer]\r\nZoneId=" + std::to_string(ZoneIdValue) + "\r\n";

  return ZoneIdValue;
}


void MarkOfTheWeb::CreateZoneIdStream(const std::wstring &Name,StringList &MotwList)
{
  if (ZoneIdValue==-1)
    return;

  size_t ExtPos=GetExtPos(Name);
  const wchar *Ext=ExtPos==std::wstring::npos ? L"":&Name[ExtPos+1];

  bool Matched=false;
  const wchar *CurMask;
  MotwList.Rewind();
  while ((CurMask=MotwList.GetString())!=nullptr)
  {
    // Perform the fast extension comparison for simple *.ext masks.
    // Also we added the fast path to wcsicomp for English only strings.
    // When extracting 100000 files with "Exe and office" masks set
    // this loop spent 85ms with this optimization and wcsicomp optimized
    // for English strings, 415ms with this optimization only, 475ms with
    // wcsicomp optimized only and 795ms without both optimizations.
    bool FastCmp=CurMask[0]=='*' && CurMask[1]=='.' && wcspbrk(CurMask+2,L"*?")==NULL;
    if (FastCmp && wcsicomp(Ext,CurMask+2)==0 || !FastCmp && CmpName(CurMask,Name,MATCH_NAMES))
    {
      Matched=true;
      break;
    }
  }

  if (!Matched)
    return;

  std::wstring StreamName=Name+MOTW_STREAM_NAME;
  
  File StreamFile;
  if (StreamFile.Create(StreamName)) // Can fail on FAT.
  {
    // We got a report that write to stream failed on Synology 2411+ NAS drive.
    // So we handle it silently instead of aborting.
    StreamFile.SetExceptions(false);
    if (StreamFile.Write(&ZoneIdStream[0],ZoneIdStream.size()))
      StreamFile.Close();
  }
}


bool MarkOfTheWeb::IsNameConflicting(const std::wstring &StreamName)
{
  // We must use the case insensitive comparison for L":Zone.Identifier"
  // to catch specially crafted archived streams like L":zone.identifier".
  return wcsicomp(StreamName,MOTW_STREAM_NAME)==0 && ZoneIdValue!=-1;
}


// Return true and prepare the file stream to write if its ZoneId is stricter
// than archive ZoneId. If it is missing, less or equally strict, return false.
bool MarkOfTheWeb::IsFileStreamMoreSecure(std::string &FileStream)
{
  int StreamZone=ParseZoneIdStream(FileStream);
  return StreamZone>ZoneIdValue;
}
