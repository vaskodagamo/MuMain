/* stb_image_write - v1.16 - public domain - http://nothings.org/stb
 *
 * Minimal implementation for MuMain ground truth capture.
 * Only PNG writing is implemented (used by GroundTruthCapture.cpp).
 *
 * USAGE:
 *   In ONE C/C++ file that #includes this file, do this:
 *      #define STB_IMAGE_WRITE_IMPLEMENTATION
 *   before the #include.  That file will build the implementation.
 *   All other files should just #include it without the #define.
 *
 * The functions:
 *   int stbi_write_png(char const *filename, int w, int h, int comp,
 *                      const void *data, int stride_in_bytes);
 *   unsigned char *stbi_write_png_to_mem(const unsigned char *pixels,
 *                      int stride_in_bytes, int w, int h, int comp, int *out_len);
 *
 * stbi_write_png returns 0 on failure and non-zero on success.
 * stbi_write_png_to_mem returns the PNG file bytes (free() them) or NULL, with
 * the byte count in *out_len; the caller writes them wherever it wants (a wide
 * path on Windows, for example).
 *
 * The data is arranged row-major, from top-to-bottom (i.e., first pixel is
 * top-left), with no padding between rows. The channels per pixel are given by
 * 'comp': 1=Y, 2=YA, 3=RGB, 4=RGBA.
 *
 * PNG creates output files with the same number of components as the input.
 * The PNG encoder requires libz or the built-in deflate; this minimal version
 * uses its own for portability: each row with the PNG filter that suits it best,
 * then deflate blocks with Huffman codes of their own over LZ77 repeats.
 *
 * CREDITS:
 *   Sean Barrett           -  PNG writing
 *   GitHub users and open  -  bugfixes and improvements
 *
 * LICENSE:
 *   Public domain (www.unlicense.org)
 */

// Note: This header intentionally uses #ifndef/#define guards instead of #pragma once.
// The STB single-header pattern requires the outer #ifndef guard to wrap both the
// declaration section and the #ifdef STB_IMAGE_WRITE_IMPLEMENTATION section. Using
// #pragma once alone would not correctly prevent double-inclusion of declarations while
// allowing the implementation to be compiled once. This is a deliberate exception to
// the project #pragma once standard (AC-STD-1), per Story 4.1.1 code review findings.
#ifndef INCLUDE_STB_IMAGE_WRITE_H
#define INCLUDE_STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef STBIW_ASSERT
#include <assert.h>
#define STBIW_ASSERT(x) assert(x)
#endif

#ifndef STBIWDEF
#ifdef STB_IMAGE_WRITE_STATIC
#define STBIWDEF static
#else
#ifdef __cplusplus
#define STBIWDEF extern "C"
#else
#define STBIWDEF extern
#endif
#endif
#endif

STBIWDEF int stbi_write_png(char const* filename, int w, int h, int comp, const void* data,
                            int stride_in_bytes);
STBIWDEF unsigned char* stbi_write_png_to_mem(const unsigned char* pixels, int stride_in_bytes, int w,
                                              int h, int comp, int* out_len);

#ifdef __cplusplus
}
#endif

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define STBIW_MSVC
#endif

#ifdef STBIW_MSVC
#pragma warning(push)
#pragma warning(disable : 4996) // _CRT_SECURE_NO_WARNINGS
#endif

