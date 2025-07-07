/*
** Copyright (c) 1989, 1990
** Truevision, Inc.
** All Rights Reserverd
**
** TGAPACK reads the contents of a Truevision(R) TGA(tm) File and provides
** the ability to compress the image data via run length encoding, or
** to uncompress images that have been stored in a run length encoded
** format.  The program only operates on files stored in the original
** TGA format.
**
** USAGE:
**              tgapack [options] [file1] [file2] ...
**
** If no filenames are provided, the program will prompt for a file
** name.  If no extension is provided with a filename, the program
** will search for the file with ".tga", ".vst", ".icb", ".vda", or
** ".win" extension.  Options are specified by a leading '-'.
** If no options are provided, the program attempts to compress an
** uncompressed image.
**
** Recognized options are:
**
**              -unpack                 uncompressed a run length encoded image
**              -32to24                 compress a 32 bit image by eliminating alpha data
**              -version                report version number of program
*/

#include <config/string_case_compare.h>

#include <tga.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
** Define byte counts associated with extension areas for various
** versions of the TGA specification.
*/
#define EXT_SIZE_20     495                     /* verison 2.0 extension size */

#define CBUFSIZE        2048            /* size of copy buffer */
#define RLEBUFSIZ       512                     /* size of largest possible RLE packet */


extern int      main( int, char ** );
extern int      DisplayImageData( unsigned char *, int, int );
extern int      OutputTGAFile( FILE *, FILE *, TGAFile * );
extern int      ParseArgs( int, char ** );
extern void     PrintImageType( int );
extern void     PrintTGAInfo( TGAFile * );
extern char     *SkipBlank( char * );
extern void     StripAlpha( unsigned char *, int );


/*
** String data for interpretting image orientation specification
*/
char    *orientStr[] =
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
char    *interleaveStr[] =
{
        "  Two Way (Even-Odd) Interleave (e.g., IBM Graphics Card Adapter), Obsolete",
        "  Four Way Interleave (e.g., AT&T 6300 High Resolution), Obsolete",
};


/*
** Filename extensions used during file search
*/
char    *extNames[] =
{
        ".tga",
        ".vst",
        ".icb",
        ".vda",
        ".win",
        NULL
};

TGAFile                 f;                              /* control structure of image data */

int                             unPack;                 /* when true, uncompress image data */
int                             noAlpha;                /* when true, converts 32 bit image to 24 */

int                             inRawPacket;    /* flags processing state for RLE data */
int                             inRLEPacket;    /* flags processing state for RLE data */
unsigned int    packetSize;     /* records current RLE packet size in bytes */
char                    rleBuf[RLEBUFSIZ];

char            copyBuf[CBUFSIZE];


char            *versionStr =
"Truevision(R) TGA(tm) File Compression Utility Version 1.3 - January 2, 1990";


