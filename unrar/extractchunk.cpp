#include "rar.hpp"

bool CmdExtract::ExtractCurrentFileChunkInit(CommandData *Cmd,
                                             Archive &Arc,
                                             size_t HeaderSize,
                                             bool &Repeat)
{
  char Command = 'T';

  Cmd->DllError=0;
  Repeat = false;

  //turn on checks reserved for the first files extracted from an archive?
  FirstFile = true;

  if (HeaderSize==0) {
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return(false);
#else
      if (!MergeArchive(Arc,&DataIO,false,Command)) //command irrelevant
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
        return false;
      }
      SignatureFound=false;
#endif
    }
    else
      return false;
  }

  int HeadType=Arc.GetHeaderType();
  if (HeadType!=FILE_HEAD)
    return false;

  DataIO.SetUnpackToMemory((byte*) this->Buffer, this->BufferSize);
  DataIO.SetSkipUnpCRC(true);
  DataIO.SetCurrentCommand(Command);
  //will still write to mem, as we've told it, but if I've screwed up the
  //there'll be no operations in the filesystem
  DataIO.SetTestMode(true);

  if ((Arc.NewLhd.Flags & (LHD_SPLIT_BEFORE/*|LHD_SOLID*/)) && FirstFile)
  {
    char CurVolName[NM];
    /* are these first two needed? */
    wcsncpyz(ArcNameW, Arc.FileNameW, ASIZE(ArcNameW));
    strncpyz(ArcName, Arc.FileName, NM);
    strncpyz(CurVolName, ArcName, sizeof CurVolName);

    bool NewNumbering=(Arc.NewMhd.Flags & MHD_NEWNUMBERING)!=0;
    VolNameToFirstName(ArcName,ArcName,NewNumbering);
    if (*ArcNameW!=0)
      VolNameToFirstName(ArcNameW,ArcNameW,NewNumbering);

    if (stricomp(ArcName,CurVolName)!=0 && FileExist(ArcName,ArcNameW))
    {
      *ArcNameW=0;
      Repeat=true;
      ErrHandler.SetErrorCode(RARX_WARNING);
      /* Actually known. The problem is that the file doesn't start on this volume. */
      Cmd->DllError = ERAR_UNKNOWN;
      return false;
    }
    strcpy(ArcName,CurVolName);
  }
  DataIO.UnpVolume=(Arc.NewLhd.Flags & LHD_SPLIT_AFTER)!=0;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos - Arc.NewLhd.FullPackSize, SEEK_SET);

  if ((Arc.NewLhd.Flags & LHD_PASSWORD)!=0)
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
          GetWideName(PasswordA,NULL,PasswordW,ASIZE(PasswordW));
          cleandata(PasswordA,sizeof(PasswordA));
        }
        Cmd->Password.Set(PasswordW);
        cleandata(PasswordW,sizeof(PasswordW));
      }
      if (!Cmd->Password.IsSet())
      {
        Cmd->DllError = ERAR_MISSING_PASSWORD; //added by me
        return false;
      }
    }
    Password=Cmd->Password;
  }

  if (Arc.NewLhd.UnpVer<13 || Arc.NewLhd.UnpVer>UNP_VER)
  {
    ErrHandler.SetErrorCode(RARX_WARNING);
    Cmd->DllError=ERAR_UNKNOWN_FORMAT;
    return false;
  }

  if (IsLink(Arc.NewLhd.FileAttr))
    return true;
  
  if (Arc.IsArcDir())
      return true;

  DataIO.CurUnpRead=0;
  DataIO.CurUnpWrite=0;
  DataIO.UnpFileCRC= Arc.OldFormat ? 0 : 0xffffffff;
  DataIO.PackedCRC= 0xffffffff;
  DataIO.SetEncryption(
    (Arc.NewLhd.Flags & LHD_PASSWORD) ? Arc.NewLhd.UnpVer : 0, &Password,
    (Arc.NewLhd.Flags & LHD_SALT) ? Arc.NewLhd.Salt : NULL, false,
    Arc.NewLhd.UnpVer >= 36);
  DataIO.SetPackedSizeToRead(Arc.NewLhd.FullPackSize);
  DataIO.SetSkipUnpCRC(true);
  DataIO.SetFiles(&Arc, NULL);

  return true;
}

bool CmdExtract::ExtractCurrentFileChunk(CommandData *Cmd, Archive &Arc,
                                         size_t *ReadSize,
                                         int *finished)
{
  if (IsLink(Arc.NewLhd.FileAttr) || Arc.IsArcDir()) {
    *ReadSize = 0;
    *finished = TRUE;
    return true;
  }

  DataIO.SetUnpackToMemory((byte*) this->Buffer, this->BufferSize);

  if (Arc.NewLhd.Method==0x30) {
    UnstoreFile(DataIO, this->BufferSize);
    /* not very sophisticated and may result in a subsequent
     * unnecessary call to this function (and probably will if
     * the buffer size is chosen so that it just fits for small
     * files) */
    *finished = (DataIO.GetUnpackToMemorySizeLeft() > 0);
  }
  else
  {
    Unp->SetDestSize(Arc.NewLhd.FullUnpSize);
    if (Arc.NewLhd.UnpVer<=15)
      Unp->DoUnpack(15,FileCount>1 && Arc.Solid, this->Buffer != NULL);
    else
      Unp->DoUnpack(Arc.NewLhd.UnpVer,
        (Arc.NewLhd.Flags & LHD_SOLID)!=0, this->Buffer != NULL);
    *finished = Unp->IsFileExtracted();
  }
  *ReadSize = this->BufferSize - DataIO.GetUnpackToMemorySizeLeft();

  return true;
}
