/*
** Copyright (c) 1989, 1990
** Truevision, Inc.
** All Rights Reserverd
**
** TGAEDIT reads the contents of a Truevision TGA(tm) File and provides
** the ability to edit various fields from the console.  It will also
** convert an original TGA file to the new extended TGA file format.
**
** USAGE:
**              tgaedit [options] [file1] [file2] ...
**
** If no filenames are provided, the program will prompt for a file
** name.  If no extension is provided with a filename, the program
** will search for the file with ".tga", ".vst", ".icb", ".vda", or
** ".win" extension.  Options are specified by a leading '-'.
**
** Recognized options are:
**
**              -noprompt               converts old TGA to new TGA without prompting for data
**              -nostamp                omits creation of postage stamp
**              -all                    enables editing of all fields of TGA file, the
**                                              default only processes non-critical fields.
**              -noextend               force output file to be old TGA format
**              -nodev                  disables copying of developer area
**              -nocolor                disables copying of color correction table
**              -noscan                 disables copying of scan line offset table
**              -version                report version number of program
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "tga.h"

#include <config/string_case_compare.h>

/*
** Define byte counts associated with extension areas for various
** versions of the TGA specification.
*/
#define EXT_SIZE_20     495                     /* verison 2.0 extension size */

#define WARN            1                       /* provides warning message during edit */
#define NOWARN          0

#define CBUFSIZE        2048            /* size of copy buffer */
#define RLEBUFSIZ       512                     /* size of largest possible RLE packet */


extern int              main( int, char ** );
extern int              CountDiffPixels( char *, int, int );
extern int              CountSamePixels( char *, int, int );
extern int              CreatePostageStamp( FILE *, TGAFile *, TGAFile * );
extern int              DisplayImageData( unsigned char *, int, int );
extern int              EditHexNumber( char *, long int, unsigned long int *, long int,
                                        long int, int );
extern int              EditDecimalNumber( char *, long int, long int *, long int,
                                        long int, int );
extern int              EditString( char *, int, char *, int );
extern int              EditTGAFields( TGAFile * );
extern UINT32           GetPixel( unsigned char *, int );
extern int              OutputTGAFile( FILE *, FILE *, TGAFile *, TGAFile *, int,
                                        struct stat * );
extern int              ParseArgs( int, char ** );
extern void             PrintColorTable( TGAFile * );
extern void             PrintExtendedTGA( TGAFile * );
extern void             PrintImageType( int );
extern void             PrintMonth( UINT16 );
extern void             PrintScanLineTable( TGAFile * );
extern void             PrintTGAInfo( TGAFile * );
extern int              RLEncodeRow( char *, char *, int, int );
extern char             *SkipBlank( char * );
extern int              WriteByte( UINT8, FILE * );
extern int              WriteColorTable( FILE *, TGAFile * );
extern int              WriteLong( UINT32, FILE * );
extern int              WriteShort( UINT16, FILE * );
extern int              WriteStr( char *, int, FILE * );


/*
** String data for performing month conversions
*/
char    *monthStr[] =
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

TGAFile         f;                              /* control structure of image data */
TGAFile         nf;                             /* edited version of input structure */

int                     noPrompt;               /* when true, conversion done without prompts */
int                     noStamp;                /* when true, postage stamp omitted from output */
int                     noDev;                  /* when true, developer area does not get copied */
int                     noColor;                /* when true, color correction table omitted */
int                     noScan;                 /* when true, scan line table omitted */
int                     allFields;              /* when true, enables editing of all TGA fields */
int                     noExtend;               /* when true, output old TGA format */

char            rleBuf[RLEBUFSIZ];

char            copyBuf[CBUFSIZE];


char            *versionStr =
"Truevision(R) TGA(tm) File Edit Utility Version 2.0 - March 24, 1990";

char            *warnStr =
"WARNING: Changing this value may cause loss or corruption of data.";