int main(int argc, char **argv)
{
        int                     fileFound;
        int                     fileCount;
        int                     files;
        char            *q;
        FILE            *fp, *outFile;
        int                     i;
        char            fileName[80];
        char            outFileName[80];
        struct stat     statbuf;

        unPack = 0;                     /* default to compressing image data */
        noAlpha = 0;            /* default to retaining all components of 32 bit */

        inRawPacket = inRLEPacket = 0;  /* initialize RLE processing flags */
        packetSize = 0;
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
                puts( versionStr );
                printf( "Enter name of file to examine: " );
                fgets( fileName, sizeof( fileName ), stdin );
                if ( strlen( fileName ) == 0 ) exit( 0 );
                fileCount = 1;
        }
        else
        {
                fileCount = ParseArgs( argc, argv );
                if ( fileCount == 0 ) exit( 0 );
                argv++;
                while ( **argv == '-' ) argv++; 
                strcpy( fileName, *argv );
        }
        for ( files = 0; files < fileCount; ++files )
        {
                if ( files != 0 )
                {
                        argv++;
                        while ( **argv == '-' ) argv++; 
                        strcpy( fileName, *argv );
                }
                /*
                ** See if we can find the file as specified or with one of the
                ** standard filename extensions...
                */
                fileFound = 0;
                if ( stat( fileName, &statbuf ) == 0 ) fileFound = 1;
                else
                {
                        /*
                        ** If there is already an extension specified, skip
                        ** the search for standard extensions
                        */
                        q = strchr( fileName, '.' );
                        if ( q != NULL )
                        {
                                q = fileName + strlen( fileName );
                        }
                        else
                        {
                                i = 0;
                                strcat( fileName, extNames[i] );
                                q = strchr( fileName, '.' );
                                while ( extNames[i] != NULL )
                                {
                                        strcpy( q, extNames[i] );
                                        if ( stat( fileName, &statbuf ) == 0 )
                                        {
                                                fileFound = 1;
                                                break;
                                        }
                                        ++i;
                                }
                        }
                }
                if ( fileFound )
                {
                        int readStatus;
                        printf( "Processing TGA File: %s\n", fileName );
                        fp = fopen( fileName, "rb" );
                        readStatus = ReadTGAFile( fp, &f );
                        if ( readStatus >= 0 )
                        {
                                if ( f.extAreaOffset != 0L )
                                {
                                        /*
                                        ** Reset offset values since this is not a new TGA file
                                        */
                                        f.extAreaOffset = 0L;
                                        f.devDirOffset = 0L;
                                        strcpy( outFileName, fileName );
                                        i = strlen( fileName );
                                        outFileName[ i - 3 ] = '\0';    /* remove extension */
                                        strcat( outFileName, "$$$" );
                                        outFile = fopen( outFileName, "wb" );
                                        if ( outFile != NULL )
                                        {
                                                if ( OutputTGAFile( fp, outFile, &f ) < 0 )
                                                {
                                                        fclose( outFile );
                                                        remove( outFileName );
                                                }
                                                else
                                                {
                                                        fclose( outFile );
                                                        fclose( fp );
                                                        fp = (FILE *)0;
                                                        remove( fileName );
                                                        rename( outFileName, fileName );
                                                }
                                        }
                                        else
                                        {
                                                puts( "Unable to create output file." );
                                        }
                                }
                                else puts( "Input file must be original TGA format." );
                        }
                        else
                        {
                                puts( "Error reading input file." );
                        }
                        if ( fp != NULL ) fclose( fp );
                }
                else
                {
                        *q = '\0';
                        printf("Unable to open image file %s\n", fileName );
                }
        }
        return 0;
}




int DisplayImageData(unsigned char *q, int n, int bpp)
{
    long i;
    int j;
    unsigned char a, b, c;

    i = 0;
    while (i < n)
    {
        printf("%08lX: ", i);
        switch (bpp)
        {
        case 4:
            for (j = 0; j < 4; ++j)
            {
                printf("%08lx ", *(unsigned long *) q);
                q += 4;
            }
            i += 16;
            break;
        case 3:
            for (j = 0; j < 8; ++j)
            {
                a = *q++;
                b = *q++;
                c = *q++;
                printf("%02x%02x%02x ", c, b, a);
            }
            i += 24;
            break;
        case 2:
            for (j = 0; j < 8; ++j)
            {
                printf("%04x ", *(unsigned int *) q);
                q += 2;
            }
            i += 16;
            break;
        default:
            for (j = 0; j < 16; ++j)
            {
                printf("%02x ", *(unsigned char *) q++);
            }
            i += 16;
            break;
        }
        putchar('\n');
    }
    return (0);
}

