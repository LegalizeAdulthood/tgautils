/*
** Copyright (c) 1989, 1990
** Truevision, Inc.
** All Rights Reserverd
**
** VSTAMP displays postage stamp data from a Truevision(R) TGA(tm) file
** on an ATVista(R) using STAGE(tm) routines.
*/

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stage.h>
#include "tga.h"

extern void     main( int, char ** );
extern void     DisplayPostageStamp( FILE *, TGAFile * );
extern UINT8    ReadByte( FILE * );
extern void     ReadCharField( FILE *, char *, int );
extern int      ReadExtendedTGA( FILE *, TGAFile * );
extern UINT16   ReadShort( FILE * );
extern UINT32   ReadLong( FILE * );


TGAFile         f;                              /* control structure of image data */

char            *versionStr =
"Truevision(R) ATVista(R) Postage Stamp Viewer Version 2.0 - March 24, 1990";

char            copyBuf[1024];

void
main( argc, argv )
int argc;
char **argv;
{
        int                     fileFound;
        float           zoomFact;
        double          doubleTmp;
        char            *q;
        FILE            *fp;
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
        if ( StageAutoInit() == 0 )
        {
                puts( "VStamp: Unable to load or initialize STAGE" );
                exit( 0 );
        }
        zoomFact = (float)1.0;
        if ( argc == 1 )
        {
                printf( "Enter name of file to examine: " );
                gets( fileName );
                if ( strlen( fileName ) == 0 ) exit( 0 );
        }
        else
        {
                strcpy( fileName, argv[1] );
                if ( argc > 2 )
                {
                        doubleTmp = atof( argv[2] );
                        if ( doubleTmp > 0.0 )
                                zoomFact = (float)doubleTmp;
                }
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
                printf( "Image File: %s\n", fileName );
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
                                puts( "VStamp: File is not in extended TGA format." );
                                exit( 0 );
                        }
                        if ( f.extAreaOffset )
                        {
                                if ( ReadExtendedTGA( fp, &f ) >= 0 )
                                {
                                        if ( f.stampOffset )
                                        {
                                                DisplayPostageStamp( fp, &f );
                                                if ( argc > 2 )
                                                {
                                                        SetZoom( FloatToFixed(zoomFact),
                                                                                FloatToFixed(zoomFact) );
                                                }
                                        }
                                        else
                                        {
                                                puts( "VStamp: File does not contain postage stamp data." );
                                        }
                                }
                        }
                        else
                        {
                                puts( "VStamp: File does not contain extension area." );
                        }
                }
                else
                {
                        puts( "VStamp: Error seeking to end of file for possible extension data" );
                }
                fclose( fp );
                exit( 0 );
        }
}



void
DisplayPostageStamp( fp, sp )
FILE    *fp;
TGAFile *sp;
{
        Rect    r;
        int             i;
        char    *p;
        int             bytesPerPixel;
        int             bytesInRow;
        int             orient;

        /*
        ** If we got this far, the file pointer should be pointing
        ** to the first byte of the postage stamp data (just after
        ** the two bytes specifying the size of the stamp).
        */
        bytesPerPixel = (sp->pixelDepth + 7) >> 3;
        bytesInRow = sp->stampWidth * bytesPerPixel;
        if ( (p = malloc( bytesInRow )) != NULL )
        {
                /*
                ** Test for top-to-bottom or bottom-to-top, but ignore
                ** left-to-right info.
                */
                orient = (sp->imageDesc >> 5) & 0x1;
                if ( orient == 0 )
                {
                        SetRect( &r, 0, sp->stampHeight - 1,
                                sp->stampWidth, sp->stampHeight );
                        /*
                        ** Postage stamp data always stored in uncompressed format
                        */
                        for ( i = 0; i < sp->stampHeight; ++i )
                        {
                                fread( p, 1, bytesInRow, fp );
                                PutHostImageToPort( r, p, (unsigned int)sp->pixelDepth );
                                r.y1--;
                                r.y2--;
                        }
                }
                else
                {
                        SetRect( &r, 0, 0, sp->stampWidth, 1 );
                        for ( i = 0; i < sp->stampHeight; ++i )
                        {
                                fread( p, 1, bytesInRow, fp );
                                PutHostImageToPort( r, p, (unsigned int)sp->pixelDepth );
                                r.y1++;
                                r.y2++;
                        }
                }
                free( p );
        }
        else puts( "Unable to allocate scan line buffer" );
}


