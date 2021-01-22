#ifndef _RAR_SAVEPOS_
#define _RAR_SAVEPOS_

class SaveFilePos
{
  private:
    File *SaveFile;
    int64 SavePos;
  public:
    SaveFilePos(File &Src)
    {
      SaveFile=&Src;
      SavePos=Src.Tell();
    }
    ~SaveFilePos()
    {
      // If file is already closed by current exception processing,
      // we would get uneeded error messages and an exception inside of
      // exception and terminate if we try to seek without checking
      // if file is still opened. We should not also restore the position
      // if external code closed the file on purpose.
      if (SaveFile->IsOpened())
        SaveFile->Seek(SavePos,SEEK_SET);
    }
};

#endif
