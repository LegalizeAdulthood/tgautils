#include <stdlib.h>
#include <string.h>
#include <tga.h>

#define RLEBUFSIZ 512 /* size of largest possible RLE packet */
#define CBUFSIZE 2048 /* size of copy buffer */

static char rleBuf[RLEBUFSIZ];
static char copyBuf[CBUFSIZE];

UINT8 ReadByte(FILE *fp)
{
    UINT8 value;

    fread(&value, 1, 1, fp);
    return (value);
}

UINT16 ReadShort(FILE *fp)
{
    UINT16 value;

    fread(&value, 1, 2, fp);
    return (value);
}

UINT32 ReadLong(FILE *fp)
{
    UINT32 value;

    fread(&value, 1, 4, fp);
    return (value);
}

void ReadCharField(FILE *fp, char *p, int n)
{
    while (n)
    {
        *p++ = (char) fgetc(fp); /* no error check, no char conversion */
        --n;
    }
}

int ReadColorTable(FILE *fp, TGAFile *sp)
{
    UINT16 *p;
    UINT16 n;

    if (!fseek(fp, sp->colorCorrectOffset, SEEK_SET))
    {
        if (sp->colorCorrectTable = malloc(1024 * sizeof(UINT16)))
        {
            p = sp->colorCorrectTable;
            for (n = 0; n < 1024; ++n)
            {
                *p++ = ReadShort(fp);
            }
        }
        else
        {
            puts("Unable to allocate Color Correction Table.");
            return (-1);
        }
    }
    else
    {
        printf("Error seeking to Color Correction Table, offset = 0x%08x\n", sp->colorCorrectOffset);
        return (-1);
    }
    return (0);
}

int ReadScanLineTable(FILE *fp, TGAFile *sp)
{
    UINT32 *p;
    UINT16 n;

    if (!fseek(fp, sp->scanLineOffset, SEEK_SET))
    {
        if (sp->scanLineTable = malloc(sp->imageHeight << 2))
        {
            p = sp->scanLineTable;
            for (n = 0; n < sp->imageHeight; ++n)
            {
                *p++ = ReadShort(fp);
            }
        }
        else
        {
            puts("Unable to allocate Scan Line Table");
            return (-1);
        }
    }
    else
    {
        printf("Error seeking to Scan Line Table, offset = 0x%08x\n", sp->scanLineOffset);
        return (-1);
    }
    return (0);
}

/*
    int             n;              buffer size in bytes
    int             bpp;            bytes per pixel
 */
int ReadRLERow(unsigned char *p, int n, int bpp, FILE *fp)
{
    unsigned int value;
    int i;
    unsigned char *q;

    while (n > 0)
    {
        value = (unsigned int) ReadByte(fp);
        if (value & 0x80)
        {
            value &= 0x7f;
            value++;
            n -= value * bpp;
            if (n < 0)
                return (-1);
            if (fread(rleBuf, 1, bpp, fp) != bpp)
                return (-1);
            while (value > 0)
            {
                *p++ = rleBuf[0];
                if (bpp > 1)
                    *p++ = rleBuf[1];
                if (bpp > 2)
                    *p++ = rleBuf[2];
                if (bpp > 3)
                    *p++ = rleBuf[3];
                value--;
            }
        }
        else
        {
            value++;
            n -= value * bpp;
            if (n < 0)
                return (-1);
            /*
            ** Maximum for value is 128 so as long as RLEBUFSIZ
            ** is at least 512, and bpp is not greater than 4
            ** we can read in the entire raw packet with one operation.
            */
            if (fread(rleBuf, bpp, value, fp) != value)
                return (-1);
            for (i = 0, q = rleBuf; i < (value * bpp); ++i)
                *p++ = *q++;
        }
    }
    return (0);
}

