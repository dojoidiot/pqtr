#include "sony_decoder.hpp"
#include "../core/cameras.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <memory>

namespace copy::io {

namespace {

    // --- BitStreamer Helpers ---

    inline uint32_t extractLowBits(uint32_t value, unsigned nBits) {
        unsigned numHighPaddingBits = 32 - nBits;
        value <<= numHighPaddingBits;
        value >>= numHighPaddingBits;
        return value;
    }

    struct BitStreamCache {
        uint64_t cache = 0;
        int fillLevel = 0;
    };

    struct BitStreamerLSB {
        const uint8_t* data;
        int size;
        int pos;
        BitStreamCache cache;

        BitStreamerLSB(const uint8_t* d, int s) : data(d), size(s), pos(0) {}

        void fill(int nbits) {
            if (cache.fillLevel >= nbits) return;
            while (cache.fillLevel < nbits && pos + 4 <= size) {
                uint32_t chunk = data[pos] |
                                ((uint32_t)data[pos + 1] << 8) |
                                ((uint32_t)data[pos + 2] << 16) |
                                ((uint32_t)data[pos + 3] << 24);
                cache.cache |= (uint64_t)chunk << cache.fillLevel;
                cache.fillLevel += 32;
                pos += 4;
            }
        }

        uint32_t peekBits(int nbits) {
            fill(nbits);
            return extractLowBits((uint32_t)cache.cache, nbits);
        }

        uint32_t getBits(int nbits) {
            fill(nbits);
            uint32_t ret = extractLowBits((uint32_t)cache.cache, nbits);
            cache.cache >>= nbits;
            cache.fillLevel -= nbits;
            return ret;
        }
    };

    // --- TableLookUp Helpers ---

    #define TABLE_MAX_ELTS 65536
    #define TABLE_SIZE (TABLE_MAX_ELTS * 2)

    struct TableLookUp {
        std::vector<uint16_t> tables;
        int dither;

        TableLookUp(int n, int d) : tables(n * TABLE_SIZE), dither(d) {}

        void setTable(int ntable, const uint16_t* table, int nfilled) {
            uint16_t* t = &tables[ntable * TABLE_SIZE];
            if (!dither) {
                for (int i = 0; i < TABLE_MAX_ELTS; i++) {
                    t[i] = (i < nfilled) ? table[i] : table[nfilled - 1];
                }
                return;
            }

            auto clampBits = [](int value, int bits) {
                int max_val = (1 << bits) - 1;
                if (value < 0) return 0;
                if (value > max_val) return max_val;
                return value;
            };

            for (int i = 0; i < nfilled; i++) {
                int center = table[i];
                int lower = i > 0 ? table[i - 1] : center;
                int upper = i < (nfilled - 1) ? table[i + 1] : center;
                if (lower > center) lower = center;
                if (upper < center) upper = center;
                int delta = upper - lower;
                t[i * 2] = (uint16_t)clampBits(center - ((upper - lower + 2) / 4), 16);
                t[(i * 2) + 1] = (uint16_t)delta;
            }

            for (int i = nfilled; i < TABLE_MAX_ELTS; i++) {
                t[i * 2] = table[nfilled - 1];
                t[(i * 2) + 1] = 0;
            }
        }
    };

    inline void setWithLookUp(const TableLookUp& table, uint16_t value, uint16_t* dest, uint32_t* random) {
        if (table.dither) {
            uint32_t base = table.tables[(2 * value) + 0];
            uint32_t delta = table.tables[(2 * value) + 1];
            uint32_t r = *random;
            uint32_t pix = base + ((delta * (r & 2047) + 1024) >> 12);
            *random = 15700 * (r & 65535) + (r >> 16);
            *dest = (uint16_t)pix;
        } else {
            *dest = table.tables[value];
        }
    }

    // --- Decoder Logic ---

    void decodeCurve(const uint16_t* sony_curve_raw, uint16_t* curve) {
        uint32_t sony_curve[6] = {0, 0, 0, 0, 0, 4095};
        for (uint32_t i = 0; i < 4; i++)
            sony_curve[i + 1] = (sony_curve_raw[i] >> 2) & 0xfff;

        for (uint32_t i = 0; i < 0x4001; i++)
            curve[i] = (uint16_t)i;

        for (uint32_t i = 0; i < 5; i++)
            for (uint32_t j = sony_curve[i] + 1; j <= sony_curve[i + 1]; j++)
                curve[j] = (uint16_t)(curve[j - 1] + (1 << i));
    }

