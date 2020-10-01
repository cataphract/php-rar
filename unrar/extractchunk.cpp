#include "rar.hpp"
bool CmdExtract::ExtractCurrentFileChunkInit(Archive &Arc,
                                             size_t HeaderSize,
                                             bool &Repeat)
{
  wchar Command = L'T';

  Cmd->DllError = false;
  Repeat = false;
  FirstFile = true;

  if (HeaderSize==0)
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return false;
#else
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
  if (HeaderType!=HEAD_FILE)
  {
    return false;
  }

  DataIO.SetUnpackToMemory((byte*) this->Buffer, this->BufferSize);
  DataIO.SetSkipUnpCRC(true);
  DataIO.SetCurrentCommand(Command);
  //will still write to mem, as we've told it, but if I've screwed up the
  //there'll be no operations in the filesystem
  DataIO.SetTestMode(true);

  if (Arc.FileHead.SplitBefore && FirstFile)
  {
    wchar CurVolName[NM];
    wcsncpyz(CurVolName,ArcName,ASIZE(CurVolName));
    VolNameToFirstName(ArcName,ArcName,ASIZE(ArcName),Arc.NewNumbering);

    if (wcsicomp(ArcName,CurVolName)!=0 && FileExist(ArcName))
    {
      // If first volume name does not match the current name and if such
      // volume name really exists, let's unpack from this first volume.
      *ArcName=0;
      Repeat=true;
      ErrHandler.SetErrorCode(RARX_WARNING);
      /* Actually known. The problem is that the file doesn't start on this volume. */
      Cmd->DllError = ERAR_UNKNOWN;
      return false;
    }
    wcsncpyz(ArcName,CurVolName,ASIZE(ArcName));
  }

  DataIO.UnpVolume=Arc.FileHead.SplitAfter;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);

  if (Arc.FileHead.Encrypted)
  {
    if (!ExtrDllGetPassword())
    {
      ErrHandler.SetErrorCode(RARX_WARNING);
      Cmd->DllError=ERAR_MISSING_PASSWORD;
      return false;
    }
  }

  if (*Cmd->DllDestName!=0)
  {
    wcsncpyz(DestFileName,Cmd->DllDestName,ASIZE(DestFileName));
//      Do we need this code?
//      if (Cmd->DllOpMode!=RAR_EXTRACT)
//        ExtrFile=false;
  }

  wchar ArcFileName[NM];
  ConvertPath(Arc.FileHead.FileName,ArcFileName,ASIZE(ArcFileName));
  if (!CheckUnpVer(Arc,ArcFileName))
  {
    ErrHandler.SetErrorCode(RARX_FATAL);
    Cmd->DllError=ERAR_UNKNOWN_FORMAT;
    return false;
  }

    SecPassword FilePassword=Cmd->Password;
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    ConvertDosPassword(Arc,FilePassword);
#endif

  byte PswCheck[SIZE_PSWCHECK];
  DataIO.SetEncryption(false,Arc.FileHead.CryptMethod,&FilePassword,
     Arc.FileHead.SaltSet ? Arc.FileHead.Salt:NULL,
     Arc.FileHead.InitV,Arc.FileHead.Lg2Count,
     Arc.FileHead.HashKey,PswCheck);

  // If header is damaged, we cannot rely on password check value,
  // because it can be damaged too.
  if (Arc.FileHead.Encrypted && Arc.FileHead.UsePswCheck &&
      memcmp(Arc.FileHead.PswCheck,PswCheck,SIZE_PSWCHECK)!=0 &&
      !Arc.BrokenHeader)
  {
    ErrHandler.SetErrorCode(RARX_BADPWD);
  }
  DataIO.CurUnpRead=0;
  DataIO.CurUnpWrite=0;
  DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
  DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
  DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
  DataIO.SetFiles(&Arc,NULL);
  DataIO.SetTestMode(true);
  DataIO.SetSkipUnpCRC(true);

  return true;
}

bool CmdExtract::ExtractCurrentFileChunk(CommandData *Cmd, Archive &Arc,
                                         size_t *ReadSize,
                                         int *finished)
{
  if (Arc.FileHead.RedirType!=FSREDIR_NONE|| Arc.IsArcDir()) {
    *ReadSize = 0;
    *finished = TRUE;
    return true;
  }

  if (Arc.FileHead.Method==0) {
    // we use to call UnstoreFile here, together with unpack to memory
    // but it's easier to bypass the intermediate buffer altogether
    // UnstoreFile would need to be changed anyway to avoid it reading
    // the whole file in one go till the output memory buffer is full
    int read = DataIO.UnpRead((byte *)this->Buffer, this->BufferSize);
    if (read <= 0) {
      *ReadSize = 0;
      *finished = true;
    } else {
      *ReadSize = (size_t)read;
      *finished = false;
    }
  }
  else
  {
    DataIO.SetUnpackToMemory((byte*) this->Buffer, this->BufferSize);
    Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
    Unp->SetDestSize(Arc.FileHead.UnpSize);
    if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
      Unp->DoUnpack(15,FileCount>1 && Arc.Solid, this->Buffer != NULL);
    else
      Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid, this->Buffer != NULL);
    *finished = Unp->IsFileExtracted();
    *ReadSize = this->BufferSize - DataIO.GetUnpackToMemorySizeLeft();
  }

  return true;
}
