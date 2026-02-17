#include "core/backend.hpp"
#include <capstone/capstone.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <sstream>
#include <iomanip>

// ── Endian helpers ──────────────────────────────────────────────────────────

static inline uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint64_t read_le64(const uint8_t* p) {
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}
static inline uint16_t read_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}
static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static inline uint64_t read_be64(const uint8_t* p) {
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
}

static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}
static inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// ════════════════════════════════════════════════════════════════════════════
// MD5 – RFC 1321 reference implementation (public domain, inlined)
// ════════════════════════════════════════════════════════════════════════════
namespace {

static const uint32_t MD5_T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const int MD5_S[64] = {
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
};

struct MD5_CTX {
    uint32_t state[4];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint32_t buflen;
};

static void md5_init(MD5_CTX* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

static void md5_transform(MD5_CTX* ctx, const uint8_t block[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; i++)
        M[i] = read_le32(block + i * 4);

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = (uint32_t)i;
        } else if (i < 32) {
            f = (d & b) | (c & ~d);
            g = (uint32_t)((5 * i + 1) % 16);
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (uint32_t)((3 * i + 5) % 16);
        } else {
            f = c ^ (b | ~d);
            g = (uint32_t)((7 * i) % 16);
        }

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + rotl32(a + f + MD5_T[i] + M[g], MD5_S[i]);
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

static void md5_update(MD5_CTX* ctx, const uint8_t* data, size_t len) {
    ctx->bitcount += (uint64_t)len * 8;
    while (len > 0) {
        uint32_t space = 64 - ctx->buflen;
        uint32_t copy = (len < space) ? (uint32_t)len : space;
        memcpy(ctx->buffer + ctx->buflen, data, copy);
        ctx->buflen += copy;
        data += copy;
        len -= copy;
        if (ctx->buflen == 64) {
            md5_transform(ctx, ctx->buffer);
            ctx->buflen = 0;
        }
    }
}

static void md5_final(MD5_CTX* ctx, uint8_t digest[16]) {
    uint64_t bits = ctx->bitcount;
    uint8_t pad = 0x80;
    md5_update(ctx, &pad, 1);
    pad = 0x00;
    while (ctx->buflen != 56)
        md5_update(ctx, &pad, 1);
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++)
        len_bytes[i] = (uint8_t)(bits >> (i * 8));
    md5_update(ctx, len_bytes, 8);
    for (int i = 0; i < 4; i++) {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// SHA-256 – FIPS 180-4 reference implementation (public domain, inlined)
// ════════════════════════════════════════════════════════════════════════════

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct SHA256_CTX {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t buffer[64];
    uint32_t buflen;
};

static void sha256_init(SHA256_CTX* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

static void sha256_transform(SHA256_CTX* ctx, const uint8_t block[64]) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++)
        W[i] = read_be32(block + i * 4);
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(W[i-15], 7) ^ rotr32(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = rotr32(W[i-2], 17) ^ rotr32(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1];
    uint32_t c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5];
    uint32_t g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + W[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len) {
    ctx->bitcount += (uint64_t)len * 8;
    while (len > 0) {
        uint32_t space = 64 - ctx->buflen;
        uint32_t copy = (len < space) ? (uint32_t)len : space;
        memcpy(ctx->buffer + ctx->buflen, data, copy);
        ctx->buflen += copy;
        data += copy;
        len -= copy;
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX* ctx, uint8_t digest[32]) {
    uint64_t bits = ctx->bitcount;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    pad = 0x00;
    while (ctx->buflen != 56)
        sha256_update(ctx, &pad, 1);
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++)
        len_bytes[i] = (uint8_t)(bits >> (56 - i * 8));
    sha256_update(ctx, len_bytes, 8);
    for (int i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// PE section parsing helpers
// ════════════════════════════════════════════════════════════════════════════

static std::string pe_section_flags(uint32_t ch) {
    std::string f;
    if (ch & 0x40000000u) f += 'R';
    if (ch & 0x80000000u) f += 'W';
    if (ch & 0x20000000u) f += 'X';
    return f.empty() ? "?" : f;
}

static std::vector<Section> parse_pe_sections(const uint8_t* data, size_t size) {
    std::vector<Section> result;
    if (size < 64) return result;
    uint32_t e_lfanew = read_le32(data + 60);
    if ((uint64_t)e_lfanew + 4 + 20 > size) return result;
    const uint8_t* pe = data + e_lfanew;
    if (pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0)
        return result;

    const uint8_t* coff = pe + 4;
    uint16_t num_sections = read_le16(coff + 2);
    uint16_t opt_hdr_size = read_le16(coff + 16);

    size_t sec_table_off = (size_t)(e_lfanew + 4 + 20 + opt_hdr_size);
    if (sec_table_off + (size_t)num_sections * 40 > size) return result;

    for (int i = 0; i < num_sections; i++) {
        const uint8_t* sh = data + sec_table_off + (size_t)i * 40;
        Section s;
        char namebuf[9] = {};
        memcpy(namebuf, sh, 8);
        s.name = namebuf;
        s.vaddr  = read_le32(sh + 12);
        s.offset = read_le32(sh + 20);
        s.size   = read_le32(sh + 16);
        s.flags  = pe_section_flags(read_le32(sh + 36));
        result.push_back(std::move(s));
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// ELF section parsing helpers
// ════════════════════════════════════════════════════════════════════════════

static std::string elf_section_flags(uint64_t fl) {
    std::string f;
    if (fl & 0x2) f += 'R';
    if (fl & 0x1) f += 'W';
    if (fl & 0x4) f += 'X';
    return f.empty() ? "?" : f;
}

static std::vector<Section> parse_elf64_sections(const uint8_t* data, size_t size) {
    std::vector<Section> result;
    if (size < 64) return result;

    uint64_t shoff     = read_le64(data + 40);
    uint16_t shentsize = read_le16(data + 58);
    uint16_t shnum     = read_le16(data + 60);
    uint16_t shstrndx  = read_le16(data + 62);

    if (shoff == 0 || shnum == 0) return result;
    if (shoff + (uint64_t)shnum * shentsize > size) return result;
    if (shentsize < 64) return result;

    const uint8_t* strtab_sh = data + shoff + (uint64_t)shstrndx * shentsize;
    uint64_t strtab_off  = 0;
    uint64_t strtab_size = 0;
    if (shstrndx < shnum) {
        strtab_off  = read_le64(strtab_sh + 24);
        strtab_size = read_le64(strtab_sh + 32);
    }

    for (int i = 0; i < shnum; i++) {
        const uint8_t* sh = data + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = read_le32(sh + 4);
        if (sh_type == 0) continue; // SHT_NULL

        Section s;
        uint32_t name_idx = read_le32(sh);
        if (strtab_off + name_idx < size && name_idx < strtab_size)
            s.name = (const char*)(data + strtab_off + name_idx);
        else
            s.name = "<unknown>";

        s.flags  = elf_section_flags(read_le64(sh + 8));
        s.vaddr  = read_le64(sh + 16);
        s.offset = read_le64(sh + 24);
        s.size   = read_le64(sh + 32);
        result.push_back(std::move(s));
    }
    return result;
}

static std::vector<Section> parse_elf32_sections(const uint8_t* data, size_t size) {
    std::vector<Section> result;
    if (size < 52) return result;

    uint32_t shoff     = read_le32(data + 32);
    uint16_t shentsize = read_le16(data + 46);
    uint16_t shnum     = read_le16(data + 48);
    uint16_t shstrndx  = read_le16(data + 50);

    if (shoff == 0 || shnum == 0) return result;
    if ((uint64_t)shoff + (uint64_t)shnum * shentsize > size) return result;
    if (shentsize < 40) return result;

    const uint8_t* strtab_sh = data + shoff + (uint64_t)shstrndx * shentsize;
    uint32_t strtab_off  = 0;
    uint32_t strtab_size = 0;
    if (shstrndx < shnum) {
        strtab_off  = read_le32(strtab_sh + 16);
        strtab_size = read_le32(strtab_sh + 20);
    }

    for (int i = 0; i < shnum; i++) {
        const uint8_t* sh = data + shoff + (uint64_t)i * shentsize;
        uint32_t sh_type = read_le32(sh + 4);
        if (sh_type == 0) continue;

        Section s;
        uint32_t name_idx = read_le32(sh);
        if (strtab_off + name_idx < size && name_idx < strtab_size)
            s.name = (const char*)(data + strtab_off + name_idx);
        else
            s.name = "<unknown>";

        s.flags  = elf_section_flags((uint64_t)read_le32(sh + 8));
        s.vaddr  = read_le32(sh + 12);
        s.offset = read_le32(sh + 16);
        s.size   = read_le32(sh + 20);
        result.push_back(std::move(s));
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// Mach-O section parsing helpers
// ════════════════════════════════════════════════════════════════════════════

static std::string macho_prot_flags(uint32_t prot) {
    std::string f;
    if (prot & 1) f += 'R';
    if (prot & 2) f += 'W';
    if (prot & 4) f += 'X';
    return f.empty() ? "?" : f;
}

static std::string safe_fixed_string(const uint8_t* p, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && p[len] != 0) len++;
    return std::string((const char*)p, len);
}

static std::vector<Section> parse_macho64_sections(const uint8_t* data, size_t size) {
    std::vector<Section> result;
    if (size < 32) return result;

    uint32_t ncmds      = read_le32(data + 16);
    uint32_t sizeofcmds = read_le32(data + 20);
    (void)sizeofcmds;

    size_t offset = 32; // past mach_header_64
    for (uint32_t cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        if (offset + 8 > size) break;
        uint32_t cmd     = read_le32(data + offset);
        uint32_t cmdsize = read_le32(data + offset + 4);
        if (cmdsize < 8 || offset + cmdsize > size) break;

        if (cmd == 0x19) { // LC_SEGMENT_64
            if (offset + 72 > size) break;
            uint32_t nsects  = read_le32(data + offset + 64);
            uint32_t initprot = read_le32(data + offset + 60);
            std::string seg_flags = macho_prot_flags(initprot);

            size_t sec_off = offset + 72;
            for (uint32_t si = 0; si < nsects; si++) {
                if (sec_off + 80 > size) break;
                Section s;
                std::string sectname = safe_fixed_string(data + sec_off, 16);
                std::string segname  = safe_fixed_string(data + sec_off + 16, 16);
                s.name   = segname + "," + sectname;
                s.vaddr  = read_le64(data + sec_off + 32);
                s.size   = read_le64(data + sec_off + 40);
                s.offset = read_le32(data + sec_off + 48);
                s.flags  = seg_flags;
                result.push_back(std::move(s));
                sec_off += 80;
            }
        }
        offset += cmdsize;
    }
    return result;
}

static std::vector<Section> parse_macho32_sections(const uint8_t* data, size_t size) {
    std::vector<Section> result;
    if (size < 28) return result;

    uint32_t ncmds = read_le32(data + 12);

    size_t offset = 28; // past mach_header
    for (uint32_t cmd_i = 0; cmd_i < ncmds; cmd_i++) {
        if (offset + 8 > size) break;
        uint32_t cmd     = read_le32(data + offset);
        uint32_t cmdsize = read_le32(data + offset + 4);
        if (cmdsize < 8 || offset + cmdsize > size) break;

        if (cmd == 0x01) { // LC_SEGMENT
            if (offset + 56 > size) break;
            uint32_t nsects  = read_le32(data + offset + 48);
            uint32_t initprot = read_le32(data + offset + 44);
            std::string seg_flags = macho_prot_flags(initprot);

            size_t sec_off = offset + 56;
            for (uint32_t si = 0; si < nsects; si++) {
                if (sec_off + 68 > size) break;
                Section s;
                std::string sectname = safe_fixed_string(data + sec_off, 16);
                std::string segname  = safe_fixed_string(data + sec_off + 16, 16);
                s.name   = segname + "," + sectname;
                s.vaddr  = read_le32(data + sec_off + 32);
                s.size   = read_le32(data + sec_off + 36);
                s.offset = read_le32(data + sec_off + 40);
                s.flags  = seg_flags;
                result.push_back(std::move(s));
                sec_off += 68;
            }
        }
        offset += cmdsize;
    }
    return result;
}

} // anonymous namespace

// ── File I/O ────────────────────────────────────────────────────────────────

std::vector<uint8_t> load_file(const char* path) {
    std::vector<uint8_t> out;
    if (!path) return out;
    FILE* f = fopen(path, "rb");
    if (!f) return out;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return out; }
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)sz);
    size_t rd = fread(out.data(), 1, out.size(), f);
    fclose(f);
    if (rd != out.size()) out.clear();
    return out;
}

bool save_file(const char* path, const uint8_t* data, size_t size) {
    if (!path) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = true;
    if (size > 0 && data) {
        ok = (fwrite(data, 1, size, f) == size);
    }
    fclose(f);
    return ok;
}

// ── Disassembly ─────────────────────────────────────────────────────────────

std::vector<DisasmLine> disassemble(const uint8_t* data, size_t size,
                                     uint64_t base_addr, int arch, int max_insns) {
    std::vector<DisasmLine> result;
    if (!data || size == 0) return result;

    cs_arch cs_a;
    cs_mode cs_m;
    switch (arch) {
        default:
        case 0: cs_a = CS_ARCH_X86;   cs_m = CS_MODE_64;      break;
        case 1: cs_a = CS_ARCH_X86;   cs_m = CS_MODE_32;      break;
        case 2: cs_a = CS_ARCH_ARM64; cs_m = CS_MODE_ARM;     break;
        case 3: cs_a = CS_ARCH_ARM;   cs_m = CS_MODE_ARM;     break;
        case 4: cs_a = CS_ARCH_MIPS;  cs_m = CS_MODE_MIPS32;  break;
    }

    csh handle;
    if (cs_open(cs_a, cs_m, &handle) != CS_ERR_OK)
        return result;
    cs_option(handle, CS_OPT_SKIPDATA, CS_OPT_ON);

    cs_insn* insn = nullptr;
    size_t count = cs_disasm(handle, data, size, base_addr,
                             (size_t)max_insns, &insn);
    if (count > 0) {
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
            DisasmLine l;
            l.address = insn[i].address;

            char hex[128] = {};
            for (int b = 0; b < insn[i].size; b++)
                snprintf(hex + b * 3, sizeof(hex) - (size_t)(b * 3),
                         "%02x ", insn[i].bytes[b]);
            l.bytes_hex = hex;
            if (!l.bytes_hex.empty() && l.bytes_hex.back() == ' ')
                l.bytes_hex.pop_back();

            l.mnemonic = insn[i].mnemonic;
            l.operands = insn[i].op_str;
            result.push_back(std::move(l));
        }
        cs_free(insn, count);
    }
    cs_close(&handle);
    return result;
}

// ── Byte histogram ──────────────────────────────────────────────────────────

std::array<uint32_t, 256> byte_histogram(const uint8_t* data, size_t size) {
    std::array<uint32_t, 256> hist{};
    if (!data) return hist;
    for (size_t i = 0; i < size; i++)
        hist[data[i]]++;
    return hist;
}

// ── Entropy curve (sliding window, step = window/2) ─────────────────────────

std::vector<float> entropy_curve(const uint8_t* data, size_t size, int window) {
    std::vector<float> out;
    if (!data || size == 0 || window <= 0) return out;
    if ((size_t)window > size) window = (int)size;

    int step = window / 2;
    if (step < 1) step = 1;
    out.reserve(size / (size_t)step + 1);

    for (size_t off = 0; off + (size_t)window <= size; off += (size_t)step) {
        int counts[256] = {};
        for (int i = 0; i < window; i++)
            counts[data[off + i]]++;
        float ent = 0.0f;
        float inv = 1.0f / (float)window;
        for (int c = 0; c < 256; c++) {
            if (counts[c] == 0) continue;
            float p = (float)counts[c] * inv;
            ent -= p * log2f(p);
        }
        out.push_back(ent);
    }
    return out;
}

// ── Bigram image (256x256 RGBA, log-scaled density) ─────────────────────────

void bigram_image(const uint8_t* data, size_t size, uint8_t* rgba_out) {
    if (!rgba_out) return;
    memset(rgba_out, 0, 256 * 256 * 4);

    if (!data || size < 2) {
        for (int i = 0; i < 256 * 256; i++)
            rgba_out[i * 4 + 3] = 255;
        return;
    }

    uint32_t counts[256][256] = {};
    for (size_t i = 0; i + 1 < size; i++)
        counts[data[i]][data[i + 1]]++;

    uint32_t mx = 1;
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
            if (counts[y][x] > mx) mx = counts[y][x];

    float logMax = log2f((float)mx + 1.0f);

    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            float val = 0.0f;
            if (counts[y][x] > 0)
                val = log2f((float)counts[y][x] + 1.0f) / logMax;
            uint8_t v = (uint8_t)(val * 255.0f);
            int idx = (y * 256 + x) * 4;
            rgba_out[idx + 0] = v;
            rgba_out[idx + 1] = v;
            rgba_out[idx + 2] = v;
            rgba_out[idx + 3] = 255;
        }
    }
}

// ── String extraction ───────────────────────────────────────────────────────

std::vector<FoundString> extract_strings(const uint8_t* data, size_t size, int min_len) {
    std::vector<FoundString> result;
    if (!data || size == 0 || min_len <= 0) return result;

    std::string cur;
    size_t start = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            if (cur.empty()) start = i;
            cur += (char)data[i];
        } else {
            if ((int)cur.size() >= min_len)
                result.push_back({start, std::move(cur)});
            cur.clear();
        }
    }
    if ((int)cur.size() >= min_len)
        result.push_back({start, std::move(cur)});
    return result;
}

