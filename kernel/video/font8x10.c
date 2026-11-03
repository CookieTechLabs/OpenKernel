#include <kvideo/font8x10.h>

static const char blank[10] = "\0\0\0\0\0\0\0\0\0\0";
static const char glyph_0[10] = "\x3c\x66\x6e\x76\x66\x66\x66\x3c\0\0";
static const char glyph_1[10] = "\x18\x38\x18\x18\x18\x18\x18\x7e\0\0";
static const char glyph_2[10] = "\x3c\x66\x06\x0c\x18\x30\x60\x7e\0\0";
static const char glyph_3[10] = "\x3c\x66\x06\x1c\x06\x06\x66\x3c\0\0";
static const char glyph_4[10] = "\x0c\x1c\x2c\x4c\x7e\x0c\x0c\x1e\0\0";
static const char glyph_5[10] = "\x7e\x60\x7c\x06\x06\x06\x66\x3c\0\0";
static const char glyph_6[10] = "\x1c\x30\x60\x7c\x66\x66\x66\x3c\0\0";
static const char glyph_7[10] = "\x7e\x66\x06\x0c\x18\x18\x18\x18\0\0";
static const char glyph_8[10] = "\x3c\x66\x66\x3c\x66\x66\x66\x3c\0\0";
static const char glyph_9[10] = "\x3c\x66\x66\x66\x3e\x06\x0c\x38\0\0";
static const char glyph_bang[10] = "\x18\x18\x18\x18\x18\x18\0\x18\0\0";
static const char glyph_quote[10] = "\x66\x66\x24\0\0\0\0\0\0\0";
static const char glyph_hash[10] = "\x24\x24\x7e\x24\x7e\x24\x24\0\0\0";
static const char glyph_dollar[10] = "\x18\x3e\x60\x3c\x06\x7c\x18\0\0\0";
static const char glyph_percent[10] = "\x62\x64\x08\x10\x26\x46\0\0\0\0";
static const char glyph_ampersand[10] = "\x30\x48\x30\x4a\x44\x3a\0\0\0\0";
static const char glyph_apostrophe[10] = "\x18\x18\x10\0\0\0\0\0\0\0";
static const char glyph_lparen[10] = "\x0c\x18\x30\x30\x30\x30\x18\x0c\0\0";
static const char glyph_rparen[10] = "\x30\x18\x0c\x0c\x0c\x0c\x18\x30\0\0";
static const char glyph_star[10] = "\0\x18\x5a\x3c\x7e\x3c\x5a\x18\0\0";
static const char glyph_plus[10] = "\0\x18\x18\x7e\x18\x18\0\0\0\0";
static const char glyph_comma[10] = "\0\0\0\0\0\x18\x18\x10\x20\0";
static const char glyph_minus[10] = "\0\0\0\0\x7e\0\0\0\0\0";
static const char glyph_period[10] = "\0\0\0\0\0\0\x18\x18\0\0";
static const char glyph_slash[10] = "\x06\x0c\x18\x30\x60\x40\0\0\0\0";
static const char glyph_colon[10] = "\0\x18\x18\0\0\x18\x18\0\0\0";
static const char glyph_semicolon[10] = "\0\x18\x18\0\0\x18\x18\x10\x20\0";
static const char glyph_less[10] = "\x0c\x18\x30\x60\x30\x18\x0c\0\0\0";
static const char glyph_equals[10] = "\0\0\x7e\0\x7e\0\0\0\0\0";
static const char glyph_greater[10] = "\x60\x30\x18\x0c\x18\x30\x60\0\0\0";
static const char glyph_question[10] = "\x3c\x66\x06\x0c\x18\x18\0\x18\0\0";
static const char glyph_at[10] = "\x3c\x42\x5a\x5a\x5c\x40\x3c\0\0\0";
static const char glyph_lbracket[10] = "\x3c\x30\x30\x30\x30\x30\x30\x3c\0\0";
static const char glyph_backslash[10] = "\x60\x30\x18\x0c\x06\x02\0\0\0\0";
static const char glyph_rbracket[10] = "\x3c\x0c\x0c\x0c\x0c\x0c\x0c\x3c\0\0";
static const char glyph_caret[10] = "\x18\x3c\x66\0\0\0\0\0\0\0";
static const char glyph_underscore[10] = "\0\0\0\0\0\0\0\x7e\0\0";
static const char glyph_backtick[10] = "\x30\x18\x0c\0\0\0\0\0\0\0";
static const char glyph_lbrace[10] = "\x0c\x18\x18\x30\x18\x18\x18\x0c\0\0";
static const char glyph_pipe[10] = "\x18\x18\x18\x18\x18\x18\x18\x18\0\0";
static const char glyph_rbrace[10] = "\x30\x18\x18\x0c\x18\x18\x18\x30\0\0";
static const char glyph_tilde[10] = "\0\x32\x4c\0\0\0\0\0\0\0";

