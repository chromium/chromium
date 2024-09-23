#include "rar.hpp"

StringList::StringList()
{
  Reset();
}


void StringList::Reset()
{
  Rewind();
  StringData.clear();
  StringsCount=0;
  SavePosNumber=0;
}


/*
void StringList::AddStringA(const char *Str)
{
  std::wstring StrW;
  CharToWide(Str,StrW);
  AddString(StrW);
}
*/


void StringList::AddString(const wchar *Str)
{
  if (Str==NULL)
    Str=L"";

  size_t PrevSize=StringData.size();
  StringData.resize(PrevSize+wcslen(Str)+1);
  wcscpy(&StringData[PrevSize],Str);

  StringsCount++;
}


void StringList::AddString(const std::wstring &Str)
{
  AddString(Str.c_str());
}


/*
bool StringList::GetStringA(char *Str,size_t MaxLength)
{
  std::wstring StrW;
  if (!GetString(StrW))
    return false;
  WideToChar(StrW.c_str(),Str,MaxLength);
  return true;
}
*/


bool StringList::GetString(wchar *Str,size_t MaxLength)
{
  wchar *StrPtr;
  if (!GetString(&StrPtr))
    return false;
  wcsncpyz(Str,StrPtr,MaxLength);
  return true;
}


bool StringList::GetString(std::wstring &Str)
{
  wchar *StrPtr;
  if (!GetString(&StrPtr))
    return false;
  Str=StrPtr;
  return true;
}


#ifndef SFX_MODULE
bool StringList::GetString(wchar *Str,size_t MaxLength,int StringNum)
{
  SavePosition();
  Rewind();
  bool RetCode=true;
  while (StringNum-- >=0)
    if (!GetString(Str,MaxLength))
    {
      RetCode=false;
      break;
    }
  RestorePosition();
  return RetCode;
}


bool StringList::GetString(std::wstring &Str,int StringNum)
{
  SavePosition();
  Rewind();
  bool RetCode=true;
  while (StringNum-- >=0)
    if (!GetString(Str))
    {
      RetCode=false;
      break;
    }
  RestorePosition();
  return RetCode;
}
#endif


wchar* StringList::GetString()
{
  wchar *Str;
  GetString(&Str);
  return Str;
}


bool StringList::GetString(wchar **Str)
{
  if (CurPos>=StringData.size()) // No more strings left unprocessed.
  {
    if (Str!=NULL)
      *Str=NULL;
    return false;
  }

  wchar *CurStr=&StringData[CurPos];
  CurPos+=wcslen(CurStr)+1;
  if (Str!=NULL)
    *Str=CurStr;

  return true;
}


void StringList::Rewind()
{
  CurPos=0;
}


#ifndef SFX_MODULE
bool StringList::Search(const std::wstring &Str,bool CaseSensitive)
{
  SavePosition();
  Rewind();
  bool Found=false;
  wchar *CurStr;
  while (GetString(&CurStr))
  {
    if (CurStr!=NULL)
      if (CaseSensitive && Str!=CurStr || !CaseSensitive && wcsicomp(Str,CurStr)!=0)
        continue;
    Found=true;
    break;
  }
  RestorePosition();
  return Found;
}
#endif


#ifndef SFX_MODULE
void StringList::SavePosition()
{
  if (SavePosNumber<ASIZE(SaveCurPos))
  {
    SaveCurPos[SavePosNumber]=CurPos;
    SavePosNumber++;
  }
}
#endif


#ifndef SFX_MODULE
void StringList::RestorePosition()
{
  if (SavePosNumber>0)
  {
    SavePosNumber--;
    CurPos=SaveCurPos[SavePosNumber];
  }
}
#endif
