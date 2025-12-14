// post.wgsl - PNG encoder compute shader
//
// Encodes float32 RGBA pixels to PNG format entirely on GPU.
// This is the ONLY place in the pipeline where fidelity loss occurs.
//
// Version: v1 - Uncompressed PNG (stored DEFLATE blocks)
// - Valid PNG format
// - No compression (larger files but fast)
// - Filter: None (0x00 prefix per row)

// ============================================================
// Bindings
// ============================================================

struct Params {
    width: u32,
    height: u32,
    row_bytes: u32,     // width * 3 (RGB) + 1 (filter byte)
    idat_size: u32,     // Total IDAT data size
}

@group(0) @binding(0) var<storage, read> pixels: array<vec4f>;
@group(0) @binding(1) var<storage, read_write> output: array<u32>;
@group(0) @binding(2) var<uniform> params: Params;

// ============================================================
// CRC32 (used for PNG chunks)
// ============================================================

// CRC32 polynomial: 0xEDB88320 (reversed)
fn crc32_byte(crc: u32, byte: u32) -> u32 {
    var c = crc ^ byte;
    for (var i = 0u; i < 8u; i++) {
        if ((c & 1u) != 0u) {
            c = (c >> 1u) ^ 0xEDB88320u;
        } else {
            c = c >> 1u;
        }
    }
    return c;
}

fn crc32_update(crc: u32, data: u32) -> u32 {
    var c = crc;
    c = crc32_byte(c, data & 0xFFu);
    c = crc32_byte(c, (data >> 8u) & 0xFFu);
    c = crc32_byte(c, (data >> 16u) & 0xFFu);
    c = crc32_byte(c, (data >> 24u) & 0xFFu);
    return c;
}

// ============================================================
// Adler32 (used for zlib wrapper)
// ============================================================

fn adler32_update(adler: vec2u, byte: u32) -> vec2u {
    let a = (adler.x + byte) % 65521u;
    let b = (adler.y + a) % 65521u;
    return vec2u(a, b);
}

// ============================================================
// Helper: Write byte to output buffer
// ============================================================

fn write_byte(offset: u32, byte: u32) {
    let word_idx = offset / 4u;
    let byte_idx = offset % 4u;
    let shift = byte_idx * 8u;
    let mask = ~(0xFFu << shift);

    // Atomic OR to handle concurrent writes from different threads
    // Note: In practice, we ensure non-overlapping writes
    let old = output[word_idx];
    output[word_idx] = (old & mask) | ((byte & 0xFFu) << shift);
}

fn write_u32_be(offset: u32, value: u32) {
    write_byte(offset, (value >> 24u) & 0xFFu);
    write_byte(offset + 1u, (value >> 16u) & 0xFFu);
    write_byte(offset + 2u, (value >> 8u) & 0xFFu);
    write_byte(offset + 3u, value & 0xFFu);
}

fn write_u16_le(offset: u32, value: u32) {
    write_byte(offset, value & 0xFFu);
    write_byte(offset + 1u, (value >> 8u) & 0xFFu);
}

// ============================================================
// PNG Header (runs once)
// ============================================================

