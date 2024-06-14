#ifndef __DISKFILE_H
#define __DISKFILE_H

#include <string>

#include "types.h"
#include "disk.h"
#include "disksector.h"

#define MAX_FILE_CHAINS         76  // 0x1c to 0xff divided by 3
#define FILENAME_LEN            10

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
    DiskFile (const char *path, DiskSector *sectorMap, int dirSector,
              int firstDataSector, int inode);
    // DiskFile (const char *path, int dirSector);
    std::string& getOSName () { return _osname; }
    void setOSName (const char *path);
    char *getTIName () { return _filehdr.tiname; }
    int getInode () { return _inode; }
    int getLength () {return _length; }
    int getFlags () { return _filehdr.flags; }
    int getRecLen () { return _filehdr.recLen; }
    void setName (const char *path);
    bool isProtected () { return _filehdr.flags & FLAG_WP; }
    bool isUnlinked () { return _unlinked; }
    int getDirSector () { return _dirSector; }
    int getSecCount () { return _filehdr.secCount; }
    void setRecLen (int recLen);
    void setFlags (int flags);
    bool close ();
    bool unlink();
    int seek (int offset, int whence);
    bool create ();
    int read (uint8_t *buff, int offset, int len);
    int write (uint8_t *buff, int offset, int len);
    void toTifiles (Tifiles *header);
    void fromTifiles (Tifiles *header);
    void printInfo (FILE *out);
    void incRefCount () { _refCount++; }
    void decRefCount () { _refCount--; }
    void readDirEnt ();
    void sync();

private:
    DiskFileHeader _filehdr;
    DiskSector *_sectorMap;
    std::string _osname;
    int _dirSector;
    int _firstDataSector;
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
    int _pos;
    void freeResources ();
    void decodeOneChain (uint8_t chain[], uint16_t *p1, uint16_t *p2);
    void encodeOneChain (uint8_t chain[], uint16_t p1, uint16_t p2);
    void encodeChain ();
    void decodeChain ();
};

#endif