static const char glyph_A[10] = "\x18\x3c\x66\x66\x7e\x66\x66\x66\0\0";
static const char glyph_B[10] = "\x7c\x66\x66\x7c\x66\x66\x66\x7c\0\0";
static const char glyph_C[10] = "\x3c\x66\x60\x60\x60\x60\x66\x3c\0\0";
static const char glyph_D[10] = "\x78\x6c\x66\x66\x66\x66\x6c\x78\0\0";
static const char glyph_E[10] = "\x7e\x60\x60\x7c\x60\x60\x60\x7e\0\0";
static const char glyph_F[10] = "\x7e\x60\x60\x7c\x60\x60\x60\x60\0\0";
static const char glyph_G[10] = "\x3c\x66\x60\x60\x6e\x66\x66\x3c\0\0";
static const char glyph_H[10] = "\x66\x66\x66\x7e\x66\x66\x66\x66\0\0";
static const char glyph_I[10] = "\x3c\x18\x18\x18\x18\x18\x18\x3c\0\0";
static const char glyph_J[10] = "\x1e\x0c\x0c\x0c\x0c\x0c\x6c\x38\0\0";
static const char glyph_K[10] = "\x66\x6c\x78\x70\x78\x6c\x66\x66\0\0";
static const char glyph_L[10] = "\x60\x60\x60\x60\x60\x60\x60\x7e\0\0";
static const char glyph_M[10] = "\x63\x77\x7f\x6b\x63\x63\x63\x63\0\0";
static const char glyph_N[10] = "\x66\x76\x7e\x7e\x6e\x66\x66\x66\0\0";
static const char glyph_O[10] = "\x3c\x66\x66\x66\x66\x66\x66\x3c\0\0";
static const char glyph_P[10] = "\x7c\x66\x66\x7c\x60\x60\x60\x60\0\0";
static const char glyph_Q[10] = "\x3c\x66\x66\x66\x66\x6e\x3c\x0e\0\0";
static const char glyph_R[10] = "\x7c\x66\x66\x7c\x78\x6c\x66\x66\0\0";
static const char glyph_S[10] = "\x3c\x66\x60\x3c\x06\x06\x66\x3c\0\0";
static const char glyph_T[10] = "\x7e\x18\x18\x18\x18\x18\x18\x18\0\0";
static const char glyph_U[10] = "\x66\x66\x66\x66\x66\x66\x66\x3c\0\0";
static const char glyph_V[10] = "\x66\x66\x66\x66\x66\x66\x3c\x18\0\0";
static const char glyph_W[10] = "\x63\x63\x63\x63\x6b\x7f\x77\x63\0\0";
static const char glyph_X[10] = "\x66\x66\x3c\x18\x3c\x66\x66\x66\0\0";
static const char glyph_Y[10] = "\x66\x66\x66\x3c\x18\x18\x18\x18\0\0";
static const char glyph_Z[10] = "\x7e\x06\x0c\x18\x30\x60\x60\x7e\0\0";

/* 
 * Hand-drawn 8x10 bitmap design for a dog's head profile looking left:
 * Row 0: 00011100 -> \x1c (Ears outline top)
 * Row 1: 00110110 -> \x36 (Floppy left ear + head back)
 * Row 2: 01100110 -> \x66 (Ears/brow structure)
 * Row 3: 01111100 -> \x7c (Eyes/snout bridge line)
 * Row 4: 11110000 -> \xf0 (Nose tip extension)
 * Row 5: 11111100 -> \xfc (Muzzle/jaw line outline)
 * Row 6: 01111110 -> \x7e (Lower jaw line finish)
 * Row 7: 00111100 -> \x3c (Neck connection point)
 * Row 8-9: Padding
 */
static const char glyph_dog[10] = "\x1c\x36\x66\x7c\xf0\xfc\x7e\x3c\0\0";

