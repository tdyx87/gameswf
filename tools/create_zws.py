#!/usr/bin/env python3
"""Create a ZWS (LZMA-compressed) SWF file from an existing FWS SWF for testing."""
import struct
import sys
import lzma

def create_zws(input_path, output_path):
    with open(input_path, 'rb') as f:
        data = f.read()

    # Verify FWS header
    sig = data[0:3]
    if sig != b'FWS':
        print(f"Not an FWS file, signature: {sig}")
        return False

    version = data[3]
    file_length = struct.unpack('<I', data[4:8])[0]
    body = data[8:]  # Everything after the 8-byte header

    print(f"Input: {input_path}")
    print(f"  Signature: FWS, Version: {version}")
    print(f"  File length: {file_length}")
    print(f"  Body size: {len(body)} bytes")

    # LZMA compress the body
    # LZMA properties: lc=3, lp=0, pb=2 (default), dicSize= min(1<<16, len)
    dic_size = min(1 << 16, len(body))

    lzma_props = struct.pack('BBBB', 9 * 0 + 0 * 5 + 2, 0, 0, 0)  # lc=0, lp=0, pb=2
    # Actually: properties byte = lc + lp*9 + pb*45
    prop_byte = 9 * 3 + 0 * 5 + 2  # lc=3, lp=0, pb=2 => 27+0+2=29
    lzma_props = struct.pack('B', prop_byte) + struct.pack('<I', dic_size)

    print(f"  LZMA props byte: {prop_byte} (lc=3, lp=0, pb=2)")
    print(f"  LZMA dict size: {dic_size}")

    # Create LZMA compressed data using LZMA alone (not xz/lzma2)
    # lzma.compress with FORMAT_ALONE creates LZMA format (5 byte props + data)
    try:
        compressed_full = lzma.compress(body, format=lzma.FORMAT_ALONE,
            filters=[{'id': lzma.FILTER_LZMA1, 'lc': 3, 'lp': 0, 'pb': 2, 'dict_size': dic_size}])
    except Exception as e:
        print(f"LZMA compress with FILTER_LZMA1 failed: {e}")
        try:
            compressed_full = lzma.compress(body, format=lzma.FORMAT_ALONE)
        except Exception as e2:
            print(f"LZMA compress FORMAT_ALONE failed: {e2}")
            return False

    # FORMAT_ALONE output: 5 bytes properties + 8 bytes size + compressed data
    # But SWF ZWS expects: 5 bytes properties + raw LZMA compressed data (no size field)
    lzma_props = compressed_full[0:5]
    # Skip the 8-byte uncompressed size field (bytes 5-12) if present
    if compressed_full[5:13] == b'\xff' * 8:
        compressed_body = compressed_full[13:]  # skip 8-byte unknown size marker
    else:
        compressed_body = compressed_full[5:]   # size is present, keep everything after props
    print(f"  FORMAT_ALONE size: {len(compressed_full)} bytes")
    print(f"  LZMA props: {lzma_props.hex()}")
    print(f"  Compressed body: {len(compressed_body)} bytes")

    print(f"  Compressed body size: {len(compressed_body)} bytes")

    # Build ZWS file
    # ZWS format:
    # Bytes 0-2: 'ZWS' signature
    # Byte 3: Version
    # Bytes 4-7: File length (uncompressed, same as original)
    # Bytes 8-11: Compressed length (size of remaining data after this field)
    # Then: 4 bytes compressed format version (usually 0x20)
    # Then: 5 bytes LZMA properties
    # Then: LZMA compressed data

    compressed_format_version = 0x00000020
    compressed_length = 4 + 5 + len(compressed_body)  # version + props + data

    zws_header = b'ZWS'
    zws_header += struct.pack('B', version)
    zws_header += struct.pack('<I', file_length)
    zws_header += struct.pack('<I', compressed_length)

    zws_body = struct.pack('<I', compressed_format_version)
    zws_body += lzma_props
    zws_body += compressed_body

    zws_data = zws_header + zws_body

    with open(output_path, 'wb') as f:
        f.write(zws_data)

    print(f"\nOutput: {output_path}")
    print(f"  Total size: {len(zws_data)} bytes")
    print(f"  ZWS header: {len(zws_header)} bytes")
    print(f"  ZWS body: {len(zws_body)} bytes")
    print(f"  Signature: {zws_data[0:3]}")

    # Verify
    with open(output_path, 'rb') as f:
        verify = f.read(3)
        print(f"\nVerification: signature = {verify}")
        if verify == b'ZWS':
            print("ZWS file created successfully!")
            return True
        else:
            print("ERROR: Bad signature!")
            return False


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_fws.swf> <output_zws.swf>")
        sys.exit(1)

    success = create_zws(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