int main(int argc, char **argv)
{
        int                     fileFound;
        int                     fileCount;
        int                     files;
        char            *q;
        FILE            *fp, *outFile;
        long            fsize;
        int                     xTGA;                   /* flags extended TGA file */
        int                     i;
        char            fileName[80];
        char            outFileName[80];
        struct stat     statbuf;

        noPrompt = 0;           /* default to prompting for changes */
        noStamp = 0;            /* default to creating postage stamp */
        noDev = 0;                      /* default to copying developer area, if present */
        noColor = 0;            /* default to copying color correction table */
        noScan = 0;                     /* default to copying scan line offset table */
        allFields = 0;          /* default to non-critical fields */
        noExtend = 0;           /* defalut to output new extended TGA format */

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
                if ( strlen( fileName ) == 0 ) exit(0);
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
                        printf( "Editing TGA File: %s\n", fileName );
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
                        xTGA = 0;
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
                                }
                                else xTGA = 1;
                                /*
                                ** If the file is an original TGA file, and falls into
                                ** one of the uncompressed image types, we can perform
                                ** an additional file size check with very little effort.
                                */
                                if ( f.imageType > 0 && f.imageType < 4 && !xTGA )
                                {
                                        /*
                                        ** Based on the header info, we should be able to calculate
                                        ** the input file size.
                                        */
                                        fsize = 18;     /* size of header in bytes */
                                        fsize += f.idLength;
                                        /* expect 8, 15, 16, 24, or 32 bits per map entry */
                                        fsize += ((f.mapWidth + 7) >> 3) * (long)f.mapLength;
                                        fsize += ((f.pixelDepth + 7) >> 3) * (long)f.imageWidth *
                                                                f.imageHeight;
                                        if ( fsize != statbuf.st_size )
                                        {
                                                /*
                                                ** Report the error, but continue to process file.
                                                */
                                                puts( "Image File Format Error." );
                                                printf("  Uncompressed File Size Should Be %ld Bytes\n",
                                                        fsize );
                                        }
                                }
                                if ( xTGA && f.extAreaOffset )
                                {
                                        if ( ReadExtendedTGA( fp, &f ) < 0 ) exit(1);
                                }
                                if ( xTGA && f.devDirOffset )
                                {
                                        if ( ReadDeveloperDirectory( fp, &f ) < 0 ) exit(1);
                                }
                                if ( !noPrompt )
                                {
                                        printf( "Press ENTER to continue: " );
                                        fgets( outFileName, sizeof( outFileName ), stdin );
                                }
                                /*
                                ** Now that we have gathered all this data from the input
                                ** file, ask the user which fields should be changed.
                                */
                                nf = f;
                                if ( noPrompt || EditTGAFields( &nf ) >= 0 )
                                {
                                        if ( !noPrompt ) puts( "(Updating File)" );
                                        /*
                                        ** If the changes were successful, write out a new
                                        ** file with the changed data
                                        */
                                        strcpy( outFileName, fileName );
                                        i = strlen( fileName );
                                        outFileName[ i - 3 ] = '\0';    /* remove extension */
                                        strcat( outFileName, "$$$" );
                                        if ( ( outFile = fopen( outFileName, "wb" ) ) != NULL )
                                        {
                                                if ( OutputTGAFile( fp, outFile, &f, &nf, xTGA, &statbuf ) < 0 )
                                                {
                                                        fclose( outFile );
                                                        remove( outFileName );
                                                        puts( "Error writing output file. No changes made." );
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
                        }
                        else
                        {
                                puts( "Error seeking to end of file for possible extension data." );
                        }
                        if ( f.scanLineTable ) free( f.scanLineTable );
                        if ( f.postStamp ) free( f.postStamp );
                        if ( f.colorCorrectTable ) free( f.colorCorrectTable );
                        if ( f.devDirs ) free( f.devDirs );
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



int CreatePostageStamp(FILE *fp, TGAFile *isp, TGAFile *sp)
{
        int                     i;
        int                     j;
        int                     dx, dy;
        int                     dxAdj;
        int                     imageXAdj, imageYAdj;
        int                     maxY;
        int                     bufSize;
        int                     bytesPerPixel;
        int                     stampSize;
        long int        fileOffset;
        unsigned char   *p, *q;
        unsigned char   *rowBuf;

        /*
        ** Create a postage stamp if reasonable to do so...
        ** Since the postage stamp size is set to 64 x 64, we
        ** require the image to be at least twice this resolution
        ** before it makes sense to increase the file size by 50%.
        ** The handling of run length encoded data also represents
        ** a more troublesome problem.
        */
        if ( sp->imageWidth > 127 && sp->imageHeight > 127 )
        {
                /*
                ** The postage stamp is created by sampling the image data.
                ** The adjustment values cause the samples to be taken from
                ** the middle of the skipped range, as well as from the middle
                ** of the image.
                */
                dx = sp->imageWidth >> 6;
                dxAdj = dx >> 1;
                imageXAdj = ( sp->imageWidth % 64 ) >> 1;
                dy = sp->imageHeight >> 6;
                imageYAdj = ( sp->imageHeight % 64 ) >> 1;
                maxY = 64 * dy + imageYAdj;

                bytesPerPixel = ( sp->pixelDepth + 7 ) >> 3;
                bufSize = bytesPerPixel * sp->imageWidth;

                stampSize = 64 * 64 * bytesPerPixel;
                if ( ( sp->postStamp = malloc( stampSize ) ) == NULL )
                        return( -1 );
                memset( sp->postStamp, 0, stampSize );
                fileOffset = 18 + isp->idLength + 
                        ((isp->mapWidth + 7) >> 3) * (long)isp->mapLength;
                if ( fseek( fp, fileOffset, SEEK_SET ) != 0 ) return( -1 );

                if ( ( rowBuf = malloc( bufSize ) ) != NULL )
                {
                        q = sp->postStamp;
                        for ( i = 0; i < maxY; ++i )
                        {
                                if ( sp->imageType > 0 && sp->imageType < 4 )
                                {
                                        fread( rowBuf, 1, bufSize, fp );
                                }
                                else if ( sp->imageType > 8 && sp->imageType < 12 )
                                {
                                        if ( ReadRLERow( rowBuf, bufSize, bytesPerPixel, fp ) < 0 )
                                        {
                                                puts( "Error reading RLE data during stamp creation." );
                                                return( -1 );
                                        }
                                }
                                else
                                {
                                        puts( "Unknown Image Type." );
                                        sp->stampOffset = 0;
                                        return( -1 );
                                }
                                if ( i < imageYAdj || ((i - imageYAdj) % dy) ) continue;
                                p = rowBuf + ( bytesPerPixel * (imageXAdj + dxAdj) );
                                for ( j = 0; j < 64; ++j )
                                {
                                        *q++ = *p;
                                        if ( bytesPerPixel > 1 ) *q++ = *(p + 1);
                                        if ( bytesPerPixel > 2 ) *q++ = *(p + 2);
                                        if ( bytesPerPixel > 3 ) *q++ = *(p + 3);
                                        p += dx * bytesPerPixel;
                                }
                        }
                        free( rowBuf );
                        sp->stampWidth = sp->stampHeight = 64;
                }
                else
                {
                        puts( "Unable to create row buffer." );
                        return( -1 );
                }
        }
        else
        {
                sp->stampOffset = 0;
        }
        return( 0 );
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



int EditDecimalNumber(char *s, long n, long *retVal, long min, long max, int warning)
{
        int             i;
        int             c;
        char    *p;
        char    str[20];

        do
        {
                puts( s );
                printf( "%ld\n\n", n );
                printf( "Enter new value (min = %ld, max = %ld), ESC for no change:\n",
                                min, max );
                if ( warning ) puts( warnStr );
                p = str;
                memset( str, 0, 20 );
                for ( i = 0; i < 19; ++i )
                {
                        c = getchar();
                        if ( c == '\033' )
                        {
                                putchar( '\n' );
                                return( -1 );
                        }
                        if ( c == '\n' || c == '\r' )
                        {
                                putchar( '\n' );
                                if ( *str == '\0' ) return( -1 );
                                break;
                        }
                        if ( c == '\b' )
                        {
                                if ( p > str ) --p;
                                if ( i > 0 ) --i;
                                *p = '\0';
                                putchar( ' ' );
                                putchar( '\b' );
                        }
                        else *p++ = (char)c;
                }
                *retVal = atol( str );
        } while ( *retVal < min || *retVal > max );
        return( 0 );
}



int EditHexNumber(char *s, long n, unsigned long *retVal, long min, long max, int warning)
{
        int             i;
        int             c;
        char    *p;
        char    str[20];

        do
        {
                puts( s );
                printf( "%lx\n\n", n );
                printf( "Enter new hexadecimal value (min = %lx, max = %lx), ESC for no change:\n",
                                min, max );
                if ( warning ) puts( warnStr );
                p = str;
                memset( str, 0, 20 );
                for ( i = 0; i < 19; ++i )
                {
                        c = getchar();
                        if ( c == '\033' )
                        {
                                putchar( '\n' );
                                return( -1 );
                        }
                        if ( c == '\n' || c == '\r' )
                        {
                                putchar( '\n' );
                                if ( *str == '\0' ) return( -1 );
                                break;
                        }
                        if ( c == '\b' )
                        {
                                if ( p > str ) --p;
                                if ( i > 0 ) --i;
                                *p = '\0';
                                putchar( ' ' );
                                putchar( '\b' );
                        }
                        else *p++ = (char)c;
                }
                sscanf( str, "%lx", retVal );
        } while ( *retVal < min || *retVal > max );
        return( 0 );
}



int EditString(char *is, int il, char *os, int ol)
{
        int             i;
        int             c;
        char    *p;
        char    *q;

        q = is;
        for ( i = 0; i < il; ++i )
        {
                if ( i > 0 && (i % 80) == 0 ) putchar( '\n' );
                if ( *q ) putchar( *q );
                else break;
                ++q;
        }
        printf( "\n\nEnter new string (max %d characters), ESC for no change:\n", ol );
        p = os;
        for ( i = 0; i < ol; ++i )
        {
                c = getchar();
                if ( c == '\033' )
                {
                        putchar( '\n' );
                        return( -1 );
                }
                if ( c == '\n' || c == '\r' )
                {
                        putchar( '\n' );
                        /* remove blank string if found */
                        c = strlen( is );
                        if ( *os == '\0' && c > 0 && SkipBlank( is ) != (is + c) )
                                return( -1 );
                        break;
                }
                if ( c == '\b' )
                {
                        if ( p > os ) --p;
                        if ( i > 0 ) --i;
                        *p = '\0';
                        putchar( ' ' );
                        putchar( '\b' );
                }
                else *p++ = (char)c;
        }
        return( 0 );
}



int EditTGAFields(TGAFile *sp)
{
        long    value;
        char    txt[256];

        if ( allFields )
        {
                if ( EditDecimalNumber( "Color Map Type:",
                        (long)sp->mapType, &value, 0L, 1L, WARN ) == 0 )
                {
                        sp->mapType = (UINT8)value;
                }
                if ( EditDecimalNumber( "Image Type:",
                        (long)sp->imageType, &value, 0L, 11L, WARN ) == 0 )
                {
                        sp->imageType = (UINT8)value;
                }
                if ( EditDecimalNumber( "Color Map Origin:",
                        (long)sp->mapOrigin, &value, 0L, 65535L, WARN ) == 0 )
                {
                        sp->mapOrigin = (UINT16)value;
                }
                if ( EditDecimalNumber( "Color Map Length:",
                        (long)sp->mapLength, &value, 0L, 65535L, WARN ) == 0 )
                {
                        sp->mapLength = (UINT16)value;
                }
                if ( EditDecimalNumber( "Color Map Entry Size:",
                        (long)sp->mapWidth, &value, 0L, 32L, WARN ) == 0 )
                {
                        sp->mapWidth = (UINT8)value;
                }
                if ( EditDecimalNumber( "Image X Origin:",
                        (long)sp->xOrigin, &value, 0L, 65535L, NOWARN ) == 0 )
                {
                        sp->xOrigin = (UINT16)value;
                }
                if ( EditDecimalNumber( "Image Y Origin:",
                        (long)sp->yOrigin, &value, 0L, 65535L, NOWARN ) == 0 )
                {
                        sp->yOrigin = (UINT16)value;
                }
                if ( EditDecimalNumber( "Image Width:",
                        (long)sp->imageWidth, &value, 0L, 65535L, WARN ) == 0 )
                {
                        sp->imageWidth = (UINT16)value;
                }
                if ( EditDecimalNumber( "Image Height:",
                        (long)sp->imageHeight, &value, 0L, 65535L, WARN ) == 0 )
                {
                        sp->imageHeight = (UINT16)value;
                }
                if ( EditDecimalNumber( "Pixel Depth:",
                        (long)sp->pixelDepth, &value, 0L, 32L, WARN ) == 0 )
                {
                        sp->pixelDepth = (UINT8)value;
                }
                if ( EditHexNumber( "Image Descriptor:",
                        (long)sp->imageDesc, &value, 0L, 0xffL, NOWARN ) == 0 )
                {
                        sp->imageDesc = (UINT8)value;
                }
        }

        memset( txt, 0, 256 );
        puts( "Current ID String:" );
        if ( EditString( sp->idString, sp->idLength, txt, 255 ) == 0 )
        {
                strcpy( sp->idString, txt );
                sp->idLength = (UINT8)strlen( sp->idString );
        }

        /*
        ** If creating old TGA format output, no need to edit remaining
        ** fields...
        */
        if ( noExtend ) return( 0 );

        memset( txt, 0, 256 );
        puts( "Author Name:" );
        if ( EditString( sp->author, strlen( sp->author), txt, 40 ) == 0 )
        {
                strcpy( sp->author, txt );
        }
        memset( txt, 0, 256 );
        puts( "Author Comments (line 1):" );
        if ( EditString( &sp->authorCom[0][0], strlen( &sp->authorCom[0][0] ),
                        txt, 80 ) == 0 )
        {
                strcpy( &sp->authorCom[0][0], txt );
        }
        memset( txt, 0, 256 );
        puts( "Author Comments (line 2):" );
        if ( EditString( &sp->authorCom[1][0], strlen( &sp->authorCom[1][0] ),
                        txt, 80 ) == 0 )
        {
                strcpy( &sp->authorCom[1][0], txt );
        }
        memset( txt, 0, 256 );
        puts( "Author Comments (line 3):" );
        if ( EditString( &sp->authorCom[2][0], strlen( &sp->authorCom[2][0] ),
                        txt, 80 ) == 0 )
        {
                strcpy( &sp->authorCom[2][0], txt );
        }
        memset( txt, 0, 256 );
        puts( "Author Comments (line 4):" );
        if ( EditString( &sp->authorCom[3][0], strlen( &sp->authorCom[3][0] ),
                        txt, 80 ) == 0 )
        {
                strcpy( &sp->authorCom[3][0], txt );
        }

        if ( EditDecimalNumber( "Date/Time Stamp (month):",
                (long)sp->month, &value, 0L, 12L, NOWARN ) == 0 )
        {
                sp->month = (UINT16)value;
        }

        if ( EditDecimalNumber( "Date/Time Stamp (day):",
                        (long)sp->day, &value, 0L, 31L, NOWARN ) == 0 )
        {
                sp->day = (UINT16)value;
        }

        /*
        ** Minimum and maximum year values are rather arbitrary...
        */
        if ( EditDecimalNumber( "Date/Time Stamp (year):",
                        (long)sp->year, &value, 1975L, 2200L, NOWARN ) == 0 )
        {
                sp->year = (UINT16)value;
        }

        if ( EditDecimalNumber( "Date/Time Stamp (hour):",
                        (long)sp->hour, &value, 0L, 23L, NOWARN ) == 0 )
        {
                sp->hour = (UINT16)value;
        }

        if ( EditDecimalNumber( "Date/Time Stamp (minute):",
                        (long)sp->minute, &value, 0L, 59L, NOWARN ) == 0 )
        {
                sp->minute = (UINT16)value;
        }

        if ( EditDecimalNumber( "Date/Time Stamp (second):",
                        (long)sp->second, &value, 0L, 59L, NOWARN ) == 0 )
        {
                sp->second = (UINT16)value;
        }
        memset( txt, 0, 256 );
        puts( "Job Name/ID:" );
        if ( EditString( sp->jobID, strlen( sp->jobID), txt, 40 ) == 0 )
        {
                strcpy( sp->jobID, txt );
        }

        if ( EditDecimalNumber( "Job Time (hours):",
                        (long)sp->jobHours, &value, 0L, 65535L, NOWARN ) == 0 )
        {
                sp->jobHours = (UINT16)value;
        }

        if ( EditDecimalNumber( "Job Time (minutes):",
                        (long)sp->jobMinutes, &value, 0L, 59L, NOWARN ) == 0 )
        {
                sp->jobMinutes = (UINT16)value;
        }

        if ( EditDecimalNumber( "Job Time (seconds):",
                        (long)sp->jobSeconds, &value, 0L, 59L, NOWARN ) == 0 )
        {
                sp->jobSeconds = (UINT16)value;
        }

        memset( txt, 0, 256 );
        puts( "Software ID:" );
        if ( EditString( sp->softID, strlen( sp->softID), txt, 40 ) == 0 )
        {
                strcpy( sp->softID, txt );
        }

        if ( EditDecimalNumber( "Software Version Number * 100:",
                        (long)sp->versionNum, &value, 0L, 65535L, NOWARN ) == 0 )
        {
                sp->versionNum = (UINT16)value;
        }

        memset( txt, 0, 256 );
        puts( "Software Version Letter:" );
        if ( EditString( &sp->versionLet, strlen( &sp->versionLet), txt, 1 ) == 0 )
        {
                sp->versionLet = txt[0];
        }
        if ( sp->versionLet == '\0' ) sp->versionLet = ' ';

        if ( EditHexNumber( "Key Color (ARGB):",
                        sp->keyColor, (unsigned long *)&value, 0L, 0xffffffffL, NOWARN ) == 0 )
        {
                sp->keyColor = (UINT32)value;
        }

        if ( EditDecimalNumber( "Pixel Aspect Ratio Numerator (width):",
                        (long)sp->pixNumerator, &value, 0L, 32767L, NOWARN ) == 0 )
        {
                sp->pixNumerator = (UINT16)value;
        }

        if ( EditDecimalNumber( "Pixel Aspect Ratio Denominator (height):",
                        (long)sp->pixDenominator, &value, 0L, 32767L, NOWARN ) == 0 )
        {
                sp->pixDenominator = (UINT16)value;
        }

        if ( EditDecimalNumber( "Gamma Correction Ratio Numerator:",
                        (long)sp->gammaNumerator, &value, 0L, 32767L, NOWARN ) == 0 )
        {
                sp->gammaNumerator = (UINT16)value;
        }

        if ( EditDecimalNumber( "Gamma Correction Ratio Denominator:",
                        (long)sp->gammaDenominator, &value, 0L, 32767L, NOWARN ) == 0 )
        {
                sp->gammaDenominator = (UINT16)value;
        }

        if ( EditDecimalNumber( "Alpha Attributes Type:",
                        (long)sp->alphaAttribute, &value, 0L, 255L, NOWARN ) == 0 )
        {
                sp->alphaAttribute = (UINT8)value;
        }
        /*
        ** Attempt to make the alpha descriptor fields consistent
        */
        if ( sp->alphaAttribute == 0 && (sp->imageDesc & 0xf) != 0 )
        {
                sp->alphaAttribute = 2;
        }

        return( 0 );    /* return 0 for success, -1 to abort */
}



/*
** Retrieve a pixel value from a buffer.  The actual size and order
** of the bytes is not important since we are only using the value
** for comparisons with other pixels.
*/

UINT32 GetPixel(unsigned char *p, int bpp)
{
        unsigned long   pixel;

        pixel = (unsigned long)*p++;
        while ( bpp-- > 1 )
        {
                pixel <<= 8;
                pixel |= (unsigned long)*p++;
        }
        return( pixel );
}



/*
FILE            *ifp;           input file pointer
FILE            *ofp;           output file pointer
TGAFile         *isp;           input TGA structure
TGAFile         *sp;            output TGA structure
int             xTGA;           flags input file as new TGA format
*/
int OutputTGAFile(FILE *ifp, FILE *ofp, TGAFile *isp, TGAFile *sp, int xTGA, struct stat *isbp)
{
        long                    byteCount;
        long                    imageByteCount;
        unsigned long   fileOffset;
        int                             i;
        int                             bytesPerPixel;

        /*
        ** The output file was just opened, so the first data
        ** to be written is the standard header based on the
        ** original TGA specification.
        */
        if ( WriteByte( sp->idLength, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->mapType, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->imageType, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->mapOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->mapLength, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->mapWidth, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->xOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->yOrigin, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->imageWidth, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->imageHeight, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->pixelDepth, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->imageDesc, ofp ) < 0 ) return( -1 );
        if ( sp->idLength )
        {
                if ( WriteStr( sp->idString, sp->idLength, ofp ) < 0 )
                        return( -1 );
        }
        /*
        ** Now we need to copy the color map data from the input file
        ** to the output file.
        */
        byteCount = 18 + isp->idLength;
        if ( fseek( ifp, byteCount, SEEK_SET ) != 0 ) return( -1 );
        byteCount = ((isp->mapWidth + 7) >> 3) * (long)isp->mapLength;
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
        ** Similarly, the image data can now be copied.
        ** This gets a little trickier since the input image could
        ** be compressed or the file could be in the new TGA format...
        */
        bytesPerPixel = (isp->pixelDepth + 7) >> 3;
        if ( isp->imageType > 0 && isp->imageType < 4 )
        {
                byteCount = bytesPerPixel *
                                        (long)isp->imageWidth *
                                        (long)isp->imageHeight;
        }
        else if ( isp->imageType > 8 && isp->imageType < 12 )
        {
                imageByteCount = CountRLEData( ifp, isp->imageWidth,
                                isp->imageHeight, bytesPerPixel );
                /* Recalculate offset to beginning of image data */
                byteCount = 18 + isp->idLength;
                byteCount += ((isp->mapWidth + 7) >> 3) * (long)isp->mapLength;
                if ( fseek( ifp, byteCount, SEEK_SET ) != 0 ) return( -1 );
                byteCount = imageByteCount;
        }
        else if ( !xTGA )
        {
                /*
                ** If the file is not an extended TGA file, we can
                ** calculate the image byte count based upon the size
                ** of the file (hopefully).
                */
                byteCount = 18 + isp->idLength;
                byteCount += ((isp->mapWidth + 7) >> 3) * (long)isp->mapLength;
                byteCount = isbp->st_size - byteCount;
        }
        else
        {
                puts( "Cannot determine amount of image data" );
                return( -1 );
        }
        fileOffset += byteCount;
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

        if ( noExtend ) return( 0 );

        /*
        ** Attempt to preserve developer area if it exists in the input file
        */
        if ( !noDev && isp->devDirOffset != 0 )
        {
                if ( (sp->devDirs = malloc( sp->devTags * sizeof(DevDir) ) ) == NULL )
                {
                        puts( "Failed to allocate memory for new developer directory." );
                        return( -1 );
                }
                for ( i = 0; i < sp->devTags; ++i )
                {
                        if ( fseek( ifp, isp->devDirs[i].tagOffset, SEEK_SET ) != 0 )
                        {
                                puts( "Error seeking to developer entry." );
                                free( sp->devDirs );
                                return( -1 );
                        }
                        sp->devDirs[i].tagOffset = fileOffset;
                        byteCount = isp->devDirs[i].tagSize;
                        fileOffset += byteCount;
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
                }
                sp->devDirOffset = fileOffset;
                WriteShort( sp->devTags, ofp );
                byteCount = (long)sp->devTags * sizeof( DevDir );
                if ( (long)fwrite( sp->devDirs, 1, (int)byteCount, ofp ) != byteCount )
                {
                        puts( "Error writing developer area." );
                        free( sp->devDirs );
                        return( -1 );
                }
                fileOffset += byteCount + 2;
                free( sp->devDirs );
        }

        /*
        ** Unlike the figure in the specification, we will output
        ** the scan line table, the postage stamp, and the color
        ** correction table before we output the extension area.
        ** This simply makes the calculation of the offset values
        ** easier to manage...
        ** Copy the scan line table from the input file to
        ** the output file.  A future version could create the table
        ** if it does not already exist.
        */
        if ( !noScan && isp->scanLineOffset != 0L )
        {
                if ( fseek( ifp, isp->scanLineOffset, SEEK_SET ) != 0 )
                        return( -1 );
                sp->scanLineOffset = fileOffset;
                for ( i = 0; i < sp->imageHeight; ++i )
                {
                        if ( WriteLong( ReadLong( ifp ), ofp ) < 0 ) return( -1 );
                }
                fileOffset += sp->imageHeight * sizeof( UINT32 );
        }

        /*
        ** Either copy the postage stamp from the input file to
        ** the output file, or create one.
        */
        if ( !noStamp )
        {
                if ( isp->stampOffset != 0 )
                {
                        if ( fseek( ifp, isp->stampOffset, SEEK_SET ) != 0 )
                                return( -1 );
                        /*
                        ** Since postage stamps are uncompressed, calculation
                        ** of its size is straight forward.
                        */
                        byteCount = bytesPerPixel *
                                                (long)isp->stampWidth *
                                                (long)isp->stampHeight + 2;

                        sp->stampOffset = fileOffset;
                        fileOffset += byteCount;
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
                }
                else
                {
                        if ( (isp->imageType > 0 && isp->imageType < 4 ) ||
                                 (isp->imageType > 8 && isp->imageType < 12 ) )
                        {
                                if ( CreatePostageStamp( ifp, isp, sp ) >= 0 && sp->postStamp )
                                {
                                        sp->stampOffset = fileOffset;
                                        WriteByte( sp->stampWidth, ofp );
                                        WriteByte( sp->stampHeight, ofp );
                                        i = sp->stampWidth * sp->stampHeight *
                                                bytesPerPixel;
                                        fileOffset += i + 2;
                                        if ( fwrite( sp->postStamp, 1, i, ofp ) != i )
                                        {
                                                puts( "Error writing postage stamp." );
                                                return( -1 );
                                        }
                                }
                                else
                                {
                                        if ( sp->postStamp )
                                                puts( "Error creating postage stamp." );
                                        else
                                                puts( "Image size too small for stamp" );
                                        return( -1 );
                                }
                        }
                        else
                        {
                                puts( "Do not know how to create postage stamp." );
                        }
                }
        }
        else sp->stampOffset = 0L;

        /*
        ** Next copy the Color Correction Table to the output file
        */
        if ( !noColor && sp->colorCorrectTable != NULL )
        {
                sp->colorCorrectOffset = fileOffset;
                if ( WriteColorTable( ofp, sp ) < 0 ) return( -1 );
                fileOffset += 1024 * sizeof(UINT16);
        }

        /*
        ** Output TGA extension area - version 2.0 format
        */
        sp->extAreaOffset = fileOffset;
        if ( WriteShort( EXT_SIZE_20, ofp ) < 0 ) return( -1 );
        if ( WriteStr( sp->author, 41, ofp ) < 0 ) return( -1 );
        if ( WriteStr( &sp->authorCom[0][0], 81, ofp ) < 0 ) return( -1 );
        if ( WriteStr( &sp->authorCom[1][0], 81, ofp ) < 0 ) return( -1 );
        if ( WriteStr( &sp->authorCom[2][0], 81, ofp ) < 0 ) return( -1 );
        if ( WriteStr( &sp->authorCom[3][0], 81, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->month, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->day, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->year, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->hour, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->minute, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->second, ofp ) < 0 ) return( -1 );
        if ( WriteStr( sp->jobID, 41, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->jobHours, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->jobMinutes, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->jobSeconds, ofp ) < 0 ) return( -1 );
        if ( WriteStr( sp->softID, 41, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->versionNum, ofp ) < 0 ) return( -1 );
        if ( sp->versionLet == '\0' ) sp->versionLet = ' ';
        if ( WriteByte( sp->versionLet, ofp ) < 0 ) return( -1 );
        if ( WriteLong( sp->keyColor, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->pixNumerator, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->pixDenominator, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->gammaNumerator, ofp ) < 0 ) return( -1 );
        if ( WriteShort( sp->gammaDenominator, ofp ) < 0 ) return( -1 );
        if ( WriteLong( sp->colorCorrectOffset, ofp ) < 0 ) return( -1 );
        if ( WriteLong( sp->stampOffset, ofp ) < 0 ) return( -1 );
        if ( WriteLong( sp->scanLineOffset, ofp ) < 0 ) return( -1 );
        if ( WriteByte( sp->alphaAttribute, ofp ) < 0 ) return( -1 );

        /*
        ** For now, simply output extended tag info
        */
        if ( WriteLong( sp->extAreaOffset, ofp ) < 0 ) return( -1 );
        if ( WriteLong( sp->devDirOffset, ofp ) < 0 ) return( -1 );
        if ( WriteStr( "TRUEVISION-XFILE.\0", 18, ofp ) < 0 ) return( -1 );
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
                        if ( string_case_compare( p, "noprompt" ) == 0 ) noPrompt = 1;
                        else if ( string_case_compare( p, "nostamp" ) == 0 ) noStamp = 1;
                        else if ( string_case_compare( p, "all" ) == 0 ) allFields = 1;
                        else if ( string_case_compare( p, "noextend" ) == 0 ) noExtend = 1;
                        else if ( string_case_compare( p, "nodev" ) == 0 ) noDev = 1;
                        else if ( string_case_compare( p, "nocolor" ) == 0 ) noColor = 1;
                        else if ( string_case_compare( p, "noscan" ) == 0 ) noScan = 1;
                        else if ( string_case_compare( p, "version" ) == 0 )
                        {
                                puts( versionStr );
                                exit( 0 );
                        }
                        else
                        {
                                puts( "Usage: tgaedit [options] [file1] [file2...]" );
                                puts( "  where options can be:" );
                                puts( "    -noprompt\t\tprocess without prompting for changes" );
                                puts( "    -nostamp\t\tsuppress postage stamp" );
                                puts( "    -nodev\t\tsuppress developer area" );
                                puts( "    -nocolor\t\tsuppress color correction table" );
                                puts( "    -noscan\t\tsuppress scan line offset table" );
                                puts( "    -all\t\tallow editing of all TGA fields" );
                                puts( "    -noextend\t\toutput old TGA format" );
                                puts( "    -version\t\treport version number" );
                                exit( 0 );
                        }
                }
                else ++n;
        }
        return( n );
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



void PrintExtendedTGA(TGAFile *sp)
{
        register int    strSize;
        char                    *blankChars = " \t";

        puts( "***** Extended TGA Fields *****" );
        printf( "Truevision TGA File Format Version: " );
        if ( sp->extSize == EXT_SIZE_20 ) puts( "2.0" );
        else printf( "UNKNOWN, extension size = %d\n", sp->extSize );

        /*
        ** Make sure the strings have length, and contain something
        ** other than blanks and tabs
        */
        strSize = strlen( sp->author );
        if ( strSize && strspn( sp->author, blankChars ) < strSize )
        {
                printf( "Author: %s\n", sp->author );
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
                printf( "Date Image Saved: " );
                PrintMonth( sp->month );
                printf( " %02u, %4u at %02u:%02u:%02u\n", sp->day, sp->year,
                        sp->hour, sp->minute, sp->second );
        }

        strSize = strlen( sp->jobID );
        if ( strSize && strspn( sp->jobID, blankChars ) < strSize )
        {
                printf( "Job Name/ID: %s\n", sp->jobID );
        }

        if ( sp->jobHours != 0 || sp->jobMinutes != 0 || sp->jobSeconds != 0 )
        {
                printf( "Job Elapsed Time: %02u:%02u:%02u\n", sp->jobHours,
                        sp->jobMinutes, sp->jobSeconds );
        }

        strSize = strlen( sp->softID );
        if ( strSize && strspn( sp->softID, blankChars ) < strSize )
        {
                printf( "Software ID: %s\n", sp->softID );
        }

        if ( sp->versionNum != 0 || sp->versionLet != ' ' )
        {
                printf( "Software Version: %d.%d%c\n", sp->versionNum/100,
                        sp->versionNum % 100, sp->versionLet );
        }

        printf( "Key Color: 0x%02x(%u) Alpha, 0x%02x(%u) Red, 0x%02x(%u) Green, 0x%02x(%u) Blue\n",
                sp->keyColor >> 24, sp->keyColor >> 24,
                (sp->keyColor >> 16) & 0xff, (sp->keyColor >> 16) & 0xff,
                (sp->keyColor >> 8) & 0xff, (sp->keyColor >> 8) & 0xff,
                sp->keyColor & 0xff, sp->keyColor & 0xff );

        if ( sp->pixNumerator != 0 && sp->pixDenominator != 0 )
        {
                printf( "Pixel Aspect Ratio: %f\n", (double)sp->pixNumerator /
                        (double)sp->pixDenominator );
        }

        if ( sp->gammaDenominator != 0 )
        {
                printf( "Gamma Correction: %f\n", (double)sp->gammaNumerator /
                        (double)sp->gammaDenominator );
        }

        printf( "Color Correction Offset = 0x%08x\n", sp->colorCorrectOffset );
        if ( sp->colorCorrectOffset && sp->colorCorrectTable )
        {
                PrintColorTable( sp );
        }
        printf( "Postage Stamp Offset = 0x%08x\n", sp->stampOffset );
        if ( sp->stampOffset )
        {
                printf( "Postage Stamp Width, Height: %3u, %3u\n",
                                        sp->stampWidth, sp->stampHeight );
        }
        printf( "Scan Line Offset = 0x%08x\n", sp->scanLineOffset );
        if ( sp->scanLineOffset && sp->scanLineTable )
        {
                PrintScanLineTable( sp );
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



/*
char    *p;             data to be encoded
char    *q;             encoded buffer
int     n;              number of pixels in buffer
int     bpp;            bytes per pixel
 */
int RLEncodeRow(char *p, char *q, int n, int bpp)
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


int WriteByte(UINT8 uc, FILE *fp)
{
        if ( fwrite( &uc, 1, 1, fp ) == 1 ) return( 0 );
        return( -1 );
}


int WriteColorTable(FILE *fp, TGAFile *sp)
{
        UINT16  *p;
        UINT16  n;

        p = sp->colorCorrectTable;
        for ( n = 0; n < 1024; ++n )
        {
                if ( WriteShort( *p++, fp ) < 0 ) return( -1 );
        }
        return( 0 );
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