/* 
 * Hand-drawn 8x10 bitmap design for a document file with folded top right corner:
 * Row 0: 01111100 -> \x7c (Top flat edge before fold angle)
 * Row 1: 01000110 -> \x46 (Fold line diagonal cutout boundary)
 * Row 2: 01000111 -> \x47 (Fold triangle shadow line corner)
 * Row 3: 01111111 -> \x7f (Solid horizontal border block split)
 * Row 4: 01000001 -> \x41 (Empty text row body left side guide)
 * Row 5: 01011101 -> \x5d (Horizontal text line representation)
 * Row 6: 01000001 -> \x41 (Line spacing segment break gap)
 * Row 7: 01011101 -> \x5d (Horizontal text line representation)
 * Row 8: 01111111 -> \x7f (Bottom boundary enclosure layout line)
 * Row 9: Padding
 */
static const char glyph_doc[10] = "\x7c\x46\x47\x7f\x41\x5d\x41\x5d\x7f\0";

const char *font8x10[128] = {
    [' '] = blank, ['!'] = glyph_bang, ['"'] = glyph_quote,
    ['#'] = glyph_hash, ['$'] = glyph_dollar, ['%'] = glyph_percent,
    ['&'] = glyph_ampersand, ['\''] = glyph_apostrophe,
    ['('] = glyph_lparen, [')'] = glyph_rparen, ['*'] = glyph_star,
    ['+'] = glyph_plus, [','] = glyph_comma, ['-'] = glyph_minus,
    ['.'] = glyph_period, ['/'] = glyph_slash, [':'] = glyph_colon,
    [';'] = glyph_semicolon, ['<'] = glyph_less, ['='] = glyph_equals,
    ['>'] = glyph_greater, ['?'] = glyph_question, ['@'] = glyph_at,
    ['['] = glyph_lbracket, ['\\'] = glyph_backslash,
    [']'] = glyph_rbracket, ['^'] = glyph_caret, ['_'] = glyph_underscore,
    ['`'] = glyph_backtick, ['{'] = glyph_lbrace, ['|'] = glyph_pipe,
    ['}'] = glyph_rbrace, ['~'] = glyph_tilde,
    ['0'] = glyph_0, ['1'] = glyph_1, ['2'] = glyph_2, ['3'] = glyph_3,
    ['4'] = glyph_4, ['5'] = glyph_5, ['6'] = glyph_6, ['7'] = glyph_7,
    ['8'] = glyph_8, ['9'] = glyph_9,
    ['A'] = glyph_A, ['B'] = glyph_B, ['C'] = glyph_C, ['D'] = glyph_D,
    ['E'] = glyph_E, ['F'] = glyph_F, ['G'] = glyph_G, ['H'] = glyph_H,
    ['I'] = glyph_I, ['J'] = glyph_J, ['K'] = glyph_K, ['L'] = glyph_L,
    ['M'] = glyph_M, ['N'] = glyph_N, ['O'] = glyph_O, ['P'] = glyph_P,
    ['Q'] = glyph_Q, ['R'] = glyph_R, ['S'] = glyph_S, ['T'] = glyph_T,
    ['U'] = glyph_U, ['V'] = glyph_V, ['W'] = glyph_W, ['X'] = glyph_X,
    ['Y'] = glyph_Y, ['Z'] = glyph_Z,
    ['a'] = glyph_A, ['b'] = glyph_B, ['c'] = glyph_C, ['d'] = glyph_D,
    ['e'] = glyph_E, ['f'] = glyph_F, ['g'] = glyph_G, ['h'] = glyph_H,
    ['i'] = glyph_I, ['j'] = glyph_J, ['k'] = glyph_K, ['l'] = glyph_L,
    ['m'] = glyph_M, ['n'] = glyph_N, ['o'] = glyph_O, ['p'] = glyph_P,
    ['q'] = glyph_Q, ['r'] = glyph_R, ['s'] = glyph_S, ['t'] = glyph_T,
    ['u'] = glyph_U, ['v'] = glyph_V, ['w'] = glyph_W, ['x'] = glyph_X,
    ['y'] = glyph_Y, ['z'] = glyph_Z,
    
    /* Mapped custom custom symbols to unused ASCII control codes slots */
    [1] = glyph_dog,
    [2] = glyph_doc
};

const char *font8x10_get(unsigned char character) {
    if (character >= 128 || font8x10[character] == 0) {
        return blank;
    }
    return font8x10[character];
}
