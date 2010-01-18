#ifndef _RAR_EXTRACT_
#define _RAR_EXTRACT_

enum EXTRACT_ARC_CODE {EXTRACT_ARC_NEXT,EXTRACT_ARC_REPEAT};

class CmdExtract
{
  private:
    EXTRACT_ARC_CODE ExtractArchive(CommandData *Cmd);
    RarTime StartTime; // time when extraction started

    ComprDataIO DataIO;
    Unpack *Unp;
    unsigned long TotalFileCount;

    unsigned long FileCount;
    unsigned long MatchedArgs;
    bool FirstFile;
    bool AllMatchesExact;
    bool ReconstructDone;

    char ArcName[NM];
    wchar ArcNameW[NM];

    char Password[MAXPASSWORD];
    bool PasswordAll;
    bool PrevExtracted;
    char DestFileName[NM];
    wchar DestFileNameW[NM];
    bool PasswordCancelled;
  public:
    CmdExtract();
    ~CmdExtract();
    void DoExtract(CommandData *Cmd);
    void ExtractArchiveInit(CommandData *Cmd,Archive &Arc);
    bool ExtractCurrentFile(CommandData *Cmd,Archive &Arc,size_t HeaderSize,
      bool &Repeat);
    bool ExtractCurrentFileChunkInit(CommandData *Cmd, Archive &Arc,
      size_t HeaderSize, bool &Repeat);
    bool ExtractCurrentFileChunk(CommandData *Cmd, Archive &Arc,
      size_t *ReadSize, int *finished);
    static void UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize);

    bool SignatureFound;
    //next two lines added by me
	void *Buffer;
	size_t BufferSize;
};

#endif
