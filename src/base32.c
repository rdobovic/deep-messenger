#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <base32.h>
#include <debug.h>

static char encode_5bit(uint8_t bt) {
    const char base32[] = "abcdefghijklmnopqrstuvwxyz234567";

    return base32[bt & 0x1F]; // bt & 0001 1111
}

static uint8_t decode_char(char ch) {
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a';
    if (ch >= '2' && ch <= '7')
        return ch - '2' + 26;

    return 0;
}

// Calculate in which byte (0-4) given 5bit block (0-7) starts
static int get_byte(int block) {
    return block * 5 / 8;
}

// Calculate how many extra bits 
static int get_offset(int block) {
    return (get_byte(block) + 1) * 8 - (block + 1) * 5;
}

// Shift left for positive index and right for negative
static uint8_t shift_left(uint8_t num, int shift) {
    return shift >= 0 ? (num << shift) : (num >> -shift);
}

// Shift right for positive shift and left for negative
static uint8_t shift_right(uint8_t num, int shift) {
    return shift >= 0 ? (num >> shift) : (num << -shift);
}

// Function is used to encode chunks of up to 5 bytes
static size_t base32_encode_chunk(const uint8_t *plain, size_t len, char *coded, int use_padding) {
    int i;

    for (i = 0; i < 8; i++) {
        uint8_t ch_index;
        int cbyte = get_byte(i);
        int offset = get_offset(i);

        if (cbyte >= len) {
            if (use_padding) {
                coded[i] = BASE32_PAD_CHARACTER;
                continue;
            }
            return i;
        }

        ch_index = shift_right(plain[cbyte], offset);

        if (offset < 0) {
            ch_index |= (cbyte + 1 < len) ? shift_right(plain[cbyte + 1], 8 + offset) : 0;
        }

        coded[i] = encode_5bit(ch_index);
    }

    return 8;
}

// Encode given binary data using base32 encoding
size_t base32_encode(const uint8_t *plain, size_t len, char *coded, int use_padding) {
    int i;
    size_t coded_length = 0;

    for (i = 0; i < len; i += 5) {
        coded_length += base32_encode_chunk(
            plain + i,
            (len - i < 5) ? (len - i) : 5, 
            coded + coded_length,
            use_padding
        );
    }

    return coded_length;
}

// Function used to decode from 2 up to 8 characters of base32 encoded data
static size_t base32_decode_chunk(const char *coded, size_t len, uint8_t *plain) {
    int i;

    for (i = len - 1; i >= 0; i--) {
        if (coded[i] == BASE32_PAD_CHARACTER) {
            --len;
        }
    }
    
    plain[0] = 0;
    for (i = 0; i < 8; i++) {
        uint8_t ch_index;
        int cbyte = get_byte(i);
        int offset = get_offset(i);

        if (i >= len)
            return cbyte;

        ch_index = decode_char(coded[i]);

        plain[cbyte] |= shift_left(ch_index, offset);

        if (offset < 0 && -offset + (len - (i + 1))*5 >= 8) {
            plain[cbyte + 1] = shift_left(ch_index, 8 + offset);
        }
    }

    return 5;
}

// Decode given base32 encoded string to 
size_t base32_decode(const char *coded, size_t len, uint8_t *plain) {
    int i;
    size_t plain_length = 0;

    for (i = 0; i < len; i += 8) {
        plain_length += base32_decode_chunk(
            coded + i,
            (len - i < 8) ? (len - i) : 8,
            plain + plain_length
        );
    }

    return plain_length;
}

// Check if given string is valid base32 encoded string
int base32_valid(const char *coded, int len) {
    int i;

    if (len == -1)
        len = strlen(coded);

    for (i = 0; i < len; i++) {
        char ch = coded[i];

        if ((ch < 'a' || ch > 'z') && (ch < '2' || ch > '9'))
            return 0;
    }
    
    return 1;
}