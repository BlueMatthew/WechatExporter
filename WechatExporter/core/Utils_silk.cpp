#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#endif

#include "Utils.h"
#include "FileSystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <silk/SKP_Silk_SDK_API.h>
#include <silk/SKP_Silk_SigProc_FIX.h>

#include <opencore-amrnb/interf_dec.h>

#ifdef _WIN32
#include <atlstr.h>
#endif

#include "Utils.h"

/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2


/* From WmfDecBytesPerFrame in dec_input_format_tab.cpp */
const int amr_frame_sizes[] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 6, 5, 5, 0, 0, 0, 0 };


#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(
    SKP_int16       vec[],
    SKP_int         len
)
{
    SKP_int i;
    SKP_int16 tmp;
    SKP_uint8 *p1, *p2;

    for( i = 0; i < len; i++ ){
        tmp = vec[ i ];
        p1 = (SKP_uint8 *)&vec[ i ]; p2 = (SKP_uint8 *)&tmp;
        p1[ 0 ] = p2[ 1 ]; p1[ 1 ] = p2[ 0 ];
    }
}
#endif

#if (defined(_WIN32) || defined(_WINCE))
#include <windows.h>    /* timer */
#else    // Linux or Mac
#include <sys/time.h>
#endif

#ifdef _WIN32

unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    /* Returns a time counter in microsec    */
    /* the resolution is platform dependent */
    /* but is typically 1.62 us resolution  */
    LARGE_INTEGER lpPerformanceCount;
    LARGE_INTEGER lpFrequency;
    QueryPerformanceCounter(&lpPerformanceCount);
    QueryPerformanceFrequency(&lpFrequency);
    return (unsigned long)((1000000*(lpPerformanceCount.QuadPart)) / lpFrequency.QuadPart);
}
#else    // Linux or Mac
unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return((tv.tv_sec*1000000)+(tv.tv_usec));
}
#endif // _WIN32

/* Seed for the random number generator, which is used for simulating packet loss */
static SKP_int32 rand_seed = 1;

bool silkToPcm(const std::string& silkPath, std::vector<unsigned char>& pcmData, bool& isSilk, std::string* error/* = NULL*/)
{
    pcmData.clear();
    isSilk = false;
    
#ifdef ENABLE_AUDIO_CONVERTION
    unsigned long tottime, starttime;
    size_t    counter;
    SKP_int32 totPackets, i, k;
    SKP_int16 ret, len, tot_len;
    SKP_int16 nBytes;
    SKP_uint8 payload[    MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * ( MAX_LBRR_DELAY + 1 ) ];
    SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
    SKP_uint8 FECpayload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ], *payloadPtr;
    SKP_int16 nBytesFEC;
    SKP_int16 nBytesPerPacket[ MAX_LBRR_DELAY + 1 ], totBytes;
    SKP_int16 out[ ( ( FRAME_LENGTH_MS * MAX_API_FS_KHZ ) << 1 ) * MAX_INPUT_FRAMES ], *outPtr;
    FILE      *bitInFile;
    SKP_int32 packetSize_ms=0, API_Fs_Hz = 0;
    SKP_int32 decSizeBytes;
    void      *psDec;
    SKP_float loss_prob;
    SKP_int32 frames, lost, quiet;
    SKP_SILK_SDK_DecControlStruct DecControl;

    /* default settings */
    loss_prob = 0.0f;

    /* Open files */
#ifdef _WIN32
    CA2W pszW(silkPath.c_str(), CP_UTF8);
    bitInFile = _wfopen((LPCWSTR)pszW, L"rb" );
#else
    bitInFile = fopen(silkPath.c_str(), "rb" );