int OutputTGAFile(FILE *ifp, /* input file pointer */
    FILE *ofp,               /* output file pointer */
    TGAFile *sp)             /* output TGA structure */
{
        long            byteCount;
        int             i;
        int             bytesPerPixel;
        int             bCount;
        int             rleCount;
        unsigned char   *imageBuff;
        unsigned char *packBuff;

        /*
        ** First, we need to determine what operation is to be performed.
        ** We could be run length encoding an uncompressed image, or
        ** we could be uncompressing an RLE image.  Or we could be compacting
        ** a 32 bit image to a 24 bit image.
        */
        if ( noAlpha )
        {
            if ( sp->pixelDepth != 32 || sp->imageType != 2 )
            {
                puts( "Image file must be in 32 bit uncompressed format." );
                return -1;
            }
            sp->pixelDepth = 24;
            sp->imageDesc &= 0xf0;
        }
        else if ( !unPack && sp->imageType > 0 && sp->imageType < 4 )
        {
            sp->imageType += 8;
        }
        else if ( unPack && sp->imageType > 8 && sp->imageType < 12 )
        {
            sp->imageType -= 8;
        }
        else
        {
            puts( "File type is inconsistent with requested operation." );
            return -1;
        }
        if (WriteTGAFile(ofp, sp))
        {
            return -1;
        }

        if ( CopyTGAColormap(sp, ifp, ofp) < 0 ) return -1;

        /*
        ** Now process the image data.
        */
        bytesPerPixel = (sp->pixelDepth + 7) >> 3;
        bCount = sp->imageWidth * bytesPerPixel;
        imageBuff = malloc( bCount );
        if ( imageBuff == NULL )
        {
                puts( "Unable to allocate image buffer" );
                return( -1 );
        }

        if ( noAlpha )
        {
                for ( i = 0; i < sp->imageHeight; ++i )
                {
                        if ( fread( imageBuff, 1, bCount, ifp ) != bCount )
                        {
                                puts( "Error reading uncompressed data." );
                                free( imageBuff );
                                return( -1 );
                        }
                        StripAlpha( imageBuff, bCount );
                        if ( fwrite( imageBuff, 1, bCount - sp->imageWidth, ofp ) !=
                                        bCount - sp->imageWidth )
                        {
                                puts( "Error writing 24 bit image data." );
                                free( imageBuff );
                                return( -1 );
                        }
                }
        }
        else if ( !unPack )
        {
                packBuff = malloc( sp->imageWidth * (bytesPerPixel + 1) );
                if ( packBuff == NULL )
                {
                        puts( "Error allocating encoded buffer." );
                        free( imageBuff );
                        return( -1 );
                }
                for ( i = 0; i < sp->imageHeight; ++i )
                {
                        if ( fread( imageBuff, 1, bCount, ifp ) != bCount )
                        {
                                puts( "Error reading uncompressed data." );
                                free( imageBuff );
                                free( packBuff );
                                return( -1 );
                        }
                        rleCount = RLEncodeRow( imageBuff, packBuff, sp->imageWidth,
                                                        bytesPerPixel );
                        if ( fwrite( packBuff, 1, rleCount, ofp ) != rleCount )
                        {
                                puts( "Error writing RLE image data." );
                                free( imageBuff );
                                free( packBuff );
                                return( -1 );
                        }
                }
                free( packBuff );
        }
        else
        {
                /*
                ** Uncompress image data
                */
                for ( i = 0; i < sp->imageHeight; ++i )
                {
                        if ( ReadRLERow( imageBuff, bCount, bytesPerPixel, ifp ) < 0 )
                        {
                                puts("Error reading RLE data." );
                                free( imageBuff );
                                return( -1 );
                        }
                        if ( fwrite( imageBuff, 1, bCount, ofp ) != bCount )
                        {
                                puts("Error writing uncompressed data." );
                                free( imageBuff );
                                return( -1 );
                        }
                }
        }
        free( imageBuff );

        return( 0 );
}



int ParseArgs(int argc, char **argv)
{
        int             i;
        int             n;
        char    *p;

        n = 0;
        for ( i = 1; i < argc; ++i )
        {
                p = *(++argv);
                if ( *p == '-' )
                {
                        p++;
                        if ( string_case_compare( p, "unpack" ) == 0 ) unPack = 1;
                        else if ( string_case_compare( p, "32to24" ) == 0 ) noAlpha = 1;
                        else if ( string_case_compare( p, "version" ) == 0 )
                        {
                                puts( versionStr );
                                exit( 0 );
                        }
                        else
                        {
                                puts( "Usage: tgapack [options] [file1] [file2...]" );
                                puts( "  where options can be:" );
                                puts( "    -unpack\t\tuncompress image data" );
                                puts( "    -32to24\t\tconvert 32 bit image to 24 bit image" );
                                puts( "    -version\t\treport version number" );
                                exit( 0 );
                        }
                }
                else ++n;
        }
        return( n );
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




void PrintTGAInfo(TGAFile *sp) /* TGA structure pointer */
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

        printf("Image Pixel Depth        = 0x%04x (%05d)\n",
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



char *SkipBlank(char *p)
{
        while ( *p != '\0' && (*p == ' ' || *p == '\t') ) ++p;
        return( p );
}



void StripAlpha(unsigned char *s, int n)
{
        int                             i;
        unsigned char   *p;

        /*
        ** Copy the RGB components omitting the alpha.  Perform the
        ** copy in its own buffer.  This algorithm is probably
        ** specific to the 80x86 since byte orderring will be different
        ** on a 680x0 processor.
        */
        p = s;
        for ( i = 0; i < n; ++i )
        {
                if ( ( i % 4) == 3 ) ++p;
                else *s++ = *p++;
        }
}
