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


extern void     main( int, char ** );
extern int      CountDiffPixels( char *, int, int );
extern long     CountRLEData( FILE *, unsigned int, unsigned int, int );
extern int      CountSamePixels( char *, int, int );
extern int      DisplayImageData( unsigned char *, int, int );
extern UINT32   GetPixel( unsigned char *, int );
extern int      OutputTGAFile( FILE *, FILE *, TGAFile * );
extern int      ParseArgs( int, char ** );
extern void     PrintImageType( int );
extern void     PrintTGAInfo( TGAFile * );
extern UINT8    ReadByte( FILE * );
extern void     ReadCharField( FILE *, char *, int );
extern UINT32   ReadLong( FILE * );
extern int      ReadRLERow( unsigned char *, int, int, FILE * );
extern UINT16   ReadShort( FILE * );
extern int      RLEncodeRow( char *, char *, int, int );
extern char     *SkipBlank( char * );
extern void     StripAlpha( unsigned char *, int );
extern int      WriteByte( UINT8, FILE * );
extern int      WriteLong( UINT32, FILE * );
extern int      WriteShort( UINT16, FILE * );
extern int      WriteStr( char *, int, FILE * );


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


void main(int argc, char **argv)
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
                gets( fileName );
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
                        printf( "Processing TGA File: %s\n", fileName );
                        fp = fopen( fileName, "rb" );
                        /*
                        ** It would be nice to be able to read in the entire
                        ** structure with one fread, but compiler dependent
                        ** structure alignment precludes the simplistic approach.
                        ** Instead, fill each field individually, and use routines
                        ** that will allow code to execute on various hosts by
                        ** recompilation with particular compiler flags.
                        **
                        ** Start by reading the fields associated with the original
                        ** TGA format.
                        */
                        f.idLength = ReadByte( fp );
                        f.mapType = ReadByte( fp );
                        f.imageType = ReadByte( fp );
                        f.mapOrigin = ReadShort( fp );
                        f.mapLength = ReadShort( fp );
                        f.mapWidth = ReadByte( fp );
                        f.xOrigin = ReadShort( fp );
                        f.yOrigin = ReadShort( fp );
                        f.imageWidth = ReadShort( fp );
                        f.imageHeight = ReadShort( fp );
                        f.pixelDepth = ReadByte( fp );
                        f.imageDesc = ReadByte( fp );
                        memset( f.idString, 0, 256 );
                        if ( f.idLength > 0 )
                        {
                                fread( f.idString, 1, f.idLength, fp );
                        }
                        /*
                        ** Now see if the file is the new (extended) TGA format.
                        */
                        if ( !fseek( fp, statbuf.st_size - 26, SEEK_SET ) )
                        {
                                f.extAreaOffset = ReadLong( fp );
                                f.devDirOffset = ReadLong( fp );
                                fgets( f.signature, 18, fp );
                                if ( strcmp( f.signature, "TRUEVISION-XFILE." ) )
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
                                        if ( ( outFile = fopen( outFileName, "wb" ) ) != NULL )
                                        {
                                                if ( OutputTGAFile( fp, outFile, &f ) < 0 )
                                                {
                                                        fclose( outFile );
                                                        unlink( outFileName );
                                                }
                                                else
                                                {
                                                        fclose( outFile );
                                                        fclose( fp );
                                                        fp = (FILE *)0;
                                                        unlink( fileName );
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
                                puts( "Error seeking to end of file for possible extension data." );
                        }
                        if ( fp != NULL ) fclose( fp );
                }
                else
                {
                        *q = '\0';
                        printf("Unable to open image file %s\n", fileName );
                }
        }
}



/*
** Count pixels in buffer until two identical adjacent ones found
*/

int CountDiffPixels(char *p, int bpp, int pixCnt)
{
        unsigned long   pixel;
        unsigned long   nextPixel;
        int                             n;

        n = 0;
        if ( pixCnt == 1 ) return( pixCnt );
        pixel = GetPixel( p, bpp );
        while ( pixCnt > 1 )
        {
                p += bpp;
                nextPixel = GetPixel( p, bpp );
                if ( nextPixel == pixel ) break;
                pixel = nextPixel;
                ++n;
                --pixCnt;
        }
        if ( nextPixel == pixel ) return( n );
        return( n + 1 );
}