// ── Magic / format detection ────────────────────────────────────────────────

std::string detect_format(const uint8_t* data, size_t size) {
    if (!data || size < 4) return "Unknown";

    if (data[0] == 'M' && data[1] == 'Z')
        return "PE executable";
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
        return "ELF binary";
    if (data[0] == 0xCF && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE)
        return "Mach-O 64-bit";
    if (data[0] == 0xCE && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE)
        return "Mach-O 32-bit";
    if (data[0] == 0xFE && data[1] == 0xED && data[2] == 0xFA && data[3] == 0xCF)
        return "Mach-O 64-bit (big-endian)";
    if (data[0] == 0xFE && data[1] == 0xED && data[2] == 0xFA && data[3] == 0xCE)
        return "Mach-O 32-bit (big-endian)";
    if (data[0] == 0xCA && data[1] == 0xFE && data[2] == 0xBA && data[3] == 0xBE)
        return "Mach-O universal binary";
    if (data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04)
        return "ZIP / JAR / APK";
    if (data[0] == 0x50 && data[1] == 0x4B)
        return "ZIP (variant)";
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
        return "PNG image";
    if (data[0] == 0xFF && data[1] == 0xD8)
        return "JPEG image";
    if (data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F')
        return "PDF document";
    if (data[0] == 0x1F && data[1] == 0x8B)
        return "GZIP archive";
    if (data[0] == 'P' && data[1] == 'K' && data[2] == 0x03 && data[3] == 0x04)
        return "ZIP archive";
    if (size >= 8 && memcmp(data, "\x00\x61\x73\x6D", 4) == 0)
        return "WebAssembly binary";
    if (size >= 4 && memcmp(data, "dex\n", 4) == 0)
        return "DEX (Android Dalvik)";
    if (size >= 8 && memcmp(data, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0)
        return "OLE2 / MS Office document";
    if (data[0] == 0x7F && data[1] == 'C' && data[2] == 'G' && data[3] == 'C')
        return "CGC binary (Cyber Grand Challenge)";

    return "Unknown format";
}

// ── Section parsing (PE / ELF / Mach-O dispatcher) ──────────────────────────

std::vector<Section> parse_sections(const uint8_t* data, size_t size) {
    if (!data || size < 4) return {};

    // PE
    if (data[0] == 'M' && data[1] == 'Z')
        return parse_pe_sections(data, size);

    // ELF
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        if (size >= 5 && data[4] == 2)
            return parse_elf64_sections(data, size);
        else
            return parse_elf32_sections(data, size);
    }

    // Mach-O 64-bit (little-endian)
    if (data[0] == 0xCF && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE)
        return parse_macho64_sections(data, size);

    // Mach-O 32-bit (little-endian)
    if (data[0] == 0xCE && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE)
        return parse_macho32_sections(data, size);

    return {};
}

// ── Hex pattern search (sliding window) ─────────────────────────────────────

std::vector<SearchHit> search_hex(const uint8_t* data, size_t size,
                                   const uint8_t* pattern, size_t pat_len) {
    std::vector<SearchHit> hits;
    if (!data || !pattern || pat_len == 0 || size < pat_len) return hits;

    // Build KMP failure function
    std::vector<size_t> fail(pat_len, 0);
    for (size_t i = 1, j = 0; i < pat_len; ) {
        if (pattern[i] == pattern[j]) {
            fail[i] = j + 1;
            i++; j++;
        } else if (j > 0) {
            j = fail[j - 1];
        } else {
            fail[i] = 0;
            i++;
        }
    }

    for (size_t i = 0, j = 0; i < size; ) {
        if (data[i] == pattern[j]) {
            i++; j++;
            if (j == pat_len) {
                hits.push_back({i - pat_len});
                j = fail[j - 1];
            }
        } else if (j > 0) {
            j = fail[j - 1];
        } else {
            i++;
        }
    }
    return hits;
}

// ── Text search ─────────────────────────────────────────────────────────────

std::vector<SearchHit> search_text(const uint8_t* data, size_t size,
                                    const char* text, bool case_sensitive) {
    std::vector<SearchHit> hits;
    if (!data || !text || size == 0) return hits;

    size_t pat_len = strlen(text);
    if (pat_len == 0 || size < pat_len) return hits;

    if (case_sensitive) {
        return search_hex(data, size, (const uint8_t*)text, pat_len);
    }

    // Case-insensitive: make lowered copies
    std::vector<uint8_t> lower_data(size);
    std::vector<uint8_t> lower_pat(pat_len);
    for (size_t i = 0; i < size; i++)
        lower_data[i] = (uint8_t)std::tolower(data[i]);
    for (size_t i = 0; i < pat_len; i++)
        lower_pat[i] = (uint8_t)std::tolower((unsigned char)text[i]);

    return search_hex(lower_data.data(), size, lower_pat.data(), pat_len);
}

// ── Hashing ─────────────────────────────────────────────────────────────────

static std::string bytes_to_hex(const uint8_t* bytes, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02x", bytes[i]);
        out += buf;
    }
    return out;
}