static unsigned int stbiw__crc32(unsigned char* buffer, int len)
{
    static unsigned int crc_table[256] = {0};
    if (crc_table[1] == 0)
    {
        for (unsigned int i = 0; i < 256; ++i)
        {
            unsigned int c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            }
            crc_table[i] = c;
        }
    }
    unsigned int crc = 0xFFFFFFFF;
    for (int i = 0; i < len; ++i)
    {
        crc = crc_table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

static void stbiw__putc(unsigned char* out, int* pos, unsigned char c)
{
    out[(*pos)++] = c;
}

static void stbiw__write32(unsigned char* out, int* pos, unsigned int val)
{
    out[(*pos)++] = (val >> 24) & 0xFF;
    out[(*pos)++] = (val >> 16) & 0xFF;
    out[(*pos)++] = (val >> 8) & 0xFF;
    out[(*pos)++] = val & 0xFF;
}

static void stbiw__write_chunk(unsigned char* out, int* pos, const char* type,
                               unsigned char* data, int len)
{
    stbiw__write32(out, pos, (unsigned int)len);
    out[(*pos)++] = type[0];
    out[(*pos)++] = type[1];
    out[(*pos)++] = type[2];
    out[(*pos)++] = type[3];
    int type_start = *pos - 4;
    if (data && len > 0)
    {
        memcpy(out + *pos, data, (size_t)len);
        *pos += len;
    }
    // CRC over type + data
    unsigned int crc = stbiw__crc32(out + type_start, 4 + len);
    stbiw__write32(out, pos, crc);
}

// zlib stream (RFC 1950) of deflate blocks with Huffman codes built for each block (RFC
// 1951, BTYPE 10), over the repeats LZ77 finds in a 32 KiB window with hash chains. The
// filtered PNG rows below are mostly small differences, which only codes of their own
// make short; a block holds at most STBIW__ZLIB_BLOCK_TOKENS literals and repeats.
#define STBIW__ZLIB_WINDOW 32768
#define STBIW__ZLIB_HASH_SIZE 32768
#define STBIW__ZLIB_MIN_MATCH 3
#define STBIW__ZLIB_MAX_MATCH 258
#define STBIW__ZLIB_MAX_CHAIN 32
#define STBIW__ZLIB_BLOCK_TOKENS 65536
#define STBIW__LITLEN_CODES 286
#define STBIW__DISTANCE_CODES 30
#define STBIW__LENGTH_CODES 19
#define STBIW__MAX_CODE_BITS 15
#define STBIW__MAX_LENGTH_CODE_BITS 7
#define STBIW__END_OF_BLOCK 256
#define STBIW__ADLER_MOD 65521
#define STBIW__ADLER_BLOCK 5552

typedef struct
{
    unsigned char* data;
    int length;
    int capacity;
    unsigned int bits;
    int bit_count;
    int failed;
} stbiw__bit_writer;

// A literal byte (distance 0) or a repeat of `value` bytes `distance` back.
typedef struct
{
    unsigned short value;
    unsigned short distance;
} stbiw__token;

// A Huffman code: each symbol's length in bits (0: unused) and its code.
typedef struct
{
    unsigned char lengths[STBIW__LITLEN_CODES];
    unsigned short codes[STBIW__LITLEN_CODES];
} stbiw__huffman;

static const unsigned short stbiw__length_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char stbiw__length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short stbiw__distance_base[30] = {1,    2,    3,    4,    5,    7,    9,    13,
                                                        17,   25,   33,   49,   65,   97,   129,  193,
                                                        257,  385,  513,  769,  1025, 1537, 2049, 3073,
                                                        4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char stbiw__distance_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
// The order a block header lists the code lengths of the code-length code in.
static const unsigned char stbiw__length_code_order[STBIW__LENGTH_CODES] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                                            11, 4,  12, 3, 13, 2, 14, 1, 15};

static void stbiw__put_byte(stbiw__bit_writer* w, unsigned char byte)
{
    if (w->failed)
    {
        return;
    }
    if (w->length == w->capacity)
    {
        int capacity = w->capacity * 2 + 64;
        unsigned char* grown = (unsigned char*)realloc(w->data, (size_t)capacity);
        if (!grown)
        {
            w->failed = 1;
            return;
        }
        w->data = grown;
        w->capacity = capacity;
    }
    w->data[w->length++] = byte;
}

// `count` bits of `value`, least significant first (deflate's bit order).
static void stbiw__put_bits(stbiw__bit_writer* w, unsigned int value, int count)
{
    w->bits |= value << w->bit_count;
    w->bit_count += count;
    while (w->bit_count >= 8)
    {
        stbiw__put_byte(w, (unsigned char)(w->bits & 0xFF));
        w->bits >>= 8;
        w->bit_count -= 8;
    }
}

// A Huffman code goes out most significant bit first.
static void stbiw__put_code(stbiw__bit_writer* w, unsigned int code, int length)
{
    unsigned int reversed = 0;
    for (int i = 0; i < length; ++i)
    {
        reversed = (reversed << 1) | ((code >> i) & 1);
    }
    stbiw__put_bits(w, reversed, length);
}

static void stbiw__put_symbol(stbiw__bit_writer* w, const stbiw__huffman* code, int symbol)
{
    stbiw__put_code(w, code->codes[symbol], code->lengths[symbol]);
}

static int stbiw__length_code(int length)
{
    int code = 28;
    while (stbiw__length_base[code] > length)
    {
        --code;
    }
    return code;
}

static int stbiw__distance_code(int distance)
{
    int code = 29;
    while (stbiw__distance_base[code] > distance)
    {
        --code;
    }
    return code;
}

// Code lengths of a Huffman code for `frequencies` (at least two above 0), none longer than
// `max_bits`: while the tree is too deep, the counts are flattened and it is built again.
static void stbiw__code_lengths(const unsigned int* frequencies, int count, int max_bits, unsigned char* lengths)
{
    unsigned int weight[2 * STBIW__LITLEN_CODES];
    int parent[2 * STBIW__LITLEN_CODES];
    unsigned int flattened[STBIW__LITLEN_CODES];
    memcpy(flattened, frequencies, sizeof(unsigned int) * (size_t)count);
    for (;;)
    {
        int nodes = count;
        for (int i = 0; i < count; ++i)
        {
            weight[i] = flattened[i];
            parent[i] = -1;
        }
        // Joins the two lightest roots until one is left; unused symbols (weight 0) stay out.
        for (;;)
        {
            int first = -1;
            int second = -1;
            for (int i = 0; i < nodes; ++i)
            {
                if (parent[i] != -1 || weight[i] == 0)
                {
                    continue;
                }
                if (first < 0 || weight[i] < weight[first])
                {
                    second = first;
                    first = i;
                }
                else if (second < 0 || weight[i] < weight[second])
                {
                    second = i;
                }
            }
            if (second < 0)
            {
                break;
            }
            weight[nodes] = weight[first] + weight[second];
            parent[nodes] = -1;
            parent[first] = nodes;
            parent[second] = nodes;
            ++nodes;
        }
        int deepest = 0;
        for (int i = 0; i < count; ++i)
        {
            int depth = 0;
            for (int node = i; flattened[i] > 0 && parent[node] != -1; node = parent[node])
            {
                ++depth;
            }
            lengths[i] = (unsigned char)depth;
            deepest = depth > deepest ? depth : deepest;
        }
        if (deepest <= max_bits)
        {
            return;
        }
        for (int i = 0; i < count; ++i)
        {
            flattened[i] = flattened[i] > 0 ? (flattened[i] >> 1) + 1 : 0;
        }
    }
}

// The canonical codes of `code->lengths` (RFC 1951, 3.2.2).
static void stbiw__canonical_codes(stbiw__huffman* code, int count)
{
    unsigned short length_count[STBIW__MAX_CODE_BITS + 1] = {0};
    unsigned short next[STBIW__MAX_CODE_BITS + 1] = {0};
    for (int i = 0; i < count; ++i)
    {
        ++length_count[code->lengths[i]];
    }
    length_count[0] = 0;
    unsigned short value = 0;
    for (int bits = 1; bits <= STBIW__MAX_CODE_BITS; ++bits)
    {
        value = (unsigned short)((value + length_count[bits - 1]) << 1);
        next[bits] = value;
    }
    for (int i = 0; i < count; ++i)
    {
        code->codes[i] = code->lengths[i] ? next[code->lengths[i]]++ : 0;
    }
}

static void stbiw__build_code(unsigned int* frequencies, int count, int max_bits, stbiw__huffman* code)
{
    // Two used symbols at least, so that the code is complete (a decoder accepts it).
    if (frequencies[0] == 0)
    {
        frequencies[0] = 1;
    }
    if (frequencies[1] == 0)
    {
        frequencies[1] = 1;
    }
    memset(code, 0, sizeof(*code));
    stbiw__code_lengths(frequencies, count, max_bits, code->lengths);
    stbiw__canonical_codes(code, count);
}

// The run-length form of the literal/length and distance code lengths (symbols 0..18 of
// the code-length alphabet, with their extra bits).
static int stbiw__run_lengths(const unsigned char* lengths, int total, unsigned char* symbols, unsigned char* extras)
{
    int emitted = 0;
    int i = 0;
    while (i < total)
    {
        int value = lengths[i];
        int run = 1;
        while (i + run < total && lengths[i + run] == value)
        {
            ++run;
        }
        i += run;
        if (value == 0)
        {
            while (run >= 11)
            {
                int take = run < 138 ? run : 138;
                symbols[emitted] = 18;
                extras[emitted++] = (unsigned char)(take - 11);
                run -= take;
            }
            if (run >= 3)
            {
                symbols[emitted] = 17;
                extras[emitted++] = (unsigned char)(run - 3);
                run = 0;
            }
        }
        else
        {
            symbols[emitted] = (unsigned char)value;
            extras[emitted++] = 0;
            --run;
            while (run >= 3)
            {
                int take = run < 6 ? run : 6;
                symbols[emitted] = 16;
                extras[emitted++] = (unsigned char)(take - 3);
                run -= take;
            }
        }
        for (; run > 0; --run)
        {
            symbols[emitted] = (unsigned char)value;
            extras[emitted++] = 0;
        }
    }
    return emitted;
}

static void stbiw__put_run(stbiw__bit_writer* w, const stbiw__huffman* code, int symbol, int extra)
{
    stbiw__put_symbol(w, code, symbol);
    if (symbol == 16)
    {
        stbiw__put_bits(w, (unsigned int)extra, 2);
    }
    else if (symbol == 17)
    {
        stbiw__put_bits(w, (unsigned int)extra, 3);
    }
    else if (symbol == 18)
    {
        stbiw__put_bits(w, (unsigned int)extra, 7);
    }
}

// BFINAL, BTYPE 10 and the two codes of a block, as the code-length code describes them.
static void stbiw__put_block_header(stbiw__bit_writer* w, int last, const stbiw__huffman* litlen,
                                    const stbiw__huffman* distance)
{
    int litlen_count = STBIW__LITLEN_CODES;
    while (litlen_count > 257 && litlen->lengths[litlen_count - 1] == 0)
    {
        --litlen_count;
    }
    int distance_count = STBIW__DISTANCE_CODES;
    while (distance_count > 1 && distance->lengths[distance_count - 1] == 0)
    {
        --distance_count;
    }
    unsigned char all[STBIW__LITLEN_CODES + STBIW__DISTANCE_CODES];
    memcpy(all, litlen->lengths, (size_t)litlen_count);
    memcpy(all + litlen_count, distance->lengths, (size_t)distance_count);
    unsigned char symbols[STBIW__LITLEN_CODES + STBIW__DISTANCE_CODES];
    unsigned char extras[STBIW__LITLEN_CODES + STBIW__DISTANCE_CODES];
    int runs = stbiw__run_lengths(all, litlen_count + distance_count, symbols, extras);

    unsigned int frequencies[STBIW__LITLEN_CODES] = {0};
    for (int i = 0; i < runs; ++i)
    {
        ++frequencies[symbols[i]];
    }
    stbiw__huffman lengths_code;
    stbiw__build_code(frequencies, STBIW__LENGTH_CODES, STBIW__MAX_LENGTH_CODE_BITS, &lengths_code);
    int order_count = STBIW__LENGTH_CODES;
    while (order_count > 4 && lengths_code.lengths[stbiw__length_code_order[order_count - 1]] == 0)
    {
        --order_count;
    }

    stbiw__put_bits(w, last ? 1u : 0u, 1);
    stbiw__put_bits(w, 2, 2);
    stbiw__put_bits(w, (unsigned int)(litlen_count - 257), 5);
    stbiw__put_bits(w, (unsigned int)(distance_count - 1), 5);
    stbiw__put_bits(w, (unsigned int)(order_count - 4), 4);
    for (int i = 0; i < order_count; ++i)
    {
        stbiw__put_bits(w, lengths_code.lengths[stbiw__length_code_order[i]], 3);
    }
    for (int i = 0; i < runs; ++i)
    {
        stbiw__put_run(w, &lengths_code, symbols[i], extras[i]);
    }
}

// One block: its own codes for the tokens, then the tokens and the end of block.
static void stbiw__put_block(stbiw__bit_writer* w, const stbiw__token* tokens, int count, int last)
{
    unsigned int litlen_frequencies[STBIW__LITLEN_CODES] = {0};
    unsigned int distance_frequencies[STBIW__LITLEN_CODES] = {0};
    for (int i = 0; i < count; ++i)
    {
        if (tokens[i].distance == 0)
        {
            ++litlen_frequencies[tokens[i].value];
            continue;
        }
        ++litlen_frequencies[257 + stbiw__length_code(tokens[i].value)];
        ++distance_frequencies[stbiw__distance_code(tokens[i].distance)];
    }
    litlen_frequencies[STBIW__END_OF_BLOCK] = 1;
    stbiw__huffman litlen;
    stbiw__huffman distance;
    stbiw__build_code(litlen_frequencies, STBIW__LITLEN_CODES, STBIW__MAX_CODE_BITS, &litlen);
    stbiw__build_code(distance_frequencies, STBIW__DISTANCE_CODES, STBIW__MAX_CODE_BITS, &distance);
    stbiw__put_block_header(w, last, &litlen, &distance);

    for (int i = 0; i < count; ++i)
    {
        const stbiw__token token = tokens[i];
        if (token.distance == 0)
        {
            stbiw__put_symbol(w, &litlen, token.value);
            continue;
        }
        int length_code = stbiw__length_code(token.value);
        stbiw__put_symbol(w, &litlen, 257 + length_code);
        stbiw__put_bits(w, (unsigned int)(token.value - stbiw__length_base[length_code]),
                        stbiw__length_extra[length_code]);
        int distance_code = stbiw__distance_code(token.distance);
        stbiw__put_symbol(w, &distance, distance_code);
        stbiw__put_bits(w, (unsigned int)(token.distance - stbiw__distance_base[distance_code]),
                        stbiw__distance_extra[distance_code]);
    }
    stbiw__put_symbol(w, &litlen, STBIW__END_OF_BLOCK);
}

static unsigned int stbiw__zlib_hash(const unsigned char* data)
{
    return (((unsigned int)data[0] << 10) ^ ((unsigned int)data[1] << 5) ^ data[2]) & (STBIW__ZLIB_HASH_SIZE - 1);
}

// The longest earlier repeat of the bytes at `pos` within the window; its length (0 when
// shorter than a match) and distance.
static int stbiw__longest_match(const unsigned char* data, int length, int pos, const int* head, const int* chain,
                                int* distance)
{
    int best = 0;
    int limit = length - pos < STBIW__ZLIB_MAX_MATCH ? length - pos : STBIW__ZLIB_MAX_MATCH;
    if (limit < STBIW__ZLIB_MIN_MATCH)
    {
        return 0;
    }
    int candidate = head[stbiw__zlib_hash(data + pos)];
    for (int steps = 0; candidate >= 0 && steps < STBIW__ZLIB_MAX_CHAIN; ++steps)
    {
        if (pos - candidate > STBIW__ZLIB_WINDOW)
        {
            break;
        }
        int run = 0;
        while (run < limit && data[candidate + run] == data[pos + run])
        {
            ++run;
        }
        if (run > best)
        {
            best = run;
            *distance = pos - candidate;
            if (run == limit)
            {
                break;
            }
        }
        int next = chain[candidate & (STBIW__ZLIB_WINDOW - 1)];
        if (next >= candidate)
        {
            break; // the slot was reused by a later position: the chain ends here
        }
        candidate = next;
    }
    return best >= STBIW__ZLIB_MIN_MATCH ? best : 0;
}

static void stbiw__remember(const unsigned char* data, int length, int pos, int* head, int* chain)
{
    if (pos + STBIW__ZLIB_MIN_MATCH > length)
    {
        return;
    }
    unsigned int hash = stbiw__zlib_hash(data + pos);
    chain[pos & (STBIW__ZLIB_WINDOW - 1)] = head[hash];
    head[hash] = pos;
}

static unsigned int stbiw__adler32(const unsigned char* data, int length)
{
    unsigned int s1 = 1;
    unsigned int s2 = 0;
    while (length > 0)
    {
        int block = length < STBIW__ADLER_BLOCK ? length : STBIW__ADLER_BLOCK;
        for (int i = 0; i < block; ++i)
        {
            s1 += data[i];
            s2 += s1;
        }
        s1 %= STBIW__ADLER_MOD;
        s2 %= STBIW__ADLER_MOD;
        data += block;
        length -= block;
    }
    return (s2 << 16) | s1;
}

// The next literal or repeat at `*pos`; moves `*pos` past it.
static stbiw__token stbiw__next_token(const unsigned char* data, int data_len, int* pos, int* head, int* chain)
{
    int distance = 0;
    int match = stbiw__longest_match(data, data_len, *pos, head, chain, &distance);
    stbiw__token token;
    if (match == 0)
    {
        token.value = data[*pos];
        token.distance = 0;
        match = 1;
    }
    else
    {
        token.value = (unsigned short)match;
        token.distance = (unsigned short)distance;
    }
    for (int i = 0; i < match; ++i)
    {
        stbiw__remember(data, data_len, *pos + i, head, chain);
    }
    *pos += match;
    return token;
}

static unsigned char* stbiw__zlib_compress(unsigned char* data, int data_len, int* out_len, int /*quality*/)
{
    stbiw__bit_writer w = {nullptr, 0, 0, 0, 0, 0};
    w.capacity = data_len / 2 + 1024;
    w.data = (unsigned char*)malloc((size_t)w.capacity);
    int* head = (int*)malloc(STBIW__ZLIB_HASH_SIZE * sizeof(int));
    int* chain = (int*)malloc(STBIW__ZLIB_WINDOW * sizeof(int));
    stbiw__token* tokens = (stbiw__token*)malloc(STBIW__ZLIB_BLOCK_TOKENS * sizeof(stbiw__token));
    if (!w.data || !head || !chain || !tokens)
    {
        free(w.data);
        free(head);
        free(chain);
        free(tokens);
        return nullptr;
    }
    for (int i = 0; i < STBIW__ZLIB_HASH_SIZE; ++i)
    {
        head[i] = -1;
    }

    stbiw__put_byte(&w, 0x78); // deflate, 32 KiB window
    stbiw__put_byte(&w, 0x01); // no dictionary; the header is a multiple of 31
    int pos = 0;
    do
    {
        int count = 0;
        while (pos < data_len && count < STBIW__ZLIB_BLOCK_TOKENS)
        {
            tokens[count++] = stbiw__next_token(data, data_len, &pos, head, chain);
        }
        stbiw__put_block(&w, tokens, count, pos >= data_len);
    } while (pos < data_len);
    stbiw__put_bits(&w, 0, 7); // to the next byte
    free(head);
    free(chain);
    free(tokens);

    unsigned int adler = stbiw__adler32(data, data_len);
    stbiw__put_byte(&w, (unsigned char)(adler >> 24));
    stbiw__put_byte(&w, (unsigned char)(adler >> 16));
    stbiw__put_byte(&w, (unsigned char)(adler >> 8));
    stbiw__put_byte(&w, (unsigned char)adler);
    if (w.failed)
    {
        free(w.data);
        return nullptr;
    }
    *out_len = w.length;
    return w.data;
}

static int stbiw__paeth(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc)
    {
        return a;
    }
    return pb <= pc ? b : c;
}

