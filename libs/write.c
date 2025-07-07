#include <tga.h>

int WriteByte(UINT8 uc, FILE *fp)
{
    if (fwrite(&uc, 1, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteShort(UINT16 us, FILE *fp)
{
    if (fwrite(&us, 2, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteLong(UINT32 ul, FILE *fp)
{
    if (fwrite(&ul, 4, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteStr(char *p, int n, FILE *fp)
{
    if (fwrite(p, 1, n, fp) == n)
        return 0;
    return -1;
}

int WriteColorTable(FILE *fp, TGAFile *sp)
{
    UINT16 *p;
    UINT16 n;

    p = sp->colorCorrectTable;
    for (n = 0; n < 1024; ++n)
    {
        if (WriteShort(*p++, fp) < 0)
            return -1;
    }
    return 0;
}

/*
** Retrieve a pixel value from a buffer.  The actual size and order
** of the bytes is not important since we are only using the value
** for comparisons with other pixels.
*/
static UINT32 GetPixel(unsigned char *p, int bpp)
{
    unsigned long pixel;

    pixel = (unsigned long) *p++;
    while (bpp-- > 1)
    {
        pixel <<= 8;
        pixel |= (unsigned long) *p++;
    }
    return pixel;
}

/*
** Count pixels in buffer until two identical adjacent ones found
*/
static int CountDiffPixels(char *p, int bpp, int pixCnt)
{
    unsigned long pixel;
    unsigned long nextPixel;
    int n;

    n = 0;
    if (pixCnt == 1)
        return pixCnt;
    pixel = GetPixel(p, bpp);
    while (pixCnt > 1)
    {
        p += bpp;
        nextPixel = GetPixel(p, bpp);
        if (nextPixel == pixel)
            break;
        pixel = nextPixel;
        ++n;
        --pixCnt;
    }
    if (nextPixel == pixel)
        return n;
    return n + 1;
}

static int CountSamePixels(char *p, int bpp, int pixCnt)
{
    unsigned long pixel;
    unsigned long nextPixel;
    int n;

    n = 1;
    pixel = GetPixel(p, bpp);
    pixCnt--;
    while (pixCnt > 0)
    {
        p += bpp;
        nextPixel = GetPixel(p, bpp);
        if (nextPixel != pixel)
            break;
        ++n;
        --pixCnt;
    }
    return n;
}

/*
char    *p;             data to be encoded
char    *q;             encoded buffer
int     n;              number of pixels in buffer
int     bpp;            bytes per pixel
 */
int RLEncodeRow(char *p, char *q, int n, int bpp)
{
    int diffCount;  /* pixel count until two identical */
    int sameCount;  /* number of identical adjacent pixels */
    int RLEBufSize; /* count of number of bytes encoded */

    RLEBufSize = 0;
    while (n > 0)
    {
        diffCount = CountDiffPixels(p, bpp, n);
        sameCount = CountSamePixels(p, bpp, n);
        if (diffCount > 128)
            diffCount = 128;
        if (sameCount > 128)
            sameCount = 128;
        if (diffCount > 0)
        {
            /* create a raw packet */
            *q++ = (char) (diffCount - 1);
            n -= diffCount;
            RLEBufSize += diffCount * bpp + 1;
            while (diffCount > 0)
            {
                *q++ = *p++;
                if (bpp > 1)
                    *q++ = *p++;
                if (bpp > 2)
                    *q++ = *p++;
                if (bpp > 3)
                    *q++ = *p++;
                diffCount--;
            }
        }
        if (sameCount > 1)
        {
            /* create a RLE packet */
            *q++ = (char) (sameCount - 1 | 0x80);
            n -= sameCount;
            RLEBufSize += bpp + 1;
            p += (sameCount - 1) * bpp;
            *q++ = *p++;
            if (bpp > 1)
                *q++ = *p++;
            if (bpp > 2)
                *q++ = *p++;
            if (bpp > 3)
                *q++ = *p++;
        }
    }
    return RLEBufSize;
}

int WriteTGAFile(FILE *ofp, TGAFile *sp)
{
    /*
    ** The output file was just opened, so the first data
    ** to be written is the standard header based on the
    ** original TGA specification.
    */
    if (WriteByte(sp->idLength, ofp) < 0)
    {
        return -1;
    }
    if (WriteByte(sp->mapType, ofp) < 0)
    {
        return -1;
    }
    if (WriteByte(sp->imageType, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->mapOrigin, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->mapLength, ofp) < 0)
    {
        return -1;
    }
    if (WriteByte(sp->mapWidth, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->xOrigin, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->yOrigin, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->imageWidth, ofp) < 0)
    {
        return -1;
    }
    if (WriteShort(sp->imageHeight, ofp) < 0)
    {
        return -1;
    }
    if (WriteByte(sp->pixelDepth, ofp) < 0)
    {
        return -1;
    }
    if (WriteByte(sp->imageDesc, ofp) < 0)
    {
        return -1;
    }
    if (sp->idLength && WriteStr(sp->idString, sp->idLength, ofp) < 0)
    {
        return -1;
    }
    return 0;
}