std::string hash_md5(const uint8_t* data, size_t size) {
    MD5_CTX ctx;
    md5_init(&ctx);
    if (data && size > 0)
        md5_update(&ctx, data, size);
    uint8_t digest[16];
    md5_final(&ctx, digest);
    return bytes_to_hex(digest, 16);
}

std::string hash_sha256(const uint8_t* data, size_t size) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    if (data && size > 0)
        sha256_update(&ctx, data, size);
    uint8_t digest[32];
    sha256_final(&ctx, digest);
    return bytes_to_hex(digest, 32);
}

// ── Data Inspector ──────────────────────────────────────────────────────────

std::vector<DataInspectorResult> inspect_data(const uint8_t* data, size_t size,
                                               size_t offset) {
    std::vector<DataInspectorResult> results;
    if (!data || offset >= size) return results;

    size_t remain = size - offset;
    const uint8_t* p = data + offset;
    char buf[256];

    // uint8
    if (remain >= 1) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)p[0]);
        results.push_back({"uint8", buf});
    }

    // int8
    if (remain >= 1) {
        snprintf(buf, sizeof(buf), "%d", (int)(int8_t)p[0]);
        results.push_back({"int8", buf});
    }

    // uint16 LE
    if (remain >= 2) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)read_le16(p));
        results.push_back({"uint16 LE", buf});
    }

    // uint16 BE
    if (remain >= 2) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)read_be16(p));
        results.push_back({"uint16 BE", buf});
    }

    // int16 LE
    if (remain >= 2) {
        snprintf(buf, sizeof(buf), "%d", (int)(int16_t)read_le16(p));
        results.push_back({"int16 LE", buf});
    }

    // int16 BE
    if (remain >= 2) {
        snprintf(buf, sizeof(buf), "%d", (int)(int16_t)read_be16(p));
        results.push_back({"int16 BE", buf});
    }

    // uint32 LE
    if (remain >= 4) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)read_le32(p));
        results.push_back({"uint32 LE", buf});
    }

    // uint32 BE
    if (remain >= 4) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)read_be32(p));
        results.push_back({"uint32 BE", buf});
    }

    // int32 LE
    if (remain >= 4) {
        snprintf(buf, sizeof(buf), "%d", (int)(int32_t)read_le32(p));
        results.push_back({"int32 LE", buf});
    }

    // int32 BE
    if (remain >= 4) {
        snprintf(buf, sizeof(buf), "%d", (int)(int32_t)read_be32(p));
        results.push_back({"int32 BE", buf});
    }

    // uint64 LE
    if (remain >= 8) {
        snprintf(buf, sizeof(buf), "%" PRIu64, read_le64(p));
        results.push_back({"uint64 LE", buf});
    }

    // uint64 BE
    if (remain >= 8) {
        snprintf(buf, sizeof(buf), "%" PRIu64, read_be64(p));
        results.push_back({"uint64 BE", buf});
    }

    // float LE (IEEE 754)
    if (remain >= 4) {
        uint32_t raw = read_le32(p);
        float fval;
        memcpy(&fval, &raw, sizeof(float));
        snprintf(buf, sizeof(buf), "%.7g", (double)fval);
        results.push_back({"float LE", buf});
    }

    // double LE (IEEE 754)
    if (remain >= 8) {
        uint64_t raw = read_le64(p);
        double dval;
        memcpy(&dval, &raw, sizeof(double));
        snprintf(buf, sizeof(buf), "%.15g", dval);
        results.push_back({"double LE", buf});
    }

    // ASCII char
    if (remain >= 1) {
        if (p[0] >= 0x20 && p[0] <= 0x7E) {
            snprintf(buf, sizeof(buf), "'%c' (0x%02x)", (char)p[0], (unsigned)p[0]);
        } else {
            snprintf(buf, sizeof(buf), "0x%02x (non-printable)", (unsigned)p[0]);
        }
        results.push_back({"ASCII char", buf});
    }

    // UTF-8 string (up to 32 chars or until null/non-printable)
    {
        std::string s;
        size_t maxlen = std::min(remain, (size_t)32);
        for (size_t i = 0; i < maxlen; i++) {
            uint8_t c = p[i];
            if (c == 0) break;
            if (c >= 0x20 && c <= 0x7E) {
                s += (char)c;
            } else if (c >= 0x80) {
                // Accept high bytes as part of UTF-8 multi-byte sequences
                s += (char)c;
            } else {
                break;
            }
        }
        if (!s.empty()) {
            results.push_back({"UTF-8 string", s});
        } else {
            results.push_back({"UTF-8 string", "(empty)"});
        }
    }

    // Hex dump (16 bytes)
    {
        std::string hex;
        size_t n = std::min(remain, (size_t)16);
        for (size_t i = 0; i < n; i++) {
            snprintf(buf, sizeof(buf), "%02x", (unsigned)p[i]);
            if (i > 0) hex += ' ';
            hex += buf;
        }
        results.push_back({"hex dump (16 bytes)", hex});
    }

    return results;
}

