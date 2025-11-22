#include <stdio.h>
#include <stdint.h>
#include <string.h>

// AES-128 constants
#define Nk 4  // Number of 32-bit words in the key (128 bits / 32 bits = 4)
#define Nb 4  // Number of columns (32-bit words) in the state (128 bits / 32 bits = 4)
#define Nr 10 // Number of rounds for AES-128

// Type definitions for state and key
typedef uint8_t state_t[4][4];

// The S-box lookup table
static const uint8_t s_box[256] = {
    // 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

// The Round Constant (Rcon) array
static const uint8_t Rcon[11] = {
    0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

// Helper function to print a 16-byte block in hex
void print_hex(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
    }
    printf("\n");
}

// Key Expansion functions
void rot_word(uint8_t* word) {
    uint8_t temp = word[0];
    word[0] = word[1];
    word[1] = word[2];
    word[2] = word[3];
    word[3] = temp;
}

void sub_word(uint8_t* word) {
    for (int i = 0; i < 4; ++i) {
        word[i] = s_box[word[i]];
    }
}

void key_expansion(uint8_t* round_key, const uint8_t* key) {
    int i = 0;
    // The first round key is the original key.
    for (i = 0; i < Nk * 4; ++i) {
        round_key[i] = key[i];
    }

    // All subsequent round keys are derived from the previous round key.
    for (i = Nk * 4; i < Nb * (Nr + 1) * 4; i += 4) {
        uint8_t temp[4];
        memcpy(temp, &round_key[i - 4], 4);

        if ((i / 4) % Nk == 0) {
            rot_word(temp);
            sub_word(temp);
            temp[0] ^= Rcon[(i / 4) / Nk];
        }

        round_key[i + 0] = round_key[i - Nk * 4 + 0] ^ temp[0];
        round_key[i + 1] = round_key[i - Nk * 4 + 1] ^ temp[1];
        round_key[i + 2] = round_key[i - Nk * 4 + 2] ^ temp[2];
        round_key[i + 3] = round_key[i - Nk * 4 + 3] ^ temp[3];
    }
}

// AES Transformation functions
void add_round_key(state_t* state, const uint8_t* round_key) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            (*state)[r][c] ^= round_key[r + 4 * c];
        }
    }
}

void sub_bytes(state_t* state) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            (*state)[r][c] = s_box[(*state)[r][c]];
        }
    }
}

void shift_rows(state_t* state) {
    uint8_t temp;
    // Row 1: 1-byte circular left shift
    temp = (*state)[1][0];
    (*state)[1][0] = (*state)[1][1];
    (*state)[1][1] = (*state)[1][2];
    (*state)[1][2] = (*state)[1][3];
    (*state)[1][3] = temp;

    // Row 2: 2-byte circular left shift
    temp = (*state)[2][0];
    (*state)[2][0] = (*state)[2][2];
    (*state)[2][2] = temp;
    temp = (*state)[2][1];
    (*state)[2][1] = (*state)[2][3];
    (*state)[2][3] = temp;

    // Row 3: 3-byte circular left shift
    temp = (*state)[0][3]; // Using temp as a scratch variable
    temp = (*state)[3][0];
    (*state)[3][0] = (*state)[3][3];
    (*state)[3][3] = (*state)[3][2];
    (*state)[3][2] = (*state)[3][1];
    (*state)[3][1] = temp;
}

// xtime is a helper function for MixColumns
#define xtime(x) ((x << 1) ^ (((x >> 7) & 1) * 0x1b))

void mix_columns(state_t* state) {
    uint8_t a, b, c, d;
    for (int j = 0; j < 4; ++j) {
        a = (*state)[0][j];
        b = (*state)[1][j];
        c = (*state)[2][j];
        d = (*state)[3][j];

        (*state)[0][j] = xtime(a) ^ (xtime(b) ^ b) ^ c ^ d;
        (*state)[1][j] = a ^ xtime(b) ^ (xtime(c) ^ c) ^ d;
        (*state)[2][j] = a ^ b ^ xtime(c) ^ (xtime(d) ^ d);
        (*state)[3][j] = (xtime(a) ^ a) ^ b ^ c ^ xtime(d);
    }
}

