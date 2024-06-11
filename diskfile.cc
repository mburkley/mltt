void dskFileFlagsSet (DskInfo *info, DskFileInfo *file, int flags)
{
    file->filehdr.flags = flags;
    file->needsWrite = true;
    writeDirectory (info);
}

void dskFileRecLenSet (DskInfo *info, DskFileInfo *file, int recLen)
{
    file->filehdr.recLen = recLen;
    file->filehdr.recSec = BYTES_PER_SECTOR / recLen;
    file->needsWrite = true;
    writeDirectory (info);
}

int dskFileSecCount (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.secCount;
}

bool dskFileProtected (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.flags & FLAG_WP;
}