    void decompressRow(const uint8_t* input, int width, int row, uint16_t* output, const TableLookUp& table) {
        const uint8_t* rowData = input + row * width;
        BitStreamerLSB bits(rowData, width);

        uint32_t random = bits.peekBits(24);

        for (int col = 0; col < width; col += ((col & 1) != 0) ? 31 : 1) {
            int _max = bits.getBits(11);
            int _min = bits.getBits(11);
            int _imax = bits.getBits(4);
            int _imin = bits.getBits(4);

            if (_imax == _imin) return; // Invariant failed

            int sh = 0;
            while ((sh < 4) && ((0x80 << sh) <= (_max - _min))) sh++;

            for (int i = 0; i < 16; i++) {
                int p;
                if (i == _imax) p = _max;
                else if (i == _imin) p = _min;
                else {
                    p = (bits.getBits(7) << sh) + _min;
                    if (p > 0x7ff) p = 0x7ff;
                }
                setWithLookUp(table, (uint16_t)(p << 1), &output[row * width + col + (i * 2)], &random);
            }
        }
    }

    // --- Metadata Parsing Helpers ---

    inline uint16_t meta_read_u16(const uint8_t* p) {
        return p[0] | ((uint16_t)p[1] << 8);
    }
    inline uint32_t meta_read_u32(const uint8_t* p) {
        return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    struct IFDEntry {
        uint16_t tag;
        uint16_t type;
        uint32_t count;
        uint32_t value_offset;
    };

    IFDEntry parse_ifd_entry(const uint8_t* data) {
        IFDEntry e;
        e.tag = meta_read_u16(data);
        e.type = meta_read_u16(data + 2);
        e.count = meta_read_u32(data + 4);
        e.value_offset = meta_read_u32(data + 8);
        return e;
    }

    void map_picture_profile(core::PictureProfile& p) {
        if (p.picture_profile != 0) {
            switch (p.picture_profile) {
                case 8:  p.saturation = 1.0f; p.vibrance = 0.50f; p.contrast = 0.10f; return;
                case 1:  p.saturation = 0.0f; p.vibrance = 0.10f; p.contrast = 0.0f; return;
                case 2:
                case 3:  p.saturation = -0.10f; p.vibrance = 0.0f; p.contrast = 0.0f; return;
                default: p.saturation = 0.0f; p.vibrance = 0.0f; p.contrast = 0.0f; return;
            }
        }
        switch (p.creative_style) {
            case 1: p.saturation = 0.25f; p.vibrance = 0.20f; p.contrast = 0.10f; break;
            case 2: p.saturation = -0.15f; p.vibrance = 0.0f; p.contrast = -0.10f; break;
            case 3: p.saturation = 0.10f; p.vibrance = 0.15f; p.contrast = 0.05f; break;
            case 4: p.saturation = 0.15f; p.vibrance = 0.10f; p.contrast = 0.15f; break;
            case 5: p.saturation = 0.05f; p.vibrance = 0.10f; p.contrast = -0.05f; break;
            case 6: p.saturation = 0.0f; p.vibrance = 0.10f; p.contrast = 0.0f; break;
            case 7: p.saturation = 0.20f; p.vibrance = 0.15f; p.contrast = 0.10f; break;
            default: p.saturation = 0.0f; p.vibrance = 0.0f; p.contrast = 0.0f; break;
        }
    }

} // anonymous namespace

    void SonyDecoder::decrypt(uint8_t* data, uint32_t length, uint32_t key) {
        uint32_t pad[128] = {0};
        uint32_t p;

        for (p = 0; p < 4; p++)
            pad[p] = key = key * 48828125 + 1;
        pad[3] = pad[3] << 1 | (pad[0] ^ pad[2]) >> 31;
        for (p = 4; p < 127; p++)
            pad[p] = (pad[p-4] ^ pad[p-2]) << 1 | (pad[p-3] ^ pad[p-1]) >> 31;
        for (p = 0; p < 127; p++)
            pad[p] = ((pad[p] & 0xff) << 24) | ((pad[p] & 0xff00) << 8) |
                     ((pad[p] >> 8) & 0xff00) | ((pad[p] >> 24) & 0xff);

        uint32_t* d = (uint32_t*)data;
        p = 127;
        for (uint32_t i = 0; i < length / 4; i++) {
            p++;
            d[i] ^= pad[(p-1) & 127] = pad[p & 127] ^ pad[(p+64) & 127];
        }
    }