// Core encryption function for a single block
void aes_encrypt_block(uint8_t* output, const uint8_t* input, const uint8_t* round_key) {
    state_t state;
    // Copy input to state matrix (column-major order)
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            state[r][c] = input[c * 4 + r];
        }
    }

    // 1. AddRoundKey (Initial Round)
    add_round_key(&state, round_key);

    // 2. Main Rounds (Nr - 1 rounds)
    for (int round = 1; round < Nr; ++round) {
        sub_bytes(&state);
        shift_rows(&state);
        mix_columns(&state);
        add_round_key(&state, round_key + round * Nb * 4);
    }

    // 3. Final Round (no MixColumns)
    sub_bytes(&state);
    shift_rows(&state);
    add_round_key(&state, round_key + Nr * Nb * 4);

    // Copy state matrix to output (column-major order)
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            output[c * 4 + r] = state[r][c];
        }
    }
}

// ECB mode encryption for multiple blocks with PKCS#7 padding
void aes_ecb_encrypt(uint8_t* output, const uint8_t* input, size_t input_len, const uint8_t* key) {
    uint8_t round_key[Nb * (Nr + 1) * 4];
    key_expansion(round_key, key);

    size_t num_blocks = input_len / 16;
    size_t last_block_len = input_len % 16;
    
    // Encrypt full blocks
    for (size_t i = 0; i < num_blocks; ++i) {
        aes_encrypt_block(output + i * 16, input + i * 16, round_key);
    }

    // Handle padding for the last block if necessary
    if (last_block_len > 0 || num_blocks == 0) {
        uint8_t padded_block[16];
        size_t offset = num_blocks * 16;
        uint8_t padding_value = 16 - last_block_len;
        
        memcpy(padded_block, input + offset, last_block_len);
        memset(padded_block + last_block_len, padding_value, padding_value);
        
        aes_encrypt_block(output + offset, padded_block, round_key);
    } else { // Handle case where input is a multiple of 16
        uint8_t padded_block[16];
        uint8_t padding_value = 16;
        memset(padded_block, padding_value, 16);
        aes_encrypt_block(output + num_blocks * 16, padded_block, round_key);
    }
}


int main() {
    printf("--- AES-128 Single Block Test Vector ---\n");

    // FIPS-197 Appendix B Test Vector
    uint8_t key[] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
    };
    uint8_t plaintext[] = {
        0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d,
        0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34
    };
    uint8_t expected_ciphertext[] = {
        0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb,
        0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32
    };

    uint8_t ciphertext[16];
    uint8_t round_key[Nb * (Nr + 1) * 4];
    
    printf("Plaintext:  ");
    print_hex(plaintext, 16);
    printf("Key:        ");
    print_hex(key, 16);

    // Expand the key
    key_expansion(round_key, key);

    // Encrypt the block
    aes_encrypt_block(ciphertext, plaintext, round_key);

    printf("Ciphertext: ");
    print_hex(ciphertext, 16);
    printf("Expected:   ");
    print_hex(expected_ciphertext, 16);

    if (memcmp(ciphertext, expected_ciphertext, 16) == 0) {
        printf("SUCCESS: Ciphertext matches the test vector.\n");
    } else {
        printf("FAILURE: Ciphertext does not match the test vector.\n");
    }
    
    printf("\n--- AES-128 ECB Mode Multi-Block Test ---\n");
    
    // Example with two blocks (32 bytes)
    uint8_t multi_block_plaintext[] = {
        0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d, 0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34,
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    
    // Output buffer needs to be larger to hold padded result.
    // Since input is multiple of 16, we add a full padding block.
    uint8_t multi_block_ciphertext[48]; 
    
    printf("Multi-block Plaintext (%zu bytes):\n", sizeof(multi_block_plaintext));
    print_hex(multi_block_plaintext, sizeof(multi_block_plaintext));
    
    aes_ecb_encrypt(multi_block_ciphertext, multi_block_plaintext, sizeof(multi_block_plaintext), key);
    
    printf("Multi-block Ciphertext (ECB with PKCS#7 padding, %d bytes):\n", 48);
    print_hex(multi_block_ciphertext, 48);
    
    return 0;
}
