/* Handle fix80  / ea3 files */
void convertEa3 (unsigned char *buffer, int len, const char *outputName)
{
    FILE *fp;
    bool compressed = false;

    if ((fp = fopen (outputName, "w")) == NULL)
    {
        fprintf (stderr, "Can't create %s\n", outputName);
        exit (1);
    }

    int pos = 0;
    char *segment = "";
    char value[5];
    char *name;
    bool done = false;
    while (pos < len)
    {
        unsigned char tag;

        tag = buffer[pos++];

        switch (tag)
        {
        case 'A':
        case '0': segment = "length"; break;
        case '3': segment = "REF"; break;
        case '5': segment = "DEF"; break;
        case '7': segment = "checksum"; break;
        case 'B': segment = "DATA"; break;
        case 'C': segment = "DATA-offset"; break;
        case 'F':
            pos = ((pos / 80) + 1) * 80;
            fprintf (fp, "         EOR %-8.8s pos=%d\n", &buffer[pos-8], pos);

            /* If end of 3rd record then advance to next sector */
            if (pos == 240)
            {
                buffer += 256;
                len -= 256;
                pos = 0;
            }
            continue;

        case 0xE5: done = true; break;
        case 0x01: segment="TAG01"; break; 

        default:
            fprintf (fp, "         tbd %02x\n", buffer[pos-1]);
            segment="tbd";
            break;
        }

        if (done)
            break;

        if (tag == 0x01)
        {
            compressed = true;
            pos += getValue (buffer+pos, value, compressed);
            fprintf (fp, "tag 01 d=%s s='%-8.8s'\n", 
                     value,
                     buffer+pos);
            pos += 8;
        }
        else
        if (tag == '0')
        {
            pos += getValue (buffer+pos, value, compressed);
            fprintf (fp, "%-8.8s %s %s\n", &buffer[pos], segment, value);
        }
        else if (strchr ("3456GHUVY", tag))
        {
            pos += getValue (buffer+pos, value, compressed);
            fprintf (fp, "%-6.6s %s %4.4s\n", &buffer[pos], segment, value);
            pos += 6;
        }
        else if (strchr ("12789ABCDST", tag))
        {
            pos += getValue (buffer+pos, value, compressed);
            fprintf (fp, "         %s %s\n", segment, value);
        }
        else if (tag == ':')
        {
            fprintf (fp, "EOF %-71.71s\n", &buffer[pos]);
            break;
        }
        else
        {
            fprintf (stderr, "Unknown length for tag %c (%02X)\n", tag, tag);
            exit(1);
        }

    }

    fclose (fp);
}