// ════════════════════════════════════════════════════════════════════════════
// Imports parsing (PE)
// ════════════════════════════════════════════════════════════════════════════
std::vector<ImportEntry> parse_imports(const uint8_t* data, size_t size) {
    std::vector<ImportEntry> result;
    if (!data || size < 64) return result;

    // PE only for now
    if (data[0] != 'M' || data[1] != 'Z') return result;

    uint32_t pe_off = read_le32(data + 0x3C);
    if (pe_off + 4 > size) return result;
    if (data[pe_off] != 'P' || data[pe_off+1] != 'E') return result;

    uint16_t magic = read_le16(data + pe_off + 24);
    bool pe32plus = (magic == 0x20b);

    // Import directory RVA is at optional header offset 104 (PE32) or 120 (PE32+)
    uint32_t import_dir_offset = pe32plus ? (pe_off + 24 + 120) : (pe_off + 24 + 104);
    if (import_dir_offset + 8 > size) return result;

    uint32_t import_rva = read_le32(data + import_dir_offset);
    uint32_t import_size = read_le32(data + import_dir_offset + 4);
    if (import_rva == 0 || import_size == 0) return result;

    // Parse section headers to resolve RVA to file offset
    uint16_t num_sections = read_le16(data + pe_off + 6);
    uint16_t opt_hdr_size = read_le16(data + pe_off + 20);
    uint32_t section_start = pe_off + 24 + opt_hdr_size;

    auto rva_to_offset = [&](uint32_t rva) -> uint32_t {
        for (uint16_t i = 0; i < num_sections; i++) {
            uint32_t sh = section_start + i * 40;
            if (sh + 40 > size) break;
            uint32_t vaddr = read_le32(data + sh + 12);
            uint32_t vsize = read_le32(data + sh + 8);
            uint32_t rawoff = read_le32(data + sh + 20);
            if (rva >= vaddr && rva < vaddr + vsize)
                return rawoff + (rva - vaddr);
        }
        return 0;
    };

    uint32_t idt_off = rva_to_offset(import_rva);
    if (idt_off == 0 || idt_off + 20 > size) return result;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t entry = idt_off + i * 20;
        if (entry + 20 > size) break;
        uint32_t ilt_rva = read_le32(data + entry);
        uint32_t name_rva = read_le32(data + entry + 12);
        if (name_rva == 0 && ilt_rva == 0) break;

        std::string dll_name;
        uint32_t name_off = rva_to_offset(name_rva);
        if (name_off != 0 && name_off < size) {
            for (uint32_t j = name_off; j < size && data[j]; j++)
                dll_name += (char)data[j];
        }

        uint32_t ilt_off = rva_to_offset(ilt_rva);
        if (ilt_off == 0) continue;

        int entry_size = pe32plus ? 8 : 4;
        for (int j = 0; j < 4096; j++) {
            uint32_t e = ilt_off + j * entry_size;
            if (e + entry_size > size) break;

            uint64_t thunk = pe32plus ? read_le64(data + e) : read_le32(data + e);
            if (thunk == 0) break;

            ImportEntry ie;
            ie.library = dll_name;

            bool by_ordinal = pe32plus ? (thunk >> 63) : (thunk >> 31);
            if (by_ordinal) {
                ie.hint = thunk & 0xFFFF;
                ie.name = "Ordinal #" + std::to_string(ie.hint);
            } else {
                uint32_t hint_off = rva_to_offset((uint32_t)(thunk & 0x7FFFFFFF));
                if (hint_off != 0 && hint_off + 2 < size) {
                    ie.hint = read_le16(data + hint_off);
                    for (uint32_t k = hint_off + 2; k < size && data[k]; k++)
                        ie.name += (char)data[k];
                }
            }
            result.push_back(std::move(ie));
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// Exports parsing (PE)
// ════════════════════════════════════════════════════════════════════════════
std::vector<ExportEntry> parse_exports(const uint8_t* data, size_t size) {
    std::vector<ExportEntry> result;
    if (!data || size < 64) return result;
    if (data[0] != 'M' || data[1] != 'Z') return result;

    uint32_t pe_off = read_le32(data + 0x3C);
    if (pe_off + 4 > size) return result;
    if (data[pe_off] != 'P' || data[pe_off+1] != 'E') return result;

    uint16_t magic = read_le16(data + pe_off + 24);
    bool pe32plus = (magic == 0x20b);

    uint32_t export_dir_offset = pe32plus ? (pe_off + 24 + 112) : (pe_off + 24 + 96);
    if (export_dir_offset + 8 > size) return result;

    uint32_t export_rva = read_le32(data + export_dir_offset);
    if (export_rva == 0) return result;

    uint16_t num_sections = read_le16(data + pe_off + 6);
    uint16_t opt_hdr_size = read_le16(data + pe_off + 20);
    uint32_t section_start = pe_off + 24 + opt_hdr_size;

    auto rva_to_offset = [&](uint32_t rva) -> uint32_t {
        for (uint16_t i = 0; i < num_sections; i++) {
            uint32_t sh = section_start + i * 40;
            if (sh + 40 > size) break;
            uint32_t vaddr = read_le32(data + sh + 12);
            uint32_t vsize = read_le32(data + sh + 8);
            uint32_t rawoff = read_le32(data + sh + 20);
            if (rva >= vaddr && rva < vaddr + vsize)
                return rawoff + (rva - vaddr);
        }
        return 0;
    };

    uint32_t edt_off = rva_to_offset(export_rva);
    if (edt_off == 0 || edt_off + 40 > size) return result;

    uint32_t num_funcs = read_le32(data + edt_off + 20);
    uint32_t num_names = read_le32(data + edt_off + 24);
    uint32_t funcs_rva = read_le32(data + edt_off + 28);
    uint32_t names_rva = read_le32(data + edt_off + 32);
    uint32_t ords_rva = read_le32(data + edt_off + 36);
    uint32_t ordinal_base = read_le32(data + edt_off + 16);

    uint32_t funcs_off = rva_to_offset(funcs_rva);
    uint32_t names_off = rva_to_offset(names_rva);
    uint32_t ords_off = rva_to_offset(ords_rva);

    if (funcs_off == 0 || names_off == 0 || ords_off == 0) return result;

    for (uint32_t i = 0; i < num_names && i < 8192; i++) {
        if (names_off + i * 4 + 4 > size) break;
        if (ords_off + i * 2 + 2 > size) break;

        uint32_t name_rva = read_le32(data + names_off + i * 4);
        uint16_t ordinal_idx = read_le16(data + ords_off + i * 2);
        uint32_t name_off = rva_to_offset(name_rva);

        ExportEntry ee;
        if (name_off != 0 && name_off < size) {
            for (uint32_t j = name_off; j < size && data[j]; j++)
                ee.name += (char)data[j];
        }
        ee.ordinal = ordinal_base + ordinal_idx;

        if (funcs_off + ordinal_idx * 4 + 4 <= size)
            ee.rva = read_le32(data + funcs_off + ordinal_idx * 4);
        else
            ee.rva = 0;

        result.push_back(std::move(ee));
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════════════
// File info
// ════════════════════════════════════════════════════════════════════════════
FileInfo get_file_info(const uint8_t* data, size_t size) {
    FileInfo fi;
    fi.file_size = size;
    fi.entry_point = 0;
    fi.format = detect_format(data, size);

    if (!data || size < 4) {
        fi.arch = "unknown"; fi.bits = "?"; fi.endian = "?";
        return fi;
    }

    // ELF
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F' && size >= 20) {
        bool elf64 = (data[4] == 2);
        fi.bits = elf64 ? "64" : "32";
        fi.endian = (data[5] == 1) ? "Little Endian" : "Big Endian";
        uint16_t e_machine = (data[5] == 1) ? read_le16(data + 18) : read_be16(data + 18);
        switch (e_machine) {
            case 3: fi.arch = "x86"; break;
            case 62: fi.arch = "x86-64"; break;
            case 40: fi.arch = "ARM"; break;
            case 183: fi.arch = "AArch64"; break;
            case 8: fi.arch = "MIPS"; break;
            case 243: fi.arch = "RISC-V"; break;
            default: fi.arch = "unknown (" + std::to_string(e_machine) + ")"; break;
        }
        if (elf64 && size >= 32) fi.entry_point = (data[5]==1) ? read_le64(data+24) : read_be64(data+24);
        else if (!elf64 && size >= 28) fi.entry_point = (data[5]==1) ? read_le32(data+24) : read_be32(data+24);
        return fi;
    }

    // PE
    if (data[0] == 'M' && data[1] == 'Z' && size >= 64) {
        uint32_t pe_off = read_le32(data + 0x3C);
        fi.endian = "Little Endian";
        if (pe_off + 24 < size && data[pe_off] == 'P' && data[pe_off+1] == 'E') {
            uint16_t machine = read_le16(data + pe_off + 4);
            uint16_t opt_magic = read_le16(data + pe_off + 24);
            fi.bits = (opt_magic == 0x20b) ? "64" : "32";
            switch (machine) {
                case 0x14c: fi.arch = "x86"; break;
                case 0x8664: fi.arch = "x86-64"; break;
                case 0xAA64: fi.arch = "AArch64"; break;
                case 0x1c0: fi.arch = "ARM"; break;
                default: fi.arch = "unknown"; break;
            }
            if (opt_magic == 0x20b && pe_off + 40 < size)
                fi.entry_point = read_le32(data + pe_off + 40);
            else if (pe_off + 40 < size)
                fi.entry_point = read_le32(data + pe_off + 40);
        }
        return fi;
    }

    // Mach-O
    if ((data[0] == 0xCF && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE) ||
        (data[0] == 0xCE && data[1] == 0xFA && data[2] == 0xED && data[3] == 0xFE)) {
        bool m64 = (data[0] == 0xCF);
        fi.bits = m64 ? "64" : "32";
        fi.endian = "Little Endian";
        if (size >= 8) {
            uint32_t cputype = read_le32(data + 4);
            switch (cputype & 0xFF) {
                case 7: fi.arch = m64 ? "x86-64" : "x86"; break;
                case 12: fi.arch = m64 ? "AArch64" : "ARM"; break;
                default: fi.arch = "unknown"; break;
            }
        }
        return fi;
    }

    fi.arch = "unknown"; fi.bits = "?"; fi.endian = "?";
    return fi;
}