// One row through PNG filter `type` (0 none, 1 sub, 2 up, 3 average, 4 Paeth); `above` is
// the unfiltered row above, or NULL for the first row.
static void stbiw__filter_row(const unsigned char* row, const unsigned char* above, int n, int bpp, int type,
                              unsigned char* out)
{
    for (int i = 0; i < n; ++i)
    {
        int a = i >= bpp ? row[i - bpp] : 0;
        int b = above ? above[i] : 0;
        int c = (above && i >= bpp) ? above[i - bpp] : 0;
        int predictor = 0;
        switch (type)
        {
        case 1:
            predictor = a;
            break;
        case 2:
            predictor = b;
            break;
        case 3:
            predictor = (a + b) >> 1;
            break;
        case 4:
            predictor = stbiw__paeth(a, b, c);
            break;
        default:
            break;
        }
        out[i] = (unsigned char)(row[i] - predictor);
    }
}

// The filter a row compresses best with, by the usual guess: the smallest sum of the
// filtered bytes taken as signed values. Writes the type byte and the filtered row.
static void stbiw__best_filtered_row(const unsigned char* row, const unsigned char* above, int n, int bpp,
                                     unsigned char* out, unsigned char* scratch)
{
    long best_score = -1;
    for (int type = 0; type <= 4; ++type)
    {
        stbiw__filter_row(row, above, n, bpp, type, scratch);
        long score = 0;
        for (int i = 0; i < n; ++i)
        {
            score += abs((int)(signed char)scratch[i]);
        }
        if (best_score < 0 || score < best_score)
        {
            best_score = score;
            out[0] = (unsigned char)type;
            memcpy(out + 1, scratch, (size_t)n);
        }
    }
}

