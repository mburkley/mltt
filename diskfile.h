#ifndef __DISKFILE_H
#define __DISKFILE_H

#include <string>

#include "types.h"
#include "disk.h"

#define MAX_FILE_CHAINS         76
#define FILENAME_LEN            11

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
DiskFileHeader;

class DiskFile
{
public:
    // const char *getName () { return osname; }
    std::string& getName () { return _osname; }
    int getInode () { return _inode; }
    int getLength () {return _length; }
    int getFlags () { return _filehdr.flags; }
    int getRecLen () { return _filehdr.recLen; }
    void setName (const char *path);
    bool isProtected () { return _filehdr.flags & FLAG_WP; }
    int getSecCount () { return _filehdr.secCount; }
    void setRecLen (int recLen);
    void setFlags (int flags);
    int seek (int offset, int whence);
    void fromTifiles (Tifiles *header);
    DiskFile *next () { return _next; }
    int dirSector () { return _sector; }
    std::string& getOsName () { return _osname; }

private:
    DiskFileHeader _filehdr;
    std::string _osname; // [FILENAME_LEN+100]; // TODO
    int _sector;
    int _length;
    struct
    {
        int start;
        int end;
    }
    _chains[MAX_FILE_CHAINS];
    int _chainCount;
    bool _needsWrite;
    bool _unlinked;
    int _refCount;
    int _inode;
    int pos;
    void freeResources ();
};

#endif