long CountRLEData(FILE *fp, unsigned int x, unsigned int y, int bytesPerPixel)
{
        long                    n;
        long                    pixelCount;
        long                    totalPixels;
        unsigned int    value;

        n = 0L;
        pixelCount = 0L;
        totalPixels = (long)x * (long)y;

        while ( pixelCount < totalPixels )
        {
                value = (unsigned int)ReadByte( fp );
                n++;
                if ( value & 0x80 )
                {
                        n += bytesPerPixel;
                        pixelCount += (value & 0x7f) + 1;
                        if ( fread( copyBuf, 1, bytesPerPixel, fp ) != bytesPerPixel )
                        {
                                puts( "Error counting RLE data." );
                                return( 0L );
                        }
                }
                else
                {
                        value++;
                        n += value * bytesPerPixel;
                        pixelCount += value;
                        if ( fread( copyBuf, bytesPerPixel, value, fp ) != value )
                        {
                                puts( "Error counting raw data." );
                                return( 0L );
                        }
                }
        }
        return( n );
}



int CountSamePixels(char *p, int bpp, int pixCnt)
{
        unsigned long   pixel;
        unsigned long   nextPixel;
        int                             n;

        n = 1;
        pixel = GetPixel( p, bpp );
        pixCnt--;
        while ( pixCnt > 0 )
        {
                p += bpp;
                nextPixel = GetPixel( p, bpp );
                if ( nextPixel != pixel ) break;
                ++n;
                --pixCnt;
        }
        return( n );
}




int DisplayImageData(unsigned char *q, int n, int bpp)
{
        long i;
        int j;
        unsigned char a, b, c;

        i = 0;
        while ( i < n ) 
        {
                printf( "%08lX: ", i );
                switch ( bpp )
                {
                case 4:
                        for ( j = 0; j < 4; ++j )
                        {
                                printf( "%08lx ", *(unsigned long *)q );
                                q += 4;
                        }
                        i += 16;
                        break;
                case 3:
                        for ( j = 0; j < 8; ++j )
                        {
                                a = *q++;
                                b = *q++;
                                c = *q++;
                                printf( "%02x%02x%02x ", c, b, a );
                        }
                        i += 24;
                        break;
                case 2:
                        for ( j = 0; j < 8; ++j )
                        {
                                printf( "%04x ", *(unsigned int *)q );
                                q += 2;
                        }
                        i += 16;
                        break;
                default:
                        for ( j = 0; j < 16; ++j )
                        {
                                printf( "%02x ", *(unsigned char *)q++ );
                        }
                        i += 16;
                        break;
                }
                putchar( '\n' );
        }
        return( 0 );
}



/*
** Retrieve a pixel value from a buffer.  The actual size and order
** of the bytes is not important since we are only using the value
** for comparisons with other pixels.
*/

UINT32 GetPixel(unsigned char *p, int bpp) /* bytes per pixel */
{
        UINT32   pixel;

        pixel = (UINT32)*p++;
        while ( bpp-- > 1 )
        {
                pixel <<= 8;
                pixel |= (UINT32)*p++;
        }
        return( pixel );
}



