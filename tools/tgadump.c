/*
** Copyright (c) 1989, 1990, 1992
** Truevision, Inc.
** All Rights Reserverd
**
** TGADUMP reads the contents of a Truevision TGA(tm) File and displays
** information about the file contents on the console device.  This
** program does not display the image.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <tga.h>

extern int              main( int, char ** );
extern void             PrintColorTable( TGAFile * );
extern void             PrintExtendedTGA( TGAFile * );
extern void             PrintImageType( int );
extern void             PrintMonth( UINT16 );
extern void             PrintScanLineTable( TGAFile * );
extern void             PrintTGAInfo( TGAFile * );


/*
** String data for performing month conversions
*/
const char      *monthStr[] =
{
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
};

/*
** String data for interpretting image orientation specification
*/
const char      *orientStr[] =
{
        "Bottom Left",
        "Bottom Right",
        "Top Left",
        "Top Right"
};

/*
** String data for interpretting interleave flag defined with VDA
** This field is now obsolete and should typically be set to zero.
*/
const char      *interleaveStr[] =
{
        "  Two Way (Even-Odd) Interleave (e.g., IBM Graphics Card Adapter), Obsolete",
        "  Four Way Interleave (e.g., AT&T 6300 High Resolution), Obsolete",
};

TGAFile         f;                              /* control structure of image data */
const char              *versionStr =
"Truevision(R) TGA(tm) File Dump Utility Version 1.4 - February 15, 1992";

int main(int argc, char **argv)
{
        int                     fileFound;
        char            *q;
        FILE            *fp;
        long            fsize;
        int                     xTGA;                   /* flags extended TGA file */
        UINT16          devDirEntries;  /* number of entries in developer area */
        char            fileName[80];
        struct stat     statbuf;

        puts( versionStr );
        /*
        ** The program can be invoked without an argument, in which case
        ** the user will be prompted for the name of the image file to be
        ** examined, or the image file name can be provided as an argument
        ** to the command.
        **
        ** File names provided do not need to include the extension if
        ** the image file extension is one of the standard strings common
        ** to Truevision TGA image file names ( e.g., TGA, WIN, VST, VDA, ICB )
        */
        if ( argc == 1 )
        {
                printf( "Enter name of file to examine: " );
                fgets( fileName, sizeof( fileName ), stdin );
                if ( strlen( fileName ) == 0 ) exit( 0 );
        }
        else
        {
                strcpy( fileName, argv[1] );
        }
        if ( fileName[0] == '-' )
        {
                puts( "Usage: tgadump [filename]" );
                exit( 0 );
        }
        /*
        ** See if we can find the file as specified or with one of the
        ** standard filename extensions...
        */
        fileFound = 0;
        if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
        else
        {
                strcat( fileName, ".tga" );
                q = strchr( fileName, '.' );
                if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                else
                {
                        strcpy( q, ".vst" );
                        if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                        else
                        {
                                strcpy( q, ".vda" );
                                if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                                else
                                {
                                        strcpy( q, ".icb" );
                                        if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                                        else
                                        {
                                                strcpy( q, ".win" );
                                                if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                                                else
                                                {
                                                        *q = '\0';
                                                        printf("Unable to open image file %s\n", fileName );
                                                }
                                        }
                                }
                        }
                }
        }
        if ( fileFound )
        {
                int readStatus;
                printf( "TGA File: %s\n", fileName );
                fp = fopen( fileName, "rb" );
                readStatus = ReadTGAFile( fp, &f );
                if ( readStatus >= 0 )
                {
                        PrintTGAInfo( &f );
                        if ( f.extAreaOffset )
                        {
                            PrintExtendedTGA(&f);
                        }
                        if ( f.devDirOffset )
                        {
                                puts( "Developer Area Specified:" );
                                {
                                        devDirEntries = f.devTags;
                                        printf( "Developer Directory contains %d Entries\n",
                                                devDirEntries );
                                }
                        }
                }
                else
                {
                        switch ( readStatus )
                        {
                        case TGA_READ_ERROR_READ_ID:
                                puts( "Couldn't read id." );
                                break;

                        case TGA_READ_ERROR_SEEK_END:
                                puts( "Couldn't seek to end of file." );
                                break;

                        case TGA_READ_ERROR_READ_SIGNATURE:
                                puts( "Couldn't read TGA signature." );
                                break;

                        case TGA_READ_ERROR_BAD_FILE_SIZE:
                                puts( "Bad image file size." );
                                break;

                        case TGA_READ_ERROR_READ_EXTENDED:
                                puts( "Couldn't read extended TGA information." );
                                break;

                        case TGA_READ_ERROR_READ_DEVELOPER_DIRECTORY:
                                puts( "Couldn't read developer directory." );
                                break;
                        }
                }
                FreeTGAFile( &f );
                fclose( fp );
        }
        return 0;
}


