void DiskChain::decodeOne (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

void DiskChain::encodeOne (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

void DiskChain::decode (uint8_t *chain, int chainCount)
{
    // TODO why 23?
    file->chainCount = 0;

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=file->filehdr.chain[i];

        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeOne (chain, &start, &len);

            file->chains[chainCount].start = start;
            file->chains[chainCount].end = start+len;
            file->chainCount++;
        }
    }
}

void DiskChain::encode (uint8_t *chain, int chainCount)
{
    // TODO why 23?

    for (int i = 0; i < 23; i++)
    {
        // uint8_t *chain=file->filehdr.chain[i];

        if (i < chainCount)
        {
            encodeOne (chain,
                         file->chains[i].start,
                         file->chains[i].end - file->chains[i].start);
        }
        else
        {
            chain[0] = chain[1] = chain[2] = 0;
        }
    }
}