UINT8
ReadByte( fp )
FILE *fp;
{
        UINT8   value;

#if MSDOS
        fread( &value, 1, 1, fp );
#else
#endif
        return( value );
}


void
ReadCharField( fp, p, n )
FILE    *fp;
char    *p;
int             n;
{
        while ( n )
        {
                *p++ = (char)fgetc( fp );       /* no error check, no char conversion */
                --n;
        }
}


int
ReadExtendedTGA( fp, sp )
FILE    *fp;
TGAFile *sp;
{
        if ( !fseek( fp, sp->extAreaOffset, SEEK_SET ) )
        {
                sp->extSize = ReadShort( fp );
                memset( sp->author, 0, 41 );
                ReadCharField( fp, sp->author, 41 );
                memset( &sp->authorCom[0][0], 0, 81 );
                ReadCharField( fp, &sp->authorCom[0][0], 81 );
                memset( &sp->authorCom[1][0], 0, 81 );
                ReadCharField( fp, &sp->authorCom[1][0], 81 );
                memset( &sp->authorCom[2][0], 0, 81 );
                ReadCharField( fp, &sp->authorCom[2][0], 81 );
                memset( &sp->authorCom[3][0], 0, 81 );
                ReadCharField( fp, &sp->authorCom[3][0], 81 );

                sp->month = ReadShort( fp );
                sp->day = ReadShort( fp );
                sp->year = ReadShort( fp );
                sp->hour = ReadShort( fp );
                sp->minute = ReadShort( fp );
                sp->second = ReadShort( fp );

                memset( sp->jobID, 0, 41 );
                ReadCharField( fp, sp->jobID, 41 );
                sp->jobHours = ReadShort( fp );
                sp->jobMinutes = ReadShort( fp );
                sp->jobSeconds = ReadShort( fp );

                memset( sp->softID, 0, 41 );
                ReadCharField( fp, sp->softID, 41 );
                sp->versionNum = ReadShort( fp );
                sp->versionLet = ReadByte( fp );

                sp->keyColor = ReadLong( fp );
                sp->pixNumerator = ReadShort( fp );
                sp->pixDenominator = ReadShort( fp );

                sp->gammaNumerator = ReadShort( fp );
                sp->gammaDenominator = ReadShort( fp );

                sp->colorCorrectOffset = ReadLong( fp );
                sp->stampOffset = ReadLong( fp );
                sp->scanLineOffset = ReadLong( fp );

                sp->alphaAttribute = ReadByte( fp );

                sp->colorCorrectTable = (UINT16 *)0;

                sp->postStamp = (void *)0;
                if ( sp->stampOffset )
                {
                        if ( !fseek( fp, sp->stampOffset, SEEK_SET ) )
                        {
                                sp->stampWidth = ReadByte( fp );
                                sp->stampHeight = ReadByte( fp );
                        }
                        else
                        {
                                printf( "Error seeking to Postage Stamp, offset = 0x%08lx\n",
                                        sp->stampOffset );
                        }
                }

                sp->scanLineTable = (UINT32 *)0;
        }
        else
        {
                printf( "Error seeking to Extended TGA Area, offset = 0x%08lx\n",
                        sp->extAreaOffset );
                return( -1 );
        }
        return( 0 );
}


UINT32
ReadLong( fp )
FILE *fp;
{
        UINT32  value;

#if MSDOS
        fread( &value, 1, 4, fp );
#else
#endif
        return( value );
}



UINT16
ReadShort( fp )
FILE *fp;
{
        UINT16  value;

#if MSDOS
        fread( &value, 1, 2, fp );
#else
#endif
        return( value );
}
