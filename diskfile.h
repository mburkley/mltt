typedef struct
{
    char tiname[10];
    int16_t _reserved;
    uint8_t flags;
    uint8_t recSec;
    int16_t secCount;
    uint8_t eofOffset;
    uint8_t recLen;
    int16_t l3Alloc;
    char dtCreate[4];
    char dtUpdate[4];
    uint8_t chain[MAX_FILE_CHAINS][3];
}
DskFileHeader;

class DiskFile
{
public:
    const char *getName () { return osname; }
    int getInode () { return inode; }
    int getLength () {return length; }
    int getFlags () { return filehdr.flags; }
    int getRecLen () { return filehdr.recLen; }



private:
    DskFileHeader filehdr;
    char osname[FILENAME_LEN+100]; // TODO
    int sector;
    int length;
    struct
    {
        int start;
        int end;
    }
    chains[MAX_FILE_CHAINS];
    int chainCount;
    bool needsWrite;
    bool unlinked;
    int refCount;
    int inode;
    int pos;
    struct _dskFileInfo *next;
};