unsigned char* stbi_write_png_to_mem(const unsigned char* pixels, int stride_bytes, int x, int y, int comp,
                                     int* out_len)
{
    if (!pixels || !out_len || x <= 0 || y <= 0 || comp < 1 || comp > 4)
    {
        return nullptr;
    }

    if (stride_bytes == 0)
    {
        stride_bytes = x * comp;
    }

    // Each row: its filter type byte, then comp*x filtered bytes.
    int row_bytes = x * comp;
    int raw_size = y * (1 + row_bytes);
    unsigned char* raw = (unsigned char*)malloc((size_t)raw_size);
    unsigned char* scratch = (unsigned char*)malloc((size_t)row_bytes);
    if (!raw || !scratch)
    {
        free(raw);
        free(scratch);
        return nullptr;
    }

    for (int j = 0; j < y; ++j)
    {
        const unsigned char* row = pixels + j * stride_bytes;
        const unsigned char* above = j > 0 ? row - stride_bytes : nullptr;
        stbiw__best_filtered_row(row, above, row_bytes, comp, raw + j * (row_bytes + 1), scratch);
    }
    free(scratch);

    int zlib_len = 0;
    unsigned char* zlib_data = stbiw__zlib_compress(raw, raw_size, &zlib_len, 8);
    free(raw);
    if (!zlib_data)
    {
        return nullptr;
    }

    // Estimate PNG size: sig(8) + IHDR(25) + IDAT(12+zlib_len) + IEND(12)
    int png_size = 8 + 25 + 12 + zlib_len + 12;
    unsigned char* png = (unsigned char*)malloc((size_t)png_size);
    if (!png)
    {
        free(zlib_data);
        return nullptr;
    }

    int pos = 0;
    // PNG signature
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(png + pos, sig, 8);
    pos += 8;

    // IHDR
    unsigned char ihdr[13];
    ihdr[0] = (x >> 24) & 0xFF;
    ihdr[1] = (x >> 16) & 0xFF;
    ihdr[2] = (x >> 8) & 0xFF;
    ihdr[3] = x & 0xFF;
    ihdr[4] = (y >> 24) & 0xFF;
    ihdr[5] = (y >> 16) & 0xFF;
    ihdr[6] = (y >> 8) & 0xFF;
    ihdr[7] = y & 0xFF;
    ihdr[8] = 8; // bit depth
    // color type: 0=gray, 2=RGB, 3=palette, 4=gray+alpha, 6=RGB+alpha
    static const unsigned char color_types[5] = {0, 0, 4, 2, 6};
    ihdr[9] = color_types[comp];
    ihdr[10] = 0; // compression method
    ihdr[11] = 0; // filter method
    ihdr[12] = 0; // interlace method
    stbiw__write_chunk(png, &pos, "IHDR", ihdr, 13);

    // IDAT
    stbiw__write_chunk(png, &pos, "IDAT", zlib_data, zlib_len);
    free(zlib_data);

    // IEND
    stbiw__write_chunk(png, &pos, "IEND", nullptr, 0);

    *out_len = pos;
    return png;
}

int stbi_write_png(char const* filename, int x, int y, int comp, const void* data,
                   int stride_bytes)
{
    if (!filename)
    {
        return 0;
    }

    int len = 0;
    unsigned char* png = stbi_write_png_to_mem((const unsigned char*)data, stride_bytes, x, y, comp, &len);
    if (!png)
    {
        return 0;
    }

    // Write to file
    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        free(png);
        return 0;
    }
    int written = (int)fwrite(png, 1, (size_t)len, f);
    fclose(f);
    free(png);
    return written == len ? 1 : 0;
}

#ifdef STBIW_MSVC
#pragma warning(pop)
#endif

#endif // STB_IMAGE_WRITE_IMPLEMENTATION
#endif // INCLUDE_STB_IMAGE_WRITE_H