int ReadExtendedTGA(FILE *fp, TGAFile *sp)
{
    if (!fseek(fp, sp->extAreaOffset, SEEK_SET))
    {
        sp->extSize = ReadShort(fp);
        memset(sp->author, 0, 41);
        ReadCharField(fp, sp->author, 41);
        memset(&sp->authorCom[0][0], 0, 81);
        ReadCharField(fp, &sp->authorCom[0][0], 81);
        memset(&sp->authorCom[1][0], 0, 81);
        ReadCharField(fp, &sp->authorCom[1][0], 81);
        memset(&sp->authorCom[2][0], 0, 81);
        ReadCharField(fp, &sp->authorCom[2][0], 81);
        memset(&sp->authorCom[3][0], 0, 81);
        ReadCharField(fp, &sp->authorCom[3][0], 81);

        sp->month = ReadShort(fp);
        sp->day = ReadShort(fp);
        sp->year = ReadShort(fp);
        sp->hour = ReadShort(fp);
        sp->minute = ReadShort(fp);
        sp->second = ReadShort(fp);

        memset(sp->jobID, 0, 41);
        ReadCharField(fp, sp->jobID, 41);
        sp->jobHours = ReadShort(fp);
        sp->jobMinutes = ReadShort(fp);
        sp->jobSeconds = ReadShort(fp);

        memset(sp->softID, 0, 41);
        ReadCharField(fp, sp->softID, 41);
        sp->versionNum = ReadShort(fp);
        sp->versionLet = ReadByte(fp);

        sp->keyColor = ReadLong(fp);
        sp->pixNumerator = ReadShort(fp);
        sp->pixDenominator = ReadShort(fp);

        sp->gammaNumerator = ReadShort(fp);
        sp->gammaDenominator = ReadShort(fp);

        sp->colorCorrectOffset = ReadLong(fp);
        sp->stampOffset = ReadLong(fp);
        sp->scanLineOffset = ReadLong(fp);

        sp->alphaAttribute = ReadByte(fp);

        sp->colorCorrectTable = (UINT16 *) 0;
        if (sp->colorCorrectOffset)
        {
            ReadColorTable(fp, sp);
        }

        sp->postStamp = (void *) 0;
        if (sp->stampOffset)
        {
            if (!fseek(fp, sp->stampOffset, SEEK_SET))
            {
                sp->stampWidth = ReadByte(fp);
                sp->stampHeight = ReadByte(fp);
            }
            else
            {
                printf("Error seeking to Postage Stamp, offset = 0x%08x\n", sp->stampOffset);
            }
        }

        sp->scanLineTable = (UINT32 *) 0;
        if (sp->scanLineOffset)
        {
            ReadScanLineTable(fp, sp);
        }
    }
    else
    {
        printf("Error seeking to Extended TGA Area, offset = 0x%08x\n", sp->extAreaOffset);
        return (-1);
    }
    return (0);
}

int ReadDeveloperDirectory(FILE *fp, TGAFile *sp)
{
    int i;

    if (!fseek(fp, sp->devDirOffset, SEEK_SET))
    {
        sp->devTags = ReadShort(fp);
        if ((sp->devDirs = malloc(sp->devTags * sizeof(DevDir))) == NULL)
        {
            puts("Unable to allocate developer directory.");
            return (-1);
        }
        for (i = 0; i < sp->devTags; ++i)
        {
            sp->devDirs[i].tagValue = ReadShort(fp);
            sp->devDirs[i].tagOffset = ReadLong(fp);
            sp->devDirs[i].tagSize = ReadLong(fp);
        }
    }
    else
    {
        printf("Error seeking to Developer Area at offset 0x%08x\n", sp->devDirOffset);
        return (-1);
    }
    return (0);
}

long CountRLEData(FILE *fp, unsigned int x, unsigned int y, int bytesPerPixel)
{
    long n;
    long pixelCount;
    long totalPixels;
    unsigned int value;

    n = 0L;
    pixelCount = 0L;
    totalPixels = (long) x * (long) y;

    while (pixelCount < totalPixels)
    {
        value = (unsigned int) ReadByte(fp);
        n++;
        if (value & 0x80)
        {
            n += bytesPerPixel;
            pixelCount += (value & 0x7f) + 1;
            if (fread(copyBuf, 1, bytesPerPixel, fp) != bytesPerPixel)
            {
                puts("Error counting RLE data.");
                return (0L);
            }
        }
        else
        {
            value++;
            n += value * bytesPerPixel;
            pixelCount += value;
            if (fread(copyBuf, bytesPerPixel, value, fp) != value)
            {
                puts("Error counting raw data.");
                return (0L);
            }
        }
    }
    return (n);
}