    bool SonyDecoder::read_meta(const std::string& filename, core::MetaData& meta) {
        FILE* f = fopen(filename.c_str(), "rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        size_t read_size = (file_size < 1024*1024) ? file_size : 1024*1024;
        std::vector<uint8_t> data(read_size);
        if (fread(data.data(), 1, read_size, f) != read_size) {
            fclose(f);
            return false;
        }
        fclose(f);

        if (read_size < 8 || data[0] != 'I' || data[1] != 'I' || data[2] != 0x2a) return false;

        uint32_t ifd0_offset = meta_read_u32(data.data() + 4);
        if (ifd0_offset + 2 > read_size) return false;

        uint16_t nentries = meta_read_u16(data.data() + ifd0_offset);
        uint32_t offset = ifd0_offset + 2;

        uint32_t sub_ifd_offset = 0;
        uint32_t exif_ifd_offset = 0;
        uint32_t sr2_offset = 0, sr2_length = 0, sr2_key = 0;

        for (int i = 0; i < nentries; i++) {
            if (offset + 12 > read_size) break;
            IFDEntry entry = parse_ifd_entry(data.data() + offset);

            if (entry.tag == 0xc634 && entry.value_offset + 100 <= read_size) {
                uint16_t sr2_num = meta_read_u16(data.data() + entry.value_offset);
                for (int j = 0; j < sr2_num && j < 20; j++) {
                    uint32_t sr2_entry_off = entry.value_offset + 2 + j * 12;
                    if (sr2_entry_off + 12 > read_size) break;
                    IFDEntry se = parse_ifd_entry(data.data() + sr2_entry_off);
                    if (se.tag == 0x7200) sr2_offset = se.value_offset;
                    if (se.tag == 0x7201) sr2_length = se.value_offset;
                    if (se.tag == 0x7221) sr2_key = se.value_offset;
                }
            }

            if (entry.tag == 330) sub_ifd_offset = entry.value_offset;
            else if (entry.tag == 34665) exif_ifd_offset = entry.value_offset;
            else if (entry.tag == 0x0201) meta.preview_offset = entry.value_offset;
            else if (entry.tag == 0x0202) meta.preview_length = entry.value_offset;

            offset += 12;
        }

        const auto* cam = core::cameras_lookup("Sony", "ILCE-7M3");
        meta.camera = cam;
        if (cam) {
            std::memcpy(meta.xyz_to_cam, cam->xyz_to_cam, sizeof(meta.xyz_to_cam));
            meta.black_level = cam->black_level;
            meta.white_level = cam->white_level;
            meta.filters = cam->filters;
            core::cameras_compute_d65(cam->xyz_to_cam, meta.d65_coeffs);
        }

        // Parse EXIF IFD
        uint32_t maker_note_offset = 0;
        if (exif_ifd_offset > 0 && exif_ifd_offset + 2 <= read_size) {
            uint16_t exif_nentries = meta_read_u16(data.data() + exif_ifd_offset);
            offset = exif_ifd_offset + 2;
            for (int i = 0; i < exif_nentries; i++) {
                if (offset + 12 > read_size) break;
                IFDEntry entry = parse_ifd_entry(data.data() + offset);
                if (entry.tag == 37500) maker_note_offset = entry.value_offset;
                if (entry.tag == 34855) {
                    meta.iso = (entry.type == 3) ? (entry.value_offset & 0xFFFF) : entry.value_offset;
                }
                offset += 12;
            }
        }

        // Parse Sony MakerNotes
        bool found_sony_curve = false;
        if (maker_note_offset > 0 && maker_note_offset + 12 <= read_size) {
            uint32_t maker_ifd_offset = maker_note_offset;
            if (data[maker_note_offset] == 'S' && data[maker_note_offset + 1] == 'O')
                maker_ifd_offset += 12;

            if (maker_ifd_offset + 2 <= read_size) {
                uint16_t maker_nentries = meta_read_u16(data.data() + maker_ifd_offset);
                uint32_t sony_tag2010_offset = 0;
                uint32_t sony_tag2010_count = 0;

                for (int i = 0; i < maker_nentries && i < 200; i++) {
                    uint32_t entry_off = maker_ifd_offset + 2 + i * 12;
                    if (entry_off + 12 > read_size) break;
                    IFDEntry entry = parse_ifd_entry(data.data() + entry_off);

                    if (entry.tag == 0x2010) {
                        sony_tag2010_offset = entry.value_offset;
                        sony_tag2010_count = entry.count;
                    }
                    if (entry.tag == 0xb020 && entry.count >= 1 && entry.value_offset + entry.count <= read_size) {
                        // Creative Style
                        std::string style((const char*)(data.data() + entry.value_offset), std::min((uint32_t)19, entry.count));
                        if (style.find("Vivid") != std::string::npos) meta.profile.creative_style = 1;
                        else if (style.find("Neutral") != std::string::npos) meta.profile.creative_style = 2;
                        else if (style.find("Clear") != std::string::npos) meta.profile.creative_style = 3;
                        else if (style.find("Deep") != std::string::npos) meta.profile.creative_style = 4;
                        else if (style.find("Light") != std::string::npos) meta.profile.creative_style = 5;
                        else if (style.find("Portrait") != std::string::npos) meta.profile.creative_style = 6;
                        else if (style.find("Landscape") != std::string::npos) meta.profile.creative_style = 7;
                        else meta.profile.creative_style = 0;
                    }
                }

                if (sony_tag2010_offset > 0 && sony_tag2010_count > 4 && sony_tag2010_offset + sony_tag2010_count <= read_size) {
                    std::vector<uint8_t> tag2010(sony_tag2010_count);
                    std::memcpy(tag2010.data(), data.data() + sony_tag2010_offset, sony_tag2010_count);
                    uint32_t key = meta_read_u32(tag2010.data());
                    decrypt(tag2010.data() + 4, sony_tag2010_count - 4, key);

                    uint8_t* dec = tag2010.data() + 4;
                    uint32_t dec_len = sony_tag2010_count - 4;
                    if (dec_len >= 2) {
                        uint16_t t2010_nentries = meta_read_u16(dec);
                        for (int i = 0; i < t2010_nentries; i++) {
                            uint32_t entry_off = 2 + i * 12;
                            if (entry_off + 12 > dec_len) break;
                            IFDEntry entry = parse_ifd_entry(dec + entry_off);
                            if (entry.tag == 0x7010 && entry.count >= 4 && entry.value_offset + 8 <= dec_len) {
                                for(int j=0; j<4; j++) meta.sony_curve[j] = meta_read_u16(dec + entry.value_offset + j * 2);
                                found_sony_curve = true;
                            }
                        }
                    }
                }
            }
        }

        // Parse SubIFD
        if (sub_ifd_offset > 0 && sub_ifd_offset + 2 <= read_size) {
            uint16_t sub_nentries = meta_read_u16(data.data() + sub_ifd_offset);
            offset = sub_ifd_offset + 2;
            bool found_wb = false;

            for (int i = 0; i < sub_nentries; i++) {
                if (offset + 12 > read_size) break;
                IFDEntry entry = parse_ifd_entry(data.data() + offset);

                if (!found_sony_curve && entry.tag == 0x7010 && entry.count >= 4 && entry.value_offset + 8 <= read_size) {
                    for(int j=0; j<4; j++) meta.sony_curve[j] = meta_read_u16(data.data() + entry.value_offset + j * 2);
                    found_sony_curve = true;
                }

                if (!found_wb && entry.tag == 0x7313 && entry.count == 4 && entry.value_offset + 8 <= read_size) {
                    uint16_t r = meta_read_u16(data.data() + entry.value_offset);
                    uint16_t g1 = meta_read_u16(data.data() + entry.value_offset + 2);
                    uint16_t g2 = meta_read_u16(data.data() + entry.value_offset + 4);
                    uint16_t b = meta_read_u16(data.data() + entry.value_offset + 6);
                    if (g1 > 0) {
                        meta.wb_rggb[0] = (float)r / g1;
                        meta.wb_rggb[1] = 1.0f;
                        meta.wb_rggb[2] = (float)b / g1;
                        meta.wb_rggb[3] = (float)g2 / g1;
                    }
                    found_wb = true;
                }

                if (entry.tag == 0x7800 && entry.count == 9 && entry.value_offset + 18 <= read_size) {
                    for (int j = 0; j < 9; j++) {
                        meta.color_matrix[j] = (int16_t)meta_read_u16(data.data() + entry.value_offset + j * 2) / 1024.0f;
                    }
                }

                if (entry.tag == 256) meta.width = (entry.type == 3) ? (entry.value_offset & 0xFFFF) : entry.value_offset;
                else if (entry.tag == 257) meta.height = (entry.type == 3) ? (entry.value_offset & 0xFFFF) : entry.value_offset;
                else if (entry.tag == 273) meta.strip_offset = entry.value_offset;

                offset += 12;
            }
        }

        // SR2 for black level
        if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 && sr2_offset + sr2_length <= read_size) {
            std::vector<uint8_t> sr2(sr2_length);
            std::memcpy(sr2.data(), data.data() + sr2_offset, sr2_length);
            decrypt(sr2.data(), sr2_length, sr2_key);

            if (sr2_length >= 2) {
                uint16_t sr2_nentries = meta_read_u16(sr2.data());
                for (int i = 0; i < sr2_nentries && i < 200; i++) {
                    uint32_t entry_off = 2 + i * 12;
                    if (entry_off + 12 > sr2_length) break;
                    IFDEntry entry = parse_ifd_entry(sr2.data() + entry_off);

                    if (entry.tag == 0x7310 && entry.count == 4) {
                        uint32_t rel = entry.value_offset - sr2_offset;
                        if (rel + 8 <= sr2_length) {
                            uint16_t min_black = 65535;
                            for (int j = 0; j < 4; j++) {
                                uint16_t bl = meta_read_u16(sr2.data() + rel + j * 2);
                                if (bl < min_black) min_black = bl;
                            }
                            meta.black_level = min_black;
                        }
                    }
                    if (entry.tag == 0x7800 && entry.count == 9 && meta.color_matrix[0] == 0) {
                        uint32_t rel = entry.value_offset - sr2_offset;
                        if (rel + 18 <= sr2_length) {
                            for (int j = 0; j < 9; j++) {
                                meta.color_matrix[j] = (int16_t)meta_read_u16(sr2.data() + rel + j * 2) / 1024.0f;
                            }
                        }
                    }
                }
            }
        }

        // External ExifTool calls for encrypted tags
        {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "exiftool -n -PictureProfile \"%s\" 2>/dev/null | grep -oE '[0-9]+'", filename.c_str());
            FILE* pp = popen(cmd, "r");
            if (pp) {
                int val = 0;
                if (fscanf(pp, "%d", &val) == 1) meta.profile.picture_profile = val;
                pclose(pp);
            }

            snprintf(cmd, sizeof(cmd), "exiftool -n -DynamicRangeOptimizer \"%s\" 2>/dev/null | grep -oE '[0-9]+'", filename.c_str());
            FILE* dro = popen(cmd, "r");
            if (dro) {
                int val = 0;
                if (fscanf(dro, "%d", &val) == 1) meta.dro_level = val;
                pclose(dro);
            }
        }