void PrintColorTable(TGAFile *sp)
{
        unsigned int    n;
        UINT16                  *p;

        puts( "Color Correction Table:" );
        p = sp->colorCorrectTable;
        for ( n = 0; n < 256; ++n )
        {
                printf( "Color Entry %3u: 0x%04x(%5u) A, ", n, *p, *p );
                ++p;
                printf( "0x%04x(%5u) R, ", *p, *p );
                ++p;
                printf( "0x%04x(%5u) G, ", *p, *p );
                ++p;
                printf( "0x%04x(%5u) B\n", *p, *p );
                ++p;
        }
}



void PrintExtendedTGA(TGAFile *sp) /* TGA structure pointer */
{
        register int    strSize;
        char                    *blankChars = " \t";

        puts( "***** Extended TGA Fields *****" );
        printf( "Truevision TGA File Format Version = " );
        if ( sp->extSize == 495 ) puts( "2.0" );
        else printf( "UNKNOWN, extension size = %d\n", sp->extSize );

        /*
        ** Make sure the strings have length, and contain something
        ** other than blanks and tabs
        */
        strSize = strlen( sp->author );
        if ( strSize && strspn( sp->author, blankChars ) < strSize )
        {
                printf( "Author                             = %s\n", sp->author );
        }
        strSize = strlen( &sp->authorCom[0][0] );
        if ( strSize && strspn( &sp->authorCom[0][0], blankChars ) < strSize )
        {
                puts( "Author Comments:" );
                puts( &sp->authorCom[0][0] );
                strSize = strlen( &sp->authorCom[1][0] );
                if ( strSize && strspn( &sp->authorCom[1][0], blankChars ) < strSize )
                {
                        puts( &sp->authorCom[1][0] );
                }
                strSize = strlen( &sp->authorCom[2][0] );
                if ( strSize && strspn( &sp->authorCom[2][0], blankChars ) < strSize )
                {
                        puts( &sp->authorCom[2][0] );
                }
                strSize = strlen( &sp->authorCom[3][0] );
                if ( strSize && strspn( &sp->authorCom[3][0], blankChars ) < strSize )
                {
                        puts( &sp->authorCom[3][0] );
                }
                puts( "[End of Author Comments]" );
        }

        if ( sp->month )
        {
                printf( "Date Image Saved                   = " );
                PrintMonth( sp->month );
                printf( " %02u, %4u at %02u:%02u:%02u\n", sp->day, sp->year,
                        sp->hour, sp->minute, sp->second );
        }

        strSize = strlen( sp->jobID );
        if ( strSize && strspn( sp->jobID, blankChars ) < strSize )
        {
                printf( "Job Name/ID                        = %s\n", sp->jobID );
        }

        if ( sp->jobHours != 0 || sp->jobMinutes != 0 || sp->jobSeconds != 0 )
        {
                printf( "Job Elapsed Time                   = %02u:%02u:%02u\n",
                        sp->jobHours, sp->jobMinutes, sp->jobSeconds );
        }

        strSize = strlen( sp->softID );
        if ( strSize && strspn( sp->softID, blankChars ) < strSize )
        {
                printf( "Software ID                        = %s\n", sp->softID );
        }

        if ( sp->versionNum != 0 || sp->versionLet != ' ' )
        {
                printf( "Software Version                   = %d.%d%c\n",
                        sp->versionNum/100, sp->versionNum % 100, sp->versionLet );
        }

        printf( "Key Color: 0x%02x(%u) Alpha, 0x%02x(%u) Red, 0x%02x(%u) Green, 0x%02x(%u) Blue\n",
                sp->keyColor >> 24, sp->keyColor >> 24,
                (sp->keyColor >> 16) & 0xff, (sp->keyColor >> 16) & 0xff,
                (sp->keyColor >> 8) & 0xff, (sp->keyColor >> 8) & 0xff,
                sp->keyColor & 0xff, sp->keyColor & 0xff );

        if ( sp->pixNumerator != 0 && sp->pixDenominator != 0 )
        {
                printf( "Pixel Aspect Ratio                 = %f\n",
                                (double)sp->pixNumerator / (double)sp->pixDenominator );
        }

        if ( sp->gammaDenominator != 0 )
        {
                printf( "Gamma Correction                   = %f\n",
                                (double)sp->gammaNumerator / (double)sp->gammaDenominator );
        }

        if ( sp->colorCorrectOffset != 0L )
        {
                printf( "Color Correction Offset            = 0x%08x\n",
                                        sp->colorCorrectOffset);
                if ( sp->colorCorrectTable )
                {
                        PrintColorTable( sp );
                }
        }
        if ( sp->stampOffset )
        {
                printf( "Postage Stamp Offset               = 0x%08x\n",
                                        sp->stampOffset );
                printf( "Postage Stamp Width, Height        = %3u, %3u\n",
                                        sp->stampWidth, sp->stampHeight );
        }
        if ( sp->scanLineOffset != 0L )
        {
                printf( "Scan Line Offset                   = 0x%08x\n",
                                        sp->scanLineOffset );
                if ( sp->scanLineTable )
                {
                        PrintScanLineTable( sp );
                }
        }

        switch (sp->alphaAttribute )
        {
        case 0:
                if ( (sp->imageDesc & 0xf) == 0 ) puts( "No Alpha Data Present" );
                else puts( "Inconsistent Alpha Data Specification" );
                break;
        case 1:
                puts( "Alpha Data Undefined and Can Be Ignored" );
                break;
        case 2:
                puts( "Alpha Data Undefined but Should Be Retained" );
                break;
        case 3:
                puts( "Useful Alpha Data Present" );
                break;
        case 4:
                puts( "Pre-Multiplied Alpha Data Present" );
                break;
        default:
                puts( "Undefined Alpha Attribute Field" );
                break;
        }
}



