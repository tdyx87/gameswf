/* lzma_adapter.cpp -- LZMA decompression adapter for SWF ZWS format
   Decompresses LZMA data from a tu_file stream into a memory buffer */

#include "base/lzma_adapter.h"
#include "base/lzma/LzmaDec.h"
#include "base/lzma/Alloc.h"
#include "base/tu_file.h"
#include "base/tu_types.h"
#include <string.h>
#include <stdio.h>

namespace lzma_adapter
{
    struct membuf_impl
    {
        unsigned char* m_data;
        int m_size;
        int m_pos;

        membuf_impl(unsigned char* data, int size)
            : m_data(data), m_size(size), m_pos(0)
        {
        }
    };

    static int membuf_read(void* dst, int bytes, void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        int remaining = buf->m_size - buf->m_pos;
        if (bytes > remaining)
            bytes = remaining;
        if (bytes > 0)
        {
            memcpy(dst, buf->m_data + buf->m_pos, bytes);
            buf->m_pos += bytes;
        }
        return bytes;
    }

    static int membuf_write(const void* src, int bytes, void* appdata)
    {
        UNUSED(appdata);
        UNUSED(src);
        UNUSED(bytes);
        return 0;  // read-only
    }

    static int membuf_seek(int pos, void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        if (pos < 0) pos = 0;
        if (pos > buf->m_size) pos = buf->m_size;
        buf->m_pos = pos;
        return buf->m_pos;
    }

    static int membuf_seek_to_end(void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        buf->m_pos = buf->m_size;
        return buf->m_pos;
    }

    static int membuf_tell(const void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        return buf->m_pos;
    }

    static bool membuf_get_eof(void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        return buf->m_pos >= buf->m_size;
    }

    static int membuf_close(void* appdata)
    {
        membuf_impl* buf = (membuf_impl*)appdata;
        delete[] buf->m_data;
        delete buf;
        return 0;
    }

    tu_file* make_inflater(tu_file* in, int compressed_size, int uncompressed_size)
    {
        if (!in || compressed_size <= 0 || uncompressed_size <= 0)
            return NULL;

        /* SWF ZWS format (per SWF spec):
         * After the 12-byte SWF header (sig + version + scriptLen + compressedLen):
         * - 5 bytes: LZMA properties
         * - compressedLen bytes: LZMA compressed data (includes 6-byte end marker)
         *
         * compressed_size = compressedLen + 5 (total bytes to consume from stream).
         * We read 5 bytes properties first, then (compressed_size - 5) bytes of data. */

        /* Read 5 bytes of LZMA properties */
        unsigned char propData[5];
        int read_count = in->read_bytes(propData, 5);
        if (read_count != 5)
        {
            fprintf(stderr, "lzma_adapter: failed to read LZMA properties\n");
            return NULL;
        }

        /* Remaining compressed data size */
        int remaining_compressed = compressed_size - 5;
        if (remaining_compressed <= 0)
        {
            fprintf(stderr, "lzma_adapter: no compressed data remaining\n");
            return NULL;
        }

        /* Read all compressed data into memory */
        unsigned char* compressed_buf = new unsigned char[remaining_compressed];
        int total_read = 0;
        while (total_read < remaining_compressed)
        {
            int n = in->read_bytes(compressed_buf + total_read, remaining_compressed - total_read);
            if (n <= 0) break;
            total_read += n;
        }

        if (total_read != remaining_compressed)
        {
            fprintf(stderr, "lzma_adapter: read %d of %d compressed bytes\n", total_read, remaining_compressed);
            delete[] compressed_buf;
            return NULL;
        }

        /* Allocate output buffer for decompressed data */
        unsigned char* decompressed_buf = new unsigned char[uncompressed_size];

        /* Decompress using LZMA SDK */
        SizeT srcLen = (SizeT)total_read;
        SizeT destLen = (SizeT)uncompressed_size;
        ELzmaStatus status;
        SRes result;

        result = LzmaDecode(
            decompressed_buf,
            &destLen,
            compressed_buf,
            &srcLen,
            propData,
            5,
            LZMA_FINISH_ANY,
            &status,
            &g_Alloc
        );

        delete[] compressed_buf;

        if (result != SZ_OK)
        {
            fprintf(stderr, "lzma_adapter: LZMA decode failed with error %d\n", result);
            delete[] decompressed_buf;
            return NULL;
        }

        fprintf(stderr, "lzma_adapter: decompressed %d -> %d bytes (status=%d)\n",
                (int)srcLen, (int)destLen, (int)status);

        /* Create a memory-backed tu_file from the decompressed data */
        membuf_impl* membuf = new membuf_impl(decompressed_buf, (int)destLen);

        return new tu_file(
            membuf,
            membuf_read,
            membuf_write,
            membuf_seek,
            membuf_seek_to_end,
            membuf_tell,
            membuf_get_eof,
            membuf_close
        );
    }
}