@compute @workgroup_size(1)
fn write_header() {
    // PNG Signature (8 bytes)
    // 89 50 4E 47 0D 0A 1A 0A
    write_byte(0u, 0x89u);
    write_byte(1u, 0x50u);  // P
    write_byte(2u, 0x4Eu);  // N
    write_byte(3u, 0x47u);  // G
    write_byte(4u, 0x0Du);  // CR
    write_byte(5u, 0x0Au);  // LF
    write_byte(6u, 0x1Au);  // EOF
    write_byte(7u, 0x0Au);  // LF

    // IHDR chunk (25 bytes total)
    // Length: 13 (0x0000000D)
    write_u32_be(8u, 13u);

    // Type: "IHDR"
    write_byte(12u, 0x49u);  // I
    write_byte(13u, 0x48u);  // H
    write_byte(14u, 0x44u);  // D
    write_byte(15u, 0x52u);  // R

    // Width (4 bytes BE)
    write_u32_be(16u, params.width);

    // Height (4 bytes BE)
    write_u32_be(20u, params.height);

    // Bit depth: 8
    write_byte(24u, 8u);

    // Color type: 2 (RGB)
    write_byte(25u, 2u);

    // Compression: 0 (deflate)
    write_byte(26u, 0u);

    // Filter method: 0
    write_byte(27u, 0u);

    // Interlace: 0 (none)
    write_byte(28u, 0u);

    // CRC32 of IHDR (type + data)
    var crc = 0xFFFFFFFFu;
    crc = crc32_byte(crc, 0x49u);  // I
    crc = crc32_byte(crc, 0x48u);  // H
    crc = crc32_byte(crc, 0x44u);  // D
    crc = crc32_byte(crc, 0x52u);  // R
    crc = crc32_byte(crc, (params.width >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (params.width >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (params.width >> 8u) & 0xFFu);
    crc = crc32_byte(crc, params.width & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 8u) & 0xFFu);
    crc = crc32_byte(crc, params.height & 0xFFu);
    crc = crc32_byte(crc, 8u);   // bit depth
    crc = crc32_byte(crc, 2u);   // color type
    crc = crc32_byte(crc, 0u);   // compression
    crc = crc32_byte(crc, 0u);   // filter
    crc = crc32_byte(crc, 0u);   // interlace
    crc = crc ^ 0xFFFFFFFFu;
    write_u32_be(29u, crc);

    // IDAT chunk header (offset 33)
    // Length of compressed data
    write_u32_be(33u, params.idat_size);

    // Type: "IDAT"
    write_byte(37u, 0x49u);  // I
    write_byte(38u, 0x44u);  // D
    write_byte(39u, 0x41u);  // A
    write_byte(40u, 0x54u);  // T

    // zlib header (2 bytes)
    // CMF: 0x78 (deflate, 32K window)
    // FLG: 0x01 (no dict, check bits for CMF*256+FLG divisible by 31)
    write_byte(41u, 0x78u);
    write_byte(42u, 0x01u);
}

// ============================================================
// Encode one row of pixels (parallel)
// ============================================================

@compute @workgroup_size(256)
fn encode_rows(@builtin(global_invocation_id) gid: vec3u) {
    let row = gid.x;
    if (row >= params.height) {
        return;
    }

    // Calculate output offset for this row's DEFLATE block
    // Header: 33 (PNG sig + IHDR) + 8 (IDAT header) + 2 (zlib header) = 43
    // Each row is a stored DEFLATE block:
    //   1 byte: block header (0x00 for non-final, 0x01 for final)
    //   2 bytes: LEN (little-endian)
    //   2 bytes: NLEN (one's complement of LEN)
    //   N bytes: data (filter byte + RGB pixels)

    let block_header_size = 5u;
    let row_data_size = params.row_bytes;  // 1 + width*3
    let block_size = block_header_size + row_data_size;

    let base_offset = 43u + row * block_size;

    // Block header
    let is_final = (row == params.height - 1u);
    write_byte(base_offset, select(0x00u, 0x01u, is_final));

    // LEN and NLEN
    write_u16_le(base_offset + 1u, row_data_size);
    write_u16_le(base_offset + 3u, row_data_size ^ 0xFFFFu);

    // Filter byte (0 = None)
    write_byte(base_offset + 5u, 0u);

    // RGB pixels (quantize float32 -> uint8)
    for (var x = 0u; x < params.width; x++) {
        let pixel_idx = row * params.width + x;
        let pixel = pixels[pixel_idx];

        // Clamp and quantize to 8-bit
        let r = u32(clamp(pixel.r, 0.0, 1.0) * 255.0 + 0.5);
        let g = u32(clamp(pixel.g, 0.0, 1.0) * 255.0 + 0.5);
        let b = u32(clamp(pixel.b, 0.0, 1.0) * 255.0 + 0.5);

        let byte_offset = base_offset + 6u + x * 3u;
        write_byte(byte_offset, r);
        write_byte(byte_offset + 1u, g);
        write_byte(byte_offset + 2u, b);
    }
}

// ============================================================
// Write footer (IDAT CRC, Adler32, IEND)
// ============================================================

// Note: This needs to run AFTER encode_rows completes
// Computes Adler32 over all row data and IDAT CRC
@compute @workgroup_size(1)
fn write_footer() {
    // Calculate Adler32 over all filtered pixel data
    var adler = vec2u(1u, 0u);  // (s1, s2)

    let data_start = 43u;  // After zlib header
    let block_header_size = 5u;
    let row_data_size = params.row_bytes;
    let block_size = block_header_size + row_data_size;

    for (var row = 0u; row < params.height; row++) {
        let row_offset = data_start + row * block_size + block_header_size;

        for (var i = 0u; i < row_data_size; i++) {
            let byte_offset = row_offset + i;
            let word_idx = byte_offset / 4u;
            let byte_idx = byte_offset % 4u;
            let byte_val = (output[word_idx] >> (byte_idx * 8u)) & 0xFFu;
            adler = adler32_update(adler, byte_val);
        }
    }

    // Write Adler32 (big-endian: s2 << 16 | s1)
    let adler32_offset = 43u + params.height * block_size;
    let adler32_val = (adler.y << 16u) | adler.x;
    write_u32_be(adler32_offset, adler32_val);

    // Calculate and write IDAT CRC (over "IDAT" + compressed data + adler32)
    var crc = 0xFFFFFFFFu;
    crc = crc32_byte(crc, 0x49u);  // I
    crc = crc32_byte(crc, 0x44u);  // D
    crc = crc32_byte(crc, 0x41u);  // A
    crc = crc32_byte(crc, 0x54u);  // T

    // CRC over zlib header
    crc = crc32_byte(crc, 0x78u);
    crc = crc32_byte(crc, 0x01u);

    // CRC over DEFLATE blocks
    for (var row = 0u; row < params.height; row++) {
        let block_offset = data_start + row * block_size;

        for (var i = 0u; i < block_size; i++) {
            let byte_offset = block_offset + i;
            let word_idx = byte_offset / 4u;
            let byte_idx = byte_offset % 4u;
            let byte_val = (output[word_idx] >> (byte_idx * 8u)) & 0xFFu;
            crc = crc32_byte(crc, byte_val);
        }
    }

    // CRC over Adler32
    crc = crc32_byte(crc, (adler32_val >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (adler32_val >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (adler32_val >> 8u) & 0xFFu);
    crc = crc32_byte(crc, adler32_val & 0xFFu);

    crc = crc ^ 0xFFFFFFFFu;
    write_u32_be(adler32_offset + 4u, crc);

    // IEND chunk (12 bytes)
    let iend_offset = adler32_offset + 8u;

    // Length: 0
    write_u32_be(iend_offset, 0u);

    // Type: "IEND"
    write_byte(iend_offset + 4u, 0x49u);  // I
    write_byte(iend_offset + 5u, 0x45u);  // E
    write_byte(iend_offset + 6u, 0x4Eu);  // N
    write_byte(iend_offset + 7u, 0x44u);  // D

    // CRC32 of "IEND"
    var iend_crc = 0xFFFFFFFFu;
    iend_crc = crc32_byte(iend_crc, 0x49u);
    iend_crc = crc32_byte(iend_crc, 0x45u);
    iend_crc = crc32_byte(iend_crc, 0x4Eu);
    iend_crc = crc32_byte(iend_crc, 0x44u);
    iend_crc = iend_crc ^ 0xFFFFFFFFu;
    write_u32_be(iend_offset + 8u, iend_crc);
}