int OutputTGAFile(FILE *ifp, /* input file pointer */
    FILE *ofp,               /* output file pointer */
    TGAFile *sp)             /* output TGA structure */
{
        long                    byteCount;
        unsigned long   fileOffset;
        int                             i;
        int                             bytesPerPixel;
        int                             bCount;
        int                             rleCount;
        unsigned char   *imageBuff;
        unsigned char   *packBuff;
        unsigned char   outType;
        unsigned char   outDepth;
        unsigned char   outDesc;

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
                        return( -1 );
                }
                else
                {
                        outDepth = 24;
                        outType = sp->imageType;
                        outDesc = sp->imageDesc & 0xf0;
                }
        }
        else if ( !unPack && sp->imageType > 0 &&  sp->imageType < 4 )
        {
                outType = sp->imageType + 8;
                outDepth = sp->pixelDepth;
                outDesc = sp->imageDesc;
        }
        else if ( unPack && sp->imageType > 8 && sp->imageType < 12 )
        {
                outType = sp->imageType - 8;
                outDepth = sp->pixelDepth;
                outDesc = sp->imageDesc;
        }
        else
        {
                puts( "File type is inconsistent with requested operation." );
                return( -1 );
        }
        /*
        ** The output file was just opened, so the first data
        ** to be written is the standard header based on the
        ** original TGA specification.
        */
        if ( WriteByte( sp->idLength, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->mapType, ofp ) < 0 ) return( -1 );
        if ( WriteByte( outType, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->mapOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->mapLength, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->mapWidth, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->xOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->yOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->imageWidth, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->imageHeight, ofp ) < 0 ) return( -1 );
        if ( WriteByte( outDepth, ofp ) < 0 ) return( -1 );
        if ( WriteByte( outDesc, ofp ) < 0 ) return( -1 );
        if ( sp->idLength )
        {
                if ( WriteStr( sp->idString, sp->idLength, ofp ) < 0 )
                        return( -1 );
        }
        /*
        ** Now we need to copy the color map data from the input file
        ** to the output file.
        */
        byteCount = 18 + sp->idLength;
        if ( fseek( ifp, byteCount, SEEK_SET ) != 0 ) return( -1 );
        byteCount = ((sp->mapWidth + 7) >> 3) * (long)sp->mapLength;
        fileOffset = 18 + sp->idLength + byteCount;
        while ( byteCount > 0 )
        {
                if ( byteCount - CBUFSIZE < 0 )
                {
                        fread( copyBuf, 1, (int)byteCount, ifp );
                        if ( fwrite( copyBuf, 1, (int)byteCount, ofp ) != (int)byteCount )
                                return( -1 );
                }
                else
                {
                        fread( copyBuf, 1, CBUFSIZE, ifp );
                        if ( fwrite( copyBuf, 1, CBUFSIZE, ofp ) != CBUFSIZE )
                                return( -1 );
                }
                byteCount -= CBUFSIZE;
        }
        /*
        ** Now process the image data.
        */
        bytesPerPixel = (sp->pixelDepth + 7) >> 3;
        bCount = sp->imageWidth * bytesPerPixel;
        if ( (imageBuff = malloc( bCount )) == NULL )
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
                if ( (packBuff = malloc( sp->imageWidth * (bytesPerPixel + 1) )) == NULL )
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


UINT8 ReadByte(FILE *fp)
{
        UINT8   value;

        fread( &value, 1, 1, fp );
        return( value );
}


void ReadCharField(FILE *fp, char *p, int n)
{
        while ( n )
        {
                *p++ = (char)fgetc( fp );       /* no error check, no char conversion */
                --n;
        }
}




UINT32 ReadLong(FILE *fp)
{
        UINT32  value;

        fread( &value, 4, 1, fp );
        return( value );
}