#endif
    
    std::unique_ptr<FILE, decltype(std::fclose) *> file(bitInFile, std::fclose);
    
    if( bitInFile == NULL )
    {
        if (NULL != error)
        {
            error->assign("Failed to open file: " + silkPath);
        }
        return false;
    }

    /* Check Silk header */
    {
        char header_buf[ 50 ];
        if (fread(header_buf, sizeof(char), 1, bitInFile) != 1)
        {
            fclose(bitInFile);
            if (NULL != error)
            {
                error->assign("Can't read header: " + silkPath);
            }
            return false;
        }
        // header_buf[ strlen( "" ) ] = '\0'; /* Terminate with a null character */
        if( header_buf[0] != 0x02 )
        {
           counter = fread( header_buf, sizeof( char ), strlen( "!SILK_V3" ), bitInFile );
           header_buf[ strlen( "!SILK_V3" ) ] = '\0'; /* Terminate with a null character */
           if( strcmp( header_buf, "!SILK_V3" ) != 0 ) {
               /* Non-equal strings */
               if (NULL != error)
               {
                   error->assign("SILK Error: Wrong Header " + silkPath + ": " + toHex(header_buf, strlen( "!SILK_V3" )));
               }
               // printf( "SILK Error: Wrong Header %s: %s\n", silkPath.c_str(), header_buf );
               // exit( 0 );
               fclose(bitInFile);
               return false;
           }
            else
            {
                isSilk = true;
            }
        }
        else
        {
            counter = fread( header_buf, sizeof( char ), strlen( "#!SILK_V3" ), bitInFile );
            header_buf[ strlen( "#!SILK_V3" ) ] = '\0'; /* Terminate with a null character */
            if( strcmp( header_buf, "#!SILK_V3" ) != 0 ) {
                /* Non-equal strings */
                if (NULL != error)
                {
                    error->assign("SILK Error: Wrong Header " + silkPath + ": " + toHex(header_buf, strlen( "!SILK_V3" )));
                }
                // printf( "SILK Error: Wrong Header %s: %s\n", silkPath.c_str(), header_buf );
                // exit( 0 );
                fclose(bitInFile);
                return false;
            }
            else
            {
                isSilk = true;
            }
        }
    }

    // speechOutFile = fopen( speechOutFileName, "wb" );
    
    /* Set the samplingrate that is requested for the output */
    if( API_Fs_Hz == 0 ) {
        DecControl.API_sampleRate = 24000;
    } else {
        DecControl.API_sampleRate = API_Fs_Hz;
    }

    /* Initialize to one frame per packet, for proper concealment before first packet arrives */
    DecControl.framesPerPacket = 1;

    /* Create decoder */
    ret = SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes );
    if( ret ) {
        // printf( "\nSKP_Silk_SDK_Get_Decoder_Size returned %d", ret );
    }
    std::vector<unsigned char> bufferDec;
    bufferDec.resize(decSizeBytes, 0);
    // psDec = malloc( decSizeBytes );
    psDec = reinterpret_cast<void *>(&(bufferDec[0]));

    /* Reset decoder */
    ret = SKP_Silk_SDK_InitDecoder( psDec );
    if( ret ) {
        // printf( "\nSKP_Silk_InitDecoder returned %d", ret );
    }

    totPackets = 0;
    tottime    = 0;
    payloadEnd = payload;

    /* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
    for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
        /* Read payload size */
        counter = fread( &nBytes, sizeof( SKP_int16 ), 1, bitInFile );
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        /* Read payload */
        counter = fread( payloadEnd, sizeof( SKP_uint8 ), nBytes, bitInFile );

        if( ( SKP_int16 )counter < nBytes ) {
            break;
        }
        nBytesPerPacket[ i ] = nBytes;
        payloadEnd          += nBytes;
        totPackets++;
    }

    while( 1 ) {
        /* Read payload size */
        counter = fread( &nBytes, sizeof( SKP_int16 ), 1, bitInFile );
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        if( nBytes < 0 || counter < 1 ) {
            break;
        }

        /* Read payload */
        counter = fread( payloadEnd, sizeof( SKP_uint8 ), nBytes, bitInFile );
        if( ( SKP_int16 )counter < nBytes ) {
            break;
        }

        /* Simulate losses */
        rand_seed = SKP_RAND( rand_seed );
        if( ( ( ( float )( ( rand_seed >> 16 ) + ( 1 << 15 ) ) ) / 65535.0f >= ( loss_prob / 100.0f ) ) && ( counter > 0 ) ) {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = nBytes;
            payloadEnd                       += nBytes;
        } else {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = 0;
        }

        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No Loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    // printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
                /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                /* Generate 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    // printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        // fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
        unsigned char *p = reinterpret_cast<unsigned char *>(out);
        std::copy(p, p + sizeof( SKP_int16 ) * tot_len, std::back_inserter(pcmData));

        /* Update buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }
        /* Check if the received totBytes is valid */
        if (totBytes < 0 || totBytes > sizeof(payload))
        {
            if (NULL != error)
            {
                *error += "\rPackets decoded:             " + std::to_string(totPackets);
            }
            // fprintf( stderr, "\rPackets decoded:             %d", totPackets );
            return false;
        }
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );
        
    }

    /* Empty the recieve buffer */
    for( k = 0; k < MAX_LBRR_DELAY; k++ ) {
        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr  = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    if (NULL != error)
                    {
                        *error += "\nSKP_Silk_SDK_Decode returned " + std::to_string(ret);
                    }
                    // printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
            /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */

            /* Generate 20 ms */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    if (NULL != error)
                    {
                        *error += "\nSKP_Silk_Decode returned " + std::to_string(ret);
                    }
                    // printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        // fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
        unsigned char *p = reinterpret_cast<unsigned char *>(out);
        std::copy(p, p + sizeof( SKP_int16 ) * tot_len, std::back_inserter(pcmData));

        /* Update Buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }

        /* Check if the received totBytes is valid */
        if (totBytes < 0 || totBytes > sizeof(payload))
        {
            if (NULL != error)
            {
                *error += "\rPackets decoded:              " + std::to_string(totPackets);
            }
            // fprintf( stderr, "\rPackets decoded:              %d", totPackets );
            return false;
        }
        
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );
    }

    /* Free decoder */
    // free( psDec );

    /* Close files */
    // fclose( bitInFile );

    // filetime = totPackets * 1e-3 * packetSize_ms;
    
