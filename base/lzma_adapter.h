/* lzma_adapter.h -- LZMA decompression adapter for SWF ZWS format
   Similar interface to zlib_adapter but handles LZMA (ZWS) compressed SWF files */

#ifndef LZMA_ADAPTER_H
#define LZMA_ADAPTER_H

#include "base/tu_config.h"
class tu_file;

namespace lzma_adapter
{
    /* Create a tu_file that reads from an LZMA-compressed stream.
     *
     * Parameters:
     *   in              - Input file stream (positioned AFTER the 5-byte LZMA properties header)
     *   compressed_size - Number of bytes of LZMA compressed data remaining in 'in'
     *   uncompressed_size - Expected uncompressed size (from SWF file header)
     *
     * Returns:
     *   A new tu_file that reads from the decompressed data, or NULL on failure.
     *   The caller owns the returned tu_file*.
     *   The caller also owns the input tu_file*; don't delete it until you've deleted the returned tu_file.
     */
    tu_file* make_inflater(tu_file* in, int compressed_size, int uncompressed_size);
}

#endif /* LZMA_ADAPTER_H */
