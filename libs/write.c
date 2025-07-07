#include <tga.h>

#define CBUFSIZE 2048 /* size of copy buffer */

static char copyBuf[CBUFSIZE];

int WriteByte(FILE *fp, UINT8 uc)
{
    if (fwrite(&uc, 1, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteShort(FILE *fp, UINT16 us)
{
    if (fwrite(&us, 2, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteLong(FILE *fp, UINT32 ul)
{
    if (fwrite(&ul, 4, 1, fp) == 1)
        return 0;
    return -1;
}

int WriteStr(FILE *fp, char *p, int n)
{
    if (fwrite(p, 1, n, fp) == n)
        return 0;
    return -1;
}

int WriteColorCorrectTable(TGAFile *sp, FILE *fp)
{
    UINT16 *p;
    UINT16 n;

    p = sp->colorCorrectTable;
    for (n = 0; n < 1024; ++n)
    {
        if (WriteShort(fp, *p++) < 0)
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

int WriteTGAFile(TGAFile *sp, FILE *ofp)
{
    /*
    ** The output file was just opened, so the first data
    ** to be written is the standard header based on the
    ** original TGA specification.
    */
    if (WriteByte(ofp, sp->idLength) < 0)
    {
        return -1;
    }
    if (WriteByte(ofp, sp->mapType) < 0)
    {
        return -1;
    }
    if (WriteByte(ofp, sp->imageType) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->mapOrigin) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->mapLength) < 0)
    {
        return -1;
    }
    if (WriteByte(ofp, sp->mapWidth) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->xOrigin) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->yOrigin) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->imageWidth) < 0)
    {
        return -1;
    }
    if (WriteShort(ofp, sp->imageHeight) < 0)
    {
        return -1;
    }
    if (WriteByte(ofp, sp->pixelDepth) < 0)
    {
        return -1;
    }
    if (WriteByte(ofp, sp->imageDesc) < 0)
    {
        return -1;
    }
    if (sp->idLength && WriteStr(ofp, sp->idString, sp->idLength) < 0)
    {
        return -1;
    }
    return 0;
}

int CopyTGAColormap(TGAFile *sp, FILE *in, FILE *out)
{
    /*
     ** Now we need to copy the color map data from the input file
     ** to the output file.
     */
    int byteCount = 18 + sp->idLength;
    if (fseek(in, byteCount, SEEK_SET) != 0)
    {
        return -1;
    }
    byteCount = ((sp->mapWidth + 7) >> 3) * (long) sp->mapLength;
    while (byteCount > 0)
    {
        if (byteCount - CBUFSIZE < 0)
        {
            fread(copyBuf, 1, (int) byteCount, in);
            if (fwrite(copyBuf, 1, (int) byteCount, out) != (int) byteCount)
            {
                return -1;
            }
        }
        else
        {
            fread(copyBuf, 1, CBUFSIZE, in);
            if (fwrite(copyBuf, 1, CBUFSIZE, out) != CBUFSIZE)
            {
                return -1;
            }
        }
        byteCount -= CBUFSIZE;
    }
    return 0;
}