        map_picture_profile(meta.profile);

        switch (meta.dro_level) {
            case 0: meta.dro_shadow_lift = 1.0f; break;
            case 1: meta.dro_shadow_lift = 1.10f; break;
            case 3: meta.dro_shadow_lift = 1.08f; break;
            case 4: meta.dro_shadow_lift = 1.20f; break;
            case 5: meta.dro_shadow_lift = 1.30f; break;
            case 6: meta.dro_shadow_lift = 1.40f; break;
            case 7: meta.dro_shadow_lift = 1.50f; break;
            case 8: meta.dro_shadow_lift = 1.60f; break;
            default: meta.dro_shadow_lift = 1.0f; break;
        }

        return true;
    }

    bool SonyDecoder::decode(const std::string& filename, const core::MetaData& meta, core::ImageBuffer<core::u16>& out_buffer) {
        if (out_buffer.width() != meta.width || out_buffer.height() != meta.height) return false;

        FILE* f = fopen(filename.c_str(), "rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, meta.strip_offset, SEEK_SET);

        int compressed_size = file_size - meta.strip_offset;
        if (compressed_size < meta.width * meta.height) {
            fclose(f);
            return false;
        }

        std::vector<uint8_t> compressed(compressed_size);
        if (fread(compressed.data(), 1, compressed_size, f) != (size_t)compressed_size) {
            fclose(f);
            return false;
        }
        fclose(f);

        std::vector<uint16_t> curve(0x4001);
        decodeCurve(meta.sony_curve, curve.data());

        TableLookUp table(1, 1);
        table.setTable(0, curve.data(), 0x4001);

        #pragma omp parallel for
        for (int row = 0; row < meta.height; row++) {
            decompressRow(compressed.data(), meta.width, row, out_buffer.data(), table);
        }

        return true;
    }

} // namespace copy::io