void PrintImageType(int Itype)
{
        if ( Itype > 255 || Itype < 0 )
        {
                puts("Illegal/Undefined Image Type");
                return;
        }

        if ( Itype > 127 )
        {
                puts("Unknown Image Type - Application Specific");
                return;
        }

        switch (Itype)
        {
        case 0:
                puts("Unknown Image Type - No Image Data Present");
                break;
        case 1:
                puts("Uncompressed Color Mapped Image (e.g., VDA/D, TARGA M8)");
                break;
        case 2:
                puts("Uncompressed True Color Image (e.g., ICB, TARGA 16/24/32)");
                break;
        case 3:
                puts("Uncompressed Black & White Image (e.g., TARGA 8/M8)");
                break;
        case 9:
                puts("Run Length Encoded Color Mapped Image (e.g., VDA/D, TARGA M8)");
                break;
        case 10:
                puts("Run Length Encoded True Color Image (e.g., ICB, TARGA 16/24/32)");
                break;
        case 11:
                puts("Compressed Black & White Image (e.g., TARGA 8/M8)");
                break;
        case 32:
        case 34:
                puts("Compressed (Huffman/Delta/RLE) Color Mapped Image (e.g., VDA/D) - Obsolete");
                break;
        case 33:
        case 35:
                puts("Compressed (Huffman/Delta/RLE) Color Mapped Four Pass Image (e.g., VDA/D) - Obsolete");
                break;
        default:
                puts("Unknown Image Type");
                break;
        }
}


void PrintMonth(UINT16 month)
{
        if ( month > 0 && month < 13 ) printf( "%s", monthStr[month - 1] );
        else printf( "Month Error" );
}


void PrintScanLineTable(TGAFile *sp)
{
        UINT16  n;
        UINT32  *p;

        puts( "Scan Line Table:" );
        p = sp->scanLineTable;
        for ( n = 0; n < sp->imageHeight; ++n )
        {
                printf( "Scan Line %6u, Offset 0x%08x(%8u)\n", n, *p, *p );
                ++p;
        }
}



void PrintTGAInfo(TGAFile *sp)
{
        int     i;

        printf("ID Field Length          = %3d\n", sp->idLength);

        printf("Color Map Type           = %3d  (Color Map Data is ", sp->mapType);
        if (sp->mapType) puts("Present)");
        else puts("Absent)");  

        printf("Image Type               = %3d\n  ", sp->imageType);
        PrintImageType( sp->imageType );

        printf("Color Map Origin         = 0x%04x (%5d)",
                sp->mapOrigin, sp->mapOrigin);
        puts( "  (First Index To Be Loaded)" );
        printf("Color Map Length         = 0x%04x (%5d)\n",
                sp->mapLength,sp->mapLength);
        printf("Color Map Entry Size     = %6d\n", sp->mapWidth);

        printf("Image X-Origin, Y-Origin =  %05d, %05d\n",
                sp->xOrigin, sp->yOrigin);
        printf("Image Width, Height      =  %05d, %05d\n",
                        sp->imageWidth, sp->imageHeight);

        printf("Image Pixel Depth        = 0x%04x (%5d)\n",
                sp->pixelDepth, sp->pixelDepth);
        printf("Image Descriptor         = 0x%04x\n", sp->imageDesc);
        printf("  %d Attribute Bits Per Pixel\n", sp->imageDesc & 0xf );
        printf("  First Pixel Destination is ");

        i = (sp->imageDesc & 0x30) >> 4;
        puts( orientStr[i] );

        i = (sp->imageDesc & 0xc0) >> 6;
        if ( i > 0 && i < 3 ) puts( interleaveStr[i - 1] );

        if ( sp->idLength )
        {
                printf( "Image ID:\n  " );
                puts( f.idString );
        }
}