#endif // ENABLE_AUDIO_CONVERTION
    
    return true;
}

bool silkToPcm(const std::string& silkPath, const std::string& pcmPath, bool& isSilk, std::string* error/* = NULL*/)
{
    std::vector<unsigned char> pcmData;
    bool result = silkToPcm(silkPath, pcmData, isSilk, error);
    if (result)
    {
        result = writeFile(pcmPath, pcmData);
    }
    return result;
}

size_t skipAmrnbHeader(FILE* fp)
{
    const char szFileHeader[] = "#!AMR\n";
    size_t headerLen = strlen(szFileHeader);
    
    unsigned char cData[32];
    size_t bytesRead = fread(cData, (size_t)1, headerLen, fp);
    if (bytesRead < headerLen || strncmp((const char *)cData, szFileHeader, bytesRead) != 0)
    {
        fseek(fp, 0, SEEK_SET);
        return 0;
    }
    
    return headerLen;
}

bool amrToPcm(const std::string& amrPath, std::vector<unsigned char>& pcmData, std::string* error/* = NULL*/)
{
#ifdef _WIN32
    CA2W pszW(amrPath.c_str(), CP_UTF8);
    FILE *fp = _wfopen((LPCWSTR)pszW, L"rb" );
#else
    FILE *fp = fopen(amrPath.c_str(), "rb" );
#endif
    if (NULL == fp)
    {
        return false;
    }

    skipAmrnbHeader(fp);
    
    void *amrnb_dec = Decoder_Interface_init();
    if (NULL == amrnb_dec)
    {
        fclose(fp);
        return false;
    }
        
    uint8_t buffer[500], littleendian[320], *ptr;
    int16_t outbuffer[160];
    size_t n = 0;
    
    while (1)
    {
        int size, i;
        
        /* Read the mode byte */
        n = fread(buffer, 1, 1, fp);
        if (n <= 0)
            break;
        /* Find the packet size */
        size = amr_frame_sizes[(buffer[0] >> 3) & 0x0f];
        
        n = fread(buffer + 1, 1, size, fp);
        if (n != size)
            break;

        /* Decode the packet */
        Decoder_Interface_Decode(amrnb_dec, buffer, outbuffer, 0);

        /* Convert to little endian and write to wav */
        ptr = littleendian;
        for (i = 0; i < 160; i++)
        {
            *ptr++ = (outbuffer[i] >> 0) & 0xff;
            *ptr++ = (outbuffer[i] >> 8) & 0xff;
        }
        pcmData.insert(pcmData.end(), littleendian, littleendian + 320);
    }
        
    Decoder_Interface_exit(amrnb_dec);
    
    fclose(fp);
    
    return true;
}

bool amrToPcm(const std::string& amrPath, const std::string& pcmPath, std::string* error/* = NULL*/)
{
    std::vector<unsigned char> pcmData;
    bool result = amrToPcm(amrPath, pcmData, error);
    if (result)
    {
        result = writeFile(pcmPath, pcmData);
    }
    return result;
}