int ReadRLERow(unsigned char *p, int n, /* buffer size in bytes */
    int bpp,                            /* bytes per pixel */
    FILE *fp)
{
        unsigned int                    value;
        int                                             i;
        static unsigned char    *q;

        while ( n > 0 )
        {
                if ( inRLEPacket )
                {
                        if ( packetSize * bpp > n )
                        {
                                value = n / bpp;                /* calculate pixel count */
                                packetSize -= value;
                                n = 0;
                        }
                        else
                        {
                                n -= packetSize * bpp;
                                value = packetSize;
                                packetSize = 0;
                                inRLEPacket = 0;
                        }
                        while ( value > 0 )
                        {
                                *p++ = rleBuf[0];
                                if ( bpp > 1 ) *p++ = rleBuf[1];
                                if ( bpp > 2 ) *p++ = rleBuf[2];
                                if ( bpp > 3 ) *p++ = rleBuf[3];
                                value--;
                        }
                }
                else if ( inRawPacket )
                {
                        if ( packetSize * bpp > n )
                        {
                                value = n;
                                packetSize -= n / bpp;
                                n = 0;
                        }
                        else
                        {
                                value = packetSize * bpp;       /* calculate byte count */
                                n -= value;
                                inRawPacket = 0;
                        }
                        for ( i = 0; i < value; ++i ) *p++ = *q++;
                }
                else
                {
                        /*
                        ** No accumulated data in buffers, so read from file
                        */
                        packetSize = (unsigned int)ReadByte( fp );
                        if ( packetSize & 0x80 )
                        {
                                packetSize &= 0x7f;
                                packetSize++;
                                if ( packetSize * bpp > n )
                                {
                                        value = n / bpp;                /* calculate pixel count */
                                        packetSize -= value;
                                        inRLEPacket = 1;
                                        n = 0;
                                }
                                else
                                {
                                        n -= packetSize * bpp;
                                        value = packetSize;
                                }
                                if ( fread( rleBuf, 1, bpp, fp ) != bpp ) return( -1 );
                                while ( value > 0 )
                                {
                                        *p++ = rleBuf[0];
                                        if ( bpp > 1 ) *p++ = rleBuf[1];
                                        if ( bpp > 2 ) *p++ = rleBuf[2];
                                        if ( bpp > 3 ) *p++ = rleBuf[3];
                                        value--;
                                }
                        }
                        else
                        {
                                packetSize++;
                                /*
                                ** Maximum for packetSize is 128 so as long as RLEBUFSIZ
                                ** is at least 512, and bpp is not greater than 4
                                ** we can read in the entire raw packet with one operation.
                                */
                                if ( fread( rleBuf, bpp, packetSize, fp ) != packetSize )
                                        return( -1 );
                                /*
                                ** But is there enough room to copy them to our line buffer?
                                */
                                if ( packetSize * bpp > n )
                                {
                                        value = n;                              /* number of bytes remaining */
                                        packetSize -= n / bpp;
                                        inRawPacket = 1;
                                        n = 0;
                                }
                                else
                                {
                                        value = packetSize * bpp;       /* calculate byte count */
                                        n -= value;
                                }
                                for ( i = 0, q = rleBuf; i < value; ++i ) *p++ = *q++;
                        }
                }
        }
        return( 0 );
}




UINT16 ReadShort(FILE *fp)
{
        UINT16  value;

        fread( &value, 2, 1, fp );
        return( value );
}



int RLEncodeRow(char *p, /* data to be encoded */
    char *q,             /* encoded buffer */
    int n,               /* number of pixels in buffer */
    int bpp)             /* bytes per pixel */
{
        int                             diffCount;              /* pixel count until two identical */
        int                             sameCount;              /* number of identical adjacent pixels */
        int                             RLEBufSize;             /* count of number of bytes encoded */

        RLEBufSize = 0;
        while ( n > 0 )
        {
                diffCount = CountDiffPixels( p, bpp, n );
                sameCount = CountSamePixels( p, bpp, n );
                if ( diffCount > 128 ) diffCount = 128;
                if ( sameCount > 128 ) sameCount = 128;
                if ( diffCount > 0 )
                {
                        /* create a raw packet */
                        *q++ = (char)(diffCount - 1);
                        n -= diffCount;
                        RLEBufSize += (diffCount * bpp) + 1;
                        while ( diffCount > 0 )
                        {
                                *q++ = *p++;
                                if ( bpp > 1 ) *q++ = *p++;
                                if ( bpp > 2 ) *q++ = *p++;
                                if ( bpp > 3 ) *q++ = *p++;
                                diffCount--;
                        }
                }
                if ( sameCount > 1 )
                {
                        /* create a RLE packet */
                        *q++ = (char)((sameCount - 1) | 0x80);
                        n -= sameCount;
                        RLEBufSize += bpp + 1;
                        p += (sameCount - 1) * bpp;
                        *q++ = *p++;
                        if ( bpp > 1 ) *q++ = *p++;
                        if ( bpp > 2 ) *q++ = *p++;
                        if ( bpp > 3 ) *q++ = *p++;
                }
        }
        return( RLEBufSize );
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


int WriteByte(UINT8 uc, FILE *fp)
{
        if ( fwrite( &uc, 1, 1, fp ) == 1 ) return( 0 );
        return( -1 );
}



int WriteLong(UINT32 ul, FILE *fp)
{
        if ( fwrite( &ul, 4, 1, fp ) == 1 ) return( 0 );
        return( -1 );
}


int WriteShort(UINT16 us, FILE *fp)
{
        if ( fwrite( &us, 2, 1, fp ) == 1 ) return( 0 );
        return( -1 );
}


int WriteStr(char *p, int n, FILE *fp)
{
        if ( fwrite( p, 1, n, fp ) == n ) return( 0 );
        return( -1 );
}
