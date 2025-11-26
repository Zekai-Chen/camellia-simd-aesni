/*
 * Copyright (C) 2025 - AArch64 + Crypto Extension port
 * Based on x86-64/AVX/AES-NI implementation
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * AArch64/NEON/Crypto Extension implementation of Camellia cipher.
 * Uses ARMv8 Crypto Extension (AES instructions) for S-box calculations.
 * This implementation processes 16 input blocks in parallel.
 *
 * Based on the work originally presented in Master's Thesis:
 *   "Block Ciphers: Fast Implementations on x86-64 Architecture" (pages 42-50)
 *   http://urn.fi/URN:NBN:fi:oulu-201305311409
 */

.text

/**********************************************************************
 * REGISTER CONVENTION AND DATA LAYOUT SPECIFICATION
 *
 * This section defines the unified register allocation used throughout
 * all macros to avoid conflicts when composing them.
 ***********************************************************************/

/*
 * Function Parameters (AArch64 ABI):
 *   x0 (CTX): pointer to camellia_simd_ctx structure
 *   x1 (DST): pointer to output buffer (16 blocks = 256 bytes)
 *   x2 (SRC): pointer to input buffer (16 blocks = 256 bytes)
 *
 * Callee-saved registers that must be preserved:
 *   x19-x29, v8-v15 (if used)
 *
 * Key Table Layout:
 *   ctx->key_table: 272 bytes (34 x 64-bit round keys)
 *   ctx->key_length: 4 bytes (key length in bytes: 16, 24, or 32)
 */

/* Context structure offsets */
#define key_table   0
#define key_length  272

/* Register allocation for main state:
 *
 * 16-block parallel processing uses byte-slicing representation:
 * - Each 128-bit register holds the same byte position from all 16 blocks
 * - State is split into AB (8 registers) and CD (8 registers) parts
 *
 * Primary state registers (used in roundsm16 macros):
 *   v0-v7:  byte-sliced AB or CD state (8 bytes × 16 blocks)
 *   v8-v15: byte-sliced CD or AB state (8 bytes × 16 blocks)
 *
 * Temporary registers (for intermediate calculations):
 *   v16-v23: temporary values in S-box and P-function
 *   v24-v27: frequently loaded constants (mask4bit, lookup tables)
 *   v28-v31: additional scratch space
 *
 * General-purpose registers:
 *   x0 (CTX):     context pointer (preserved across most operations)
 *   x1 (DST):     output pointer
 *   x2 (SRC):     input pointer
 *   x3:           temporary storage pointer (256-byte stack buffer)
 *   x4:           loop counter / key offset
 *   x5-x9:        scratch registers
 *   x10-x15:      additional scratch space
 *
 * Stack layout (when needed):
 *   [sp, #0]:     saved callee-saved registers
 *   [sp, #64]:    temporary storage for AB state (128 bytes)
 *   [sp, #192]:   temporary storage for CD state (128 bytes)
 *   Total: 320 bytes aligned to 16-byte boundary
 */

/* Helper macros for register naming */
#define CTX  x0
#define DST  x1
#define SRC  x2
#define TMP_STORAGE x3

/**********************************************************************
 * Data layout notes:
 *
 * Camellia block structure:
 * - 128-bit block = 16 bytes
 * - Represented as two 64-bit words (L and R) in big-endian
 *
 * Byte-slicing transformation:
 * - Input: 16 blocks in standard representation
 * - After byte-slice: 16 registers, each containing same byte position
 *   from all blocks
 * - Example: v0[0..15] = byte 0 from blocks 0..15
 *           v1[0..15] = byte 1 from blocks 0..15
 *           ...
 *           v15[0..15] = byte 15 from blocks 0..15
 *
 * Endianness:
 * - Camellia spec uses big-endian representation
 * - Need to byteswap when loading/storing blocks
 * - Internal state after byte-slicing is in natural byte order
 ***********************************************************************/

/**********************************************************************
 * Constant data section
 ***********************************************************************/

.section .rodata
.align 4

/* Byte shuffle masks for broadcasting 64-bit keys to all lanes */
.Lbyte_zeros:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lbyte_ones:
	.byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
.Lbyte_twos:
	.byte 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
.Lbyte_threes:
	.byte 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
.Lbyte_fours:
	.byte 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
.Lbyte_fives:
	.byte 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
.Lbyte_sixs:
	.byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
.Lbyte_sevens:
	.byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7

/* Byteswap mask for 128-bit blocks */
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/* 4-bit mask for filter_8bit operations */
.L0f0f0f0f:
	.byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
	.byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f

/*
 * Pre-SubByte transform for sbox1, sbox2, sbox3
 * Maps Camellia input to AES SubBytes input via affine transformation
 */
.Lpre_tf_lo_s1:
	.byte 0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86
	.byte 0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88
.Lpre_tf_hi_s1:
	.byte 0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a
	.byte 0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23

/*
 * Pre-SubByte transform for sbox4
 * (includes rotation in the input transformation)
 */
.Lpre_tf_lo_s4:
	.byte 0x45, 0x40, 0x2e, 0x2b, 0x4b, 0x4e, 0x20, 0x25
	.byte 0x14, 0x11, 0x7f, 0x7a, 0x1a, 0x1f, 0x71, 0x74
.Lpre_tf_hi_s4:
	.byte 0x00, 0xf1, 0x8a, 0x7b, 0x09, 0xf8, 0x83, 0x72
	.byte 0xad, 0x5c, 0x27, 0xd6, 0xa4, 0x55, 0x2e, 0xdf

/*
 * Post-SubByte transform for sbox1, sbox4
 * Maps AES SubBytes output to Camellia output via affine transformation
 */
.Lpost_tf_lo_s1:
	.byte 0x3c, 0xcc, 0xcf, 0x3f, 0x32, 0xc2, 0xc1, 0x31
	.byte 0xdc, 0x2c, 0x2f, 0xdf, 0xd2, 0x22, 0x21, 0xd1
.Lpost_tf_hi_s1:
	.byte 0x00, 0xf9, 0x86, 0x7f, 0xd7, 0x2e, 0x51, 0xa8
	.byte 0xa4, 0x5d, 0x22, 0xdb, 0x73, 0x8a, 0xf5, 0x0c

/*
 * Post-SubByte transform for sbox2
 */
.Lpost_tf_lo_s2:
	.byte 0x78, 0x99, 0x9f, 0x7e, 0x64, 0x85, 0x83, 0x62
	.byte 0xb9, 0x58, 0x5e, 0xbf, 0xa5, 0x44, 0x42, 0xa3
.Lpost_tf_hi_s2:
	.byte 0x00, 0xf3, 0x0d, 0xfe, 0xaf, 0x5c, 0xa2, 0x51
	.byte 0x49, 0xba, 0x44, 0xb7, 0xe6, 0x15, 0xeb, 0x18

/*
 * Post-SubByte transform for sbox3
 */
.Lpost_tf_lo_s3:
	.byte 0x1e, 0x66, 0xe7, 0x9f, 0x19, 0x61, 0xe0, 0x98
	.byte 0x6e, 0x16, 0x97, 0xef, 0x69, 0x11, 0x90, 0xe8
.Lpost_tf_hi_s3:
	.byte 0x00, 0xfc, 0x43, 0xbf, 0xeb, 0x17, 0xa8, 0x54
	.byte 0x52, 0xae, 0x11, 0xed, 0xb9, 0x45, 0xfa, 0x06

/*
 * Inverse shift row pattern for AES
 * Used to apply inverse ShiftRows before SubBytes
 */
.Linv_shift_row:
	.byte 0x00, 0x0d, 0x0a, 0x07, 0x04, 0x01, 0x0e, 0x0b
	.byte 0x08, 0x05, 0x02, 0x0f, 0x0c, 0x09, 0x06, 0x03

/*
 * Shuffle mask for byte-slice transpose operations
 */
.Lshufb_16x16b:
	.byte 0x00, 0x04, 0x08, 0x0c, 0x01, 0x05, 0x09, 0x0d
	.byte 0x02, 0x06, 0x0a, 0x0e, 0x03, 0x07, 0x0b, 0x0f

/*
 * Pack/unpack byteswap mask
 * Keep low 8 bytes unchanged, clear high 8 bytes (0x0f -> out of range for tbl)
 */
.Lpack_bswap:
	.byte 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04
	.byte 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80

.text
.align 4

/**********************************************************************
 * Core macro implementations
 ***********************************************************************/

/*
 * filter_8bit - 8-bit table lookup using 4-bit split method
 *
 * This macro implements the core S-box lookup using AES SubBytes:
 * 1. Split input byte into high 4 bits and low 4 bits
 * 2. Use table lookup for each half
 * 3. XOR the results together
 *
 * Parameters:
 *   x:        input/output register (MODIFIED)
 *   lo_t:     low nibble lookup table (128-bit)
 *   hi_t:     high nibble lookup table (128-bit)
 *   mask4bit: 0x0f0f... mask register
 *   tmp0:     temporary register (CLOBBERED)
 *
 * Input:  x = input bytes
 * Output: x = filtered bytes
 * Clobbers: tmp0
 */
.macro filter_8bit x, lo_t, hi_t, mask4bit, tmp0
	// Extract low 4 bits: tmp0 = x & 0x0f
	and	\tmp0\().16b, \x\().16b, \mask4bit\().16b

	// Extract high 4 bits: x = (x & ~0x0f) >> 4
	// x86 does: vpandn x, mask4bit, x  then  vpsrld $4, x, x
	// CRITICAL: Must clear low 4 bits BEFORE shifting to match x86!
	bic	\x\().16b, \x\().16b, \mask4bit\().16b  /* x &= ~0x0f */
	ushr	\x\().4s, \x\().4s, #4                  /* shift by 32-bit word */

	// Table lookup for low nibble: tmp0 = lo_t[tmp0]
	tbl	\tmp0\().16b, {\lo_t\().16b}, \tmp0\().16b

	// Table lookup for high nibble: x = hi_t[x]
	tbl	\x\().16b, {\hi_t\().16b}, \x\().16b

	// XOR the results: x = tmp0 ^ x
	eor	\x\().16b, \tmp0\().16b, \x\().16b
.endm

/*
 * transpose_4x4 - Transpose 4x4 matrix of 32-bit elements
 *
 * Parameters:
 *   x0, x1, x2, x3: input/output registers
 *   t1, t2:         temporary registers
 *
 * This matches the C macro using vtrn1/vtrn2 (not zip1/zip2!)
 */
.macro transpose_4x4 x0, x1, x2, x3, t1, t2
	/* C reference: vpunpckhdq128(x1, x0, t2) => vzip2q_u32(x0, x1) */
	zip2	\t2\().4s, \x0\().4s, \x1\().4s

	/* C reference: vpunpckldq128(x1, x0, x0) => vzip1q_u32(x0, x1) */
	zip1	\x0\().4s, \x0\().4s, \x1\().4s

	/* C reference: vpunpckldq128(x3, x2, t1) => vzip1q_u32(x2, x3) */
	zip1	\t1\().4s, \x2\().4s, \x3\().4s

	/* C reference: vpunpckhdq128(x3, x2, x2) => vzip2q_u32(x2, x3) */
	zip2	\x2\().4s, \x2\().4s, \x3\().4s

	/* C reference: vpunpckhqdq128(t1, x0, x1) => vzip2q_u64(x0, t1) */
	zip2	\x1\().2d, \x0\().2d, \t1\().2d

	/* C reference: vpunpcklqdq128(t1, x0, x0) => vzip1q_u64(x0, t1) */
	zip1	\x0\().2d, \x0\().2d, \t1\().2d

	/* C reference: vpunpckhqdq128(x2, t2, x3) => vzip2q_u64(t2, x2) */
	zip2	\x3\().2d, \t2\().2d, \x2\().2d

	/* C reference: vpunpcklqdq128(x2, t2, x2) => vzip1q_u64(t2, x2) */
	zip1	\x2\().2d, \t2\().2d, \x2\().2d
.endm

/*
 * byteslice_16x16b_fast - 16x16 byte matrix transpose
 *
 * Transposes between block format and byte-sliced format.
 * This is the core transformation for parallel block processing.
 *
 * Parameters (16 input/output vectors + 2 temporaries):
 *   a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, a3, b3, c3, d3
 *   st0, st1: temporary storage registers
 *
 * Note: Requires x_tmp as scratch register for loading constant address
 */
.macro byteslice_16x16b_fast a0, b0, c0, d0, a1, b1, c1, d1, \
                              a2, b2, c2, d2, a3, b3, c3, d3, \
                              st0, st1, x_tmp
	/* Save d2, d3 */
	mov	\st0\().16b, \d2\().16b
	mov	\st1\().16b, \d3\().16b

	/* First group of 4x4 transposes */
	transpose_4x4	\a0, \a1, \a2, \a3, \d2, \d3
	transpose_4x4	\b0, \b1, \b2, \b3, \d2, \d3

	/* Restore d2, d3 */
	mov	\d2\().16b, \st0\().16b
	mov	\d3\().16b, \st1\().16b

	/* Save a0, a1 */
	mov	\st0\().16b, \a0\().16b
	mov	\st1\().16b, \a1\().16b

	/* Second group of 4x4 transposes */
	transpose_4x4	\c0, \c1, \c2, \c3, \a0, \a1
	transpose_4x4	\d0, \d1, \d2, \d3, \a0, \a1

	/* Load shuffle mask */
	adrp	\x_tmp, .Lshufb_16x16b
	add	\x_tmp, \x_tmp, :lo12:.Lshufb_16x16b
	ld1	{\a0\().16b}, [\x_tmp]

	/* Restore a1 to apply shuffle */
	mov	\a1\().16b, \st1\().16b

	/* Apply shuffle to all vectors except a0 (which holds the mask) */
	tbl	\a2\().16b, {\a2\().16b}, \a0\().16b
	tbl	\a3\().16b, {\a3\().16b}, \a0\().16b
	tbl	\b0\().16b, {\b0\().16b}, \a0\().16b
	tbl	\b1\().16b, {\b1\().16b}, \a0\().16b
	tbl	\b2\().16b, {\b2\().16b}, \a0\().16b
	tbl	\b3\().16b, {\b3\().16b}, \a0\().16b
	tbl	\a1\().16b, {\a1\().16b}, \a0\().16b
	tbl	\c0\().16b, {\c0\().16b}, \a0\().16b
	tbl	\c1\().16b, {\c1\().16b}, \a0\().16b
	tbl	\c2\().16b, {\c2\().16b}, \a0\().16b
	tbl	\c3\().16b, {\c3\().16b}, \a0\().16b
	tbl	\d0\().16b, {\d0\().16b}, \a0\().16b
	tbl	\d1\().16b, {\d1\().16b}, \a0\().16b
	tbl	\d2\().16b, {\d2\().16b}, \a0\().16b
	tbl	\d3\().16b, {\d3\().16b}, \a0\().16b

	/* Save d3, restore original a0 from st0 */
	mov	\st1\().16b, \d3\().16b
	mov	\d3\().16b, \st0\().16b

	/* Apply shuffle to restored a0 (which was saved in d3) */
	tbl	\a0\().16b, {\d3\().16b}, \a0\().16b

	/* Save d2 for later */
	mov	\st0\().16b, \d2\().16b

	/* Third group of 4x4 transposes */
	transpose_4x4	\a0, \b0, \c0, \d0, \d2, \d3
	transpose_4x4	\a1, \b1, \c1, \d1, \d2, \d3

	/* Restore d2, d3 */
	mov	\d2\().16b, \st0\().16b
	mov	\d3\().16b, \st1\().16b

	/* Save b0, b1 */
	mov	\st0\().16b, \b0\().16b
	mov	\st1\().16b, \b1\().16b

	/* Fourth group of 4x4 transposes */
	transpose_4x4	\a2, \b2, \c2, \d2, \b0, \b1
	transpose_4x4	\a3, \b3, \c3, \d3, \b0, \b1

	/* Restore b0, b1 */
	mov	\b0\().16b, \st0\().16b
	mov	\b1\().16b, \st1\().16b
.endm

/*
 * inpack16_pre - Load 16 blocks and XOR with pre-whitening key
 *
 * Loads 16 input blocks from memory and XORs each with key_table[0].
 * Output is in block format (not yet byte-sliced).
 *
 * Parameters:
 *   x0-x7, y0-y7: output registers (16 blocks total)
 *   rio:          input pointer (must be 16-byte aligned)
 *   key:          64-bit whitening key value
 *   tmp:          temporary register for key vector
 *   x_tmp:        scratch register for loading pack_bswap address
 *
 * C equivalent:
 *   vmovq128((key), x0);
 *   vpshufb128(pack_bswap_stack, x0, x0);
 *   for (i = 0; i < 16; i++) {
 *     output[i] = input[i] ^ x0;
 *   }
 */
.macro inpack16_pre x0, x1, x2, x3, x4, x5, x6, x7, \
                     y0, y1, y2, y3, y4, y5, y6, y7, \
                     rio, key, tmp, x_tmp
	/* Load and prepare whitening key using pack_bswap (matching x86 behavior) */
	/* Store key (8 bytes) to stack temp area at sp+32 */
	str	\key, [sp, #32]
	/* Store 8 zero bytes at sp+40 */
	mov	\x_tmp, xzr
	str	\x_tmp, [sp, #40]
	/* Load complete 16-byte vector: [key_bytes_0-7 | zero_bytes_8-15] */
	add	\x_tmp, sp, #32
	ld1	{\tmp\().16b}, [\x_tmp]

	/* Apply pack_bswap shuffle (matching x86's vpshufb) */
	adrp	\x_tmp, .Lpack_bswap
	add	\x_tmp, \x_tmp, :lo12:.Lpack_bswap
	ld1	{v31.16b}, [\x_tmp]
	tbl	\tmp\().16b, {\tmp\().16b}, v31.16b

	/* Load 16 blocks and XOR with key (REVERSED order to match C reference!)
	 * C loads: block 0→y7, block 1→y6, ..., block 7→y0,
	 *          block 8→x7, block 9→x6, ..., block 15→x0
	 */
	ld1	{\y7\().16b}, [\rio], #16
	eor	\y7\().16b, \y7\().16b, \tmp\().16b

	ld1	{\y6\().16b}, [\rio], #16
	eor	\y6\().16b, \y6\().16b, \tmp\().16b

	ld1	{\y5\().16b}, [\rio], #16
	eor	\y5\().16b, \y5\().16b, \tmp\().16b

	ld1	{\y4\().16b}, [\rio], #16
	eor	\y4\().16b, \y4\().16b, \tmp\().16b

	ld1	{\y3\().16b}, [\rio], #16
	eor	\y3\().16b, \y3\().16b, \tmp\().16b

	ld1	{\y2\().16b}, [\rio], #16
	eor	\y2\().16b, \y2\().16b, \tmp\().16b

	ld1	{\y1\().16b}, [\rio], #16
	eor	\y1\().16b, \y1\().16b, \tmp\().16b

	ld1	{\y0\().16b}, [\rio], #16
	eor	\y0\().16b, \y0\().16b, \tmp\().16b

	ld1	{\x7\().16b}, [\rio], #16
	eor	\x7\().16b, \x7\().16b, \tmp\().16b

	ld1	{\x6\().16b}, [\rio], #16
	eor	\x6\().16b, \x6\().16b, \tmp\().16b

	ld1	{\x5\().16b}, [\rio], #16
	eor	\x5\().16b, \x5\().16b, \tmp\().16b

	ld1	{\x4\().16b}, [\rio], #16
	eor	\x4\().16b, \x4\().16b, \tmp\().16b

	ld1	{\x3\().16b}, [\rio], #16
	eor	\x3\().16b, \x3\().16b, \tmp\().16b

	ld1	{\x2\().16b}, [\rio], #16
	eor	\x2\().16b, \x2\().16b, \tmp\().16b

	ld1	{\x1\().16b}, [\rio], #16
	eor	\x1\().16b, \x1\().16b, \tmp\().16b

	ld1	{\x0\().16b}, [\rio], #16
	eor	\x0\().16b, \x0\().16b, \tmp\().16b
.endm

/*
 * inpack16_post - Byteslice pre-whitened blocks and store to memory
 *
 * Converts 16 blocks from block format to byte-sliced format and stores
 * to AB/CD memory buffers for round processing.
 *
 * Parameters:
 *   x0-x7, y0-y7:   input/output registers (16 blocks)
 *   mem_ab:         pointer to AB buffer (128 bytes = 8 vectors)
 *   mem_cd:         pointer to CD buffer (128 bytes = 8 vectors)
 *   tmp0, tmp1:     temporary registers for byteslice
 *   x_tmp:          scratch register for byteslice
 *
 * C equivalent:
 *   byteslice_16x16b_fast(x0, x1, ..., y7, mem_ab[0], mem_cd[0]);
 *   mem_ab[0..7] = x0..x7;
 *   mem_cd[0..7] = y0..y7;
 */
.macro inpack16_post x0, x1, x2, x3, x4, x5, x6, x7, \
                      y0, y1, y2, y3, y4, y5, y6, y7, \
                      mem_ab, mem_cd, tmp0, tmp1, x_tmp
	/* Apply byteslice transformation */
	/* Input: x0-x7=v0-v7 (blocks 0-7), y0-y7=v8-v15 (blocks 8-15) */
	/* C reference: byteslice_16x16b_fast(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, ...) */
	/* IMPORTANT: Must match C parameter order exactly - NOT interleaved! */
	byteslice_16x16b_fast \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	                      \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	                      \tmp0, \tmp1, \x_tmp

	/* Store to AB buffer */
	st1	{\x0\().16b, \x1\().16b}, [\mem_ab], #32
	st1	{\x2\().16b, \x3\().16b}, [\mem_ab], #32
	st1	{\x4\().16b, \x5\().16b}, [\mem_ab], #32
	st1	{\x6\().16b, \x7\().16b}, [\mem_ab]

	/* Store to CD buffer */
	st1	{\y0\().16b, \y1\().16b}, [\mem_cd], #32
	st1	{\y2\().16b, \y3\().16b}, [\mem_cd], #32
	st1	{\y4\().16b, \y5\().16b}, [\mem_cd], #32
	st1	{\y6\().16b, \y7\().16b}, [\mem_cd]
.endm

/*
 * outunpack16 - Byteslice output blocks and XOR with post-whitening key
 *
 * Converts 16 blocks from byte-sliced format to block format, XORs with
 * post-whitening key, preparing for output.
 *
 * Parameters:
 *   x0-x7, y0-y7:   input/output registers (16 blocks)
 *   key:            64-bit whitening key value
 *   stack_tmp0:     temporary storage (will be clobbered)
 *   stack_tmp1:     temporary storage for byteslice
 *   x_tmp:          scratch register
 *
 * C equivalent:
 *   byteslice_16x16b_fast(y0, y4, x0, x4, ..., stack_tmp0, stack_tmp1);
 *   vmovq128((key), x0);
 *   vpshufb128(pack_bswap_stack, x0, x0);
 *   for (i = 0; i < 16; i++) output[i] ^= x0;
 */
.macro outunpack16 x0, x1, x2, x3, x4, x5, x6, x7, \
                    y0, y1, y2, y3, y4, y5, y6, y7, \
                    key, stack_tmp0, stack_tmp1, x_tmp
	/* Apply byteslice (DIFFERENT order than inpack: y-params first!) */
	/* Input: x0-x7=AB[0-7] (v0-v7), y0-y7=CD[0-7] (v8-v15) */
	/* Order MUST match C: y0,y4,x0,x4,y1,y5,x1,x5,y2,y6,x2,x6,y3,y7,x3,x7 */
	byteslice_16x16b_fast \y0, \y4, \x0, \x4, \y1, \y5, \x1, \x5, \
	                      \y2, \y6, \x2, \x6, \y3, \y7, \x3, \x7, \
	                      \stack_tmp0, \stack_tmp1, \x_tmp

	/* Save x0 to stack_tmp0 */
	mov	\stack_tmp0\().16b, \x0\().16b

	/* Load and prepare whitening key using pack_bswap (matching x86 behavior) */
	/* Store key (8 bytes) to stack temp area at sp+32 */
	str	\key, [sp, #32]
	/* Store 8 zero bytes at sp+40 */
	mov	\x_tmp, xzr
	str	\x_tmp, [sp, #40]
	/* Load complete 16-byte vector: [key_bytes_0-7 | zero_bytes_8-15] */
	add	\x_tmp, sp, #32
	ld1	{\x0\().16b}, [\x_tmp]

	/* Apply pack_bswap shuffle (matching x86's vpshufb) */
	adrp	\x_tmp, .Lpack_bswap
	add	\x_tmp, \x_tmp, :lo12:.Lpack_bswap
	ld1	{v31.16b}, [\x_tmp]
	tbl	\x0\().16b, {\x0\().16b}, v31.16b

	/* XOR all blocks with key (order matches C: y7,y6,...,y0,x7,...,x1) */
	eor	\y7\().16b, \y7\().16b, \x0\().16b
	eor	\y6\().16b, \y6\().16b, \x0\().16b
	eor	\y5\().16b, \y5\().16b, \x0\().16b
	eor	\y4\().16b, \y4\().16b, \x0\().16b
	eor	\y3\().16b, \y3\().16b, \x0\().16b
	eor	\y2\().16b, \y2\().16b, \x0\().16b
	eor	\y1\().16b, \y1\().16b, \x0\().16b
	eor	\y0\().16b, \y0\().16b, \x0\().16b
	eor	\x7\().16b, \x7\().16b, \x0\().16b
	eor	\x6\().16b, \x6\().16b, \x0\().16b
	eor	\x5\().16b, \x5\().16b, \x0\().16b
	eor	\x4\().16b, \x4\().16b, \x0\().16b
	eor	\x3\().16b, \x3\().16b, \x0\().16b
	eor	\x2\().16b, \x2\().16b, \x0\().16b
	eor	\x1\().16b, \x1\().16b, \x0\().16b

	/* XOR x0 with saved value */
	eor	\x0\().16b, \stack_tmp0\().16b, \x0\().16b
.endm

/*
 * rol32_1_16 - Rotate left 32-bit values by 1 bit (16-way parallel)
 *
 * Implements rotation of byte-sliced 32-bit integers.
 * The rotation carries the MSB from one byte to the LSB of the next.
 *
 * Parameters:
 *   v0, v1, v2, v3: byte-sliced 32-bit values (INPUT/OUTPUT)
 *   t0, t1, t2:     temporary registers
 *   zero:           all-zero register
 *
 * Note: In byte-sliced representation:
 *   v0 = byte 0 of all blocks
 *   v1 = byte 1 of all blocks
 *   v2 = byte 2 of all blocks
 *   v3 = byte 3 of all blocks
 *
 * The rotation needs to carry bit 7 of each byte to bit 0 of the next byte
 * in the word (v0[7]->v1[0], v1[7]->v2[0], v2[7]->v3[0], v3[7]->v0[0])
 */
.macro rol32_1_16 v0, v1, v2, v3, t0, t1, t2, zero
	// Extract MSB (bit 7) of each byte using signed comparison with zero
	// If byte >= 0, MSB=0; if byte < 0 (signed), MSB=1
	cmlt	\t0\().16b, \v0\().16b, #0
	shl	\v0\().16b, \v0\().16b, #1
	abs	\t0\().16b, \t0\().16b  // Convert comparison result to 0 or 1

	cmlt	\t1\().16b, \v1\().16b, #0
	shl	\v1\().16b, \v1\().16b, #1
	abs	\t1\().16b, \t1\().16b

	cmlt	\t2\().16b, \v2\().16b, #0
	shl	\v2\().16b, \v2\().16b, #1
	abs	\t2\().16b, \t2\().16b

	// OR the carry bits into the next bytes
	orr	\v1\().16b, \v1\().16b, \t0\().16b

	cmlt	\t0\().16b, \v3\().16b, #0
	shl	\v3\().16b, \v3\().16b, #1
	abs	\t0\().16b, \t0\().16b

	orr	\v2\().16b, \v2\().16b, \t1\().16b
	orr	\v3\().16b, \v3\().16b, \t2\().16b
	orr	\v0\().16b, \v0\().16b, \t0\().16b
.endm

/*
 * Helper macro: Load a constant table address into a register
 * Usage: load_const_addr label, temp_xreg, output_vreg
 */
.macro load_const_addr label, xreg, vreg
	adrp	\xreg, \label
	add	\xreg, \xreg, :lo12:\label
	ld1	{\vreg\().16b}, [\xreg]
.endm

/*
 * roundsm16 - One Feistel F-function round (16-way parallel)
 *
 * Semantic operation:
 *   new_CD = old_CD ⊕ F(AB, key)
 *   where F(AB, key) = P(S(AB)) ⊕ expand(key)
 *
 * Three stages:
 *   1. S-function: Transform AB through Camellia S-boxes (using AES SubBytes)
 *   2. P-function: Linear mixing with XORs
 *   3. Feistel XOR: Combine with old_CD and key to produce new_CD
 *
 * Parameters:
 *   x0-x7:    INPUT: byte-sliced AB state (8 × 16 bytes)
 *             OUTPUT: byte-sliced new_CD state (8 × 16 bytes)
 *   t0-t7:    Temporary registers (will be clobbered)
 *   mem_cd:   Pointer (x-register) to old CD state in memory
 *   key_addr: Address of 64-bit round key
 *
 * Clobbers: t0-t7, x10-x12
 */
.macro roundsm16 x0, x1, x2, x3, x4, x5, x6, x7, t0, t1, t2, t3, t4, t5, t6, t7, mem_cd, key_addr
	/**************************************************************
	 * STAGE 1: S-FUNCTION (Camellia S-boxes via AES SubBytes)
	 **************************************************************/

	/* Load frequently-used constants */
	load_const_addr .Linv_shift_row, x10, \t4
	load_const_addr .L0f0f0f0f, x10, \t7
	load_const_addr .Lpre_tf_lo_s1, x10, \t0
	load_const_addr .Lpre_tf_hi_s1, x10, \t1

	/* Apply inverse ShiftRows to cancel AESE's ShiftRows */
	tbl	\x0\().16b, {\x0\().16b}, \t4\().16b
	tbl	\x7\().16b, {\x7\().16b}, \t4\().16b
	tbl	\x1\().16b, {\x1\().16b}, \t4\().16b
	tbl	\x4\().16b, {\x4\().16b}, \t4\().16b
	tbl	\x2\().16b, {\x2\().16b}, \t4\().16b
	tbl	\x5\().16b, {\x5\().16b}, \t4\().16b
	tbl	\x3\().16b, {\x3\().16b}, \t4\().16b
	tbl	\x6\().16b, {\x6\().16b}, \t4\().16b

	/* Pre-filter S-boxes 1, 2, 3 (bytes 0,1,2,4,5,7) */
	load_const_addr .Lpre_tf_lo_s4, x10, \t2
	load_const_addr .Lpre_tf_hi_s4, x10, \t3

	filter_8bit \x0, \t0, \t1, \t7, \t6  /* s1: byte 0 */
	filter_8bit \x7, \t0, \t1, \t7, \t6  /* s1: byte 7 */
	filter_8bit \x1, \t0, \t1, \t7, \t6  /* s2: byte 1 */
	filter_8bit \x4, \t0, \t1, \t7, \t6  /* s2: byte 4 */
	filter_8bit \x2, \t0, \t1, \t7, \t6  /* s3: byte 2 */
	filter_8bit \x5, \t0, \t1, \t7, \t6  /* s3: byte 5 */

	/* Pre-filter S-box 4 (bytes 3, 6) */
	eor	\t4\().16b, \t4\().16b, \t4\().16b  /* zero for AESE */
	filter_8bit \x3, \t2, \t3, \t7, \t6  /* s4: byte 3 */
	filter_8bit \x6, \t2, \t3, \t7, \t6  /* s4: byte 6 */

	/* AES SubBytes using AESE (matches C reference which doesn't use aesimc)
	 * AESE does: SubBytes + ShiftRows + MixColumns + XOR
	 * Note: C reference uses vaeseq_u8 without inverse MixColumns */
	aese	\x0\().16b, \t4\().16b
	aese	\x7\().16b, \t4\().16b
	aese	\x1\().16b, \t4\().16b
	aese	\x4\().16b, \t4\().16b
	aese	\x2\().16b, \t4\().16b
	aese	\x5\().16b, \t4\().16b
	aese	\x3\().16b, \t4\().16b
	aese	\x6\().16b, \t4\().16b

	/* Post-filter: Map AES SubBytes output to Camellia S-box output */
	load_const_addr .Lpost_tf_lo_s1, x10, \t0
	load_const_addr .Lpost_tf_hi_s1, x10, \t1
	load_const_addr .Lpost_tf_lo_s3, x10, \t2
	load_const_addr .Lpost_tf_hi_s3, x10, \t3

	filter_8bit \x0, \t0, \t1, \t7, \t6  /* s1: byte 0 */
	filter_8bit \x7, \t0, \t1, \t7, \t6  /* s1: byte 7 */
	filter_8bit \x3, \t0, \t1, \t7, \t6  /* s4: byte 3 */
	filter_8bit \x6, \t0, \t1, \t7, \t6  /* s4: byte 6 */

	load_const_addr .Lpost_tf_lo_s2, x10, \t4
	load_const_addr .Lpost_tf_hi_s2, x10, \t5

	filter_8bit \x2, \t2, \t3, \t7, \t6  /* s3: byte 2 */
	filter_8bit \x5, \t2, \t3, \t7, \t6  /* s3: byte 5 */

	/* Load round key (64-bit) and prepare for broadcasting */
	ldr	x11, [\key_addr]
	dup	v30.2d, x11  /* Broadcast 64-bit key to both lanes */

	filter_8bit \x1, \t4, \t5, \t7, \t2  /* s2: byte 1 */
	filter_8bit \x4, \t4, \t5, \t7, \t2  /* s2: byte 4 */

	/**************************************************************
	 * STAGE 2: P-FUNCTION (Linear Mixing)
	 **************************************************************/

	/* Extract and broadcast individual key bytes for later XOR */
	dup	\t0\().16b, v30.b[0]  /* key byte 0 */
	dup	\t1\().16b, v30.b[1]  /* key byte 1 */
	dup	\t2\().16b, v30.b[2]  /* key byte 2 */
	dup	\t3\().16b, v30.b[3]  /* key byte 3 */

	/* P-function: Series of XORs for linear mixing
	 * This implements the Camellia P permutation */
	eor	\x0\().16b, \x0\().16b, \x5\().16b
	eor	\x1\().16b, \x1\().16b, \x6\().16b
	eor	\x2\().16b, \x2\().16b, \x7\().16b
	eor	\x3\().16b, \x3\().16b, \x4\().16b

	dup	\t4\().16b, v30.b[4]  /* key byte 4 */
	dup	\t5\().16b, v30.b[5]  /* key byte 5 */

	eor	\x4\().16b, \x4\().16b, \x2\().16b
	eor	\x5\().16b, \x5\().16b, \x3\().16b
	eor	\x6\().16b, \x6\().16b, \x0\().16b
	eor	\x7\().16b, \x7\().16b, \x1\().16b

	dup	\t6\().16b, v30.b[6]  /* key byte 6 */
	dup	\t7\().16b, v30.b[7]  /* key byte 7 */

	eor	\x0\().16b, \x0\().16b, \x7\().16b
	eor	\x1\().16b, \x1\().16b, \x4\().16b
	eor	\x2\().16b, \x2\().16b, \x5\().16b
	eor	\x3\().16b, \x3\().16b, \x6\().16b

	eor	\x4\().16b, \x4\().16b, \x3\().16b
	eor	\x5\().16b, \x5\().16b, \x0\().16b
	eor	\x6\().16b, \x6\().16b, \x1\().16b
	eor	\x7\().16b, \x7\().16b, \x2\().16b

	/**************************************************************
	 * STAGE 3: FEISTEL XOR (Combine with old_CD and key)
	 **************************************************************/

	/* XOR key material and CD into state
	 * Correct x86 mapping (as pointed out by user):
	 *   x4 ^= key[3] ^ CD[0]
	 *   x5 ^= key[2] ^ CD[1]
	 *   x6 ^= key[1] ^ CD[2]
	 *   x7 ^= key[0] ^ CD[3]
	 *   x0 ^= key[7] ^ CD[4]
	 *   x1 ^= key[6] ^ CD[5]
	 *   x2 ^= key[5] ^ CD[6]
	 *   x3 ^= key[4] ^ CD[7]
	 */

	/* x4 ^= key[3] ^ CD[0] */
	eor	\x4\().16b, \x4\().16b, \t3\().16b
	ldr	q31, [\mem_cd, #0]
	eor	\x4\().16b, \x4\().16b, v31.16b

	/* x5 ^= key[2] ^ CD[1] */
	eor	\x5\().16b, \x5\().16b, \t2\().16b
	ldr	q31, [\mem_cd, #16]
	eor	\x5\().16b, \x5\().16b, v31.16b

	/* x6 ^= key[1] ^ CD[2] */
	eor	\x6\().16b, \x6\().16b, \t1\().16b
	ldr	q31, [\mem_cd, #32]
	eor	\x6\().16b, \x6\().16b, v31.16b

	/* x7 ^= key[0] ^ CD[3] */
	eor	\x7\().16b, \x7\().16b, \t0\().16b
	ldr	q31, [\mem_cd, #48]
	eor	\x7\().16b, \x7\().16b, v31.16b

	/* x0 ^= key[7] ^ CD[4] */
	eor	\x0\().16b, \x0\().16b, \t7\().16b
	ldr	q31, [\mem_cd, #64]
	eor	\x0\().16b, \x0\().16b, v31.16b

	/* x1 ^= key[6] ^ CD[5] */
	eor	\x1\().16b, \x1\().16b, \t6\().16b
	ldr	q31, [\mem_cd, #80]
	eor	\x1\().16b, \x1\().16b, v31.16b

	/* x2 ^= key[5] ^ CD[6] */
	eor	\x2\().16b, \x2\().16b, \t5\().16b
	ldr	q31, [\mem_cd, #96]
	eor	\x2\().16b, \x2\().16b, v31.16b

	/* x3 ^= key[4] ^ CD[7] */
	eor	\x3\().16b, \x3\().16b, \t4\().16b
	ldr	q31, [\mem_cd, #112]
	eor	\x3\().16b, \x3\().16b, v31.16b

	/* Output: x0-x7 now contain new_CD state (byte-sliced) */
.endm

/*
 * roundsm16_opt - Optimized version with preloaded constants
 *
 * Same as roundsm16, but accepts preloaded constants as parameters
 * to avoid redundant adrp+add+ld1 in hot path.
 *
 * Parameters (same as roundsm16):
 *   x0-x7:    INPUT: byte-sliced AB state (8 × 16 bytes)
 *             OUTPUT: byte-sliced new_CD state (8 × 16 bytes)
 *   t0-t7:    Temporary registers (will be clobbered)
 *   mem_cd:   Pointer (x-register) to old CD state in memory
 *   key_addr: Address of 64-bit round key
 *
 * Additional parameters (preloaded constants):
 *   c_inv_sr:   .Linv_shift_row (inverse ShiftRows pattern)
 *   c_mask4:    .L0f0f0f0f (4-bit mask for filter_8bit)
 *   c_pre_lo1:  .Lpre_tf_lo_s1 (pre-transform low for sbox1/2/3)
 *   c_pre_hi1:  .Lpre_tf_hi_s1 (pre-transform high for sbox1/2/3)
 *   c_pre_lo4:  .Lpre_tf_lo_s4 (pre-transform low for sbox4)
 *   c_pre_hi4:  .Lpre_tf_hi_s4 (pre-transform high for sbox4)
 *
 * Expected register allocation for constants:
 *   v24 = c_inv_sr, v25 = c_mask4, v26 = c_pre_lo1
 *   v27 = c_pre_hi1, v28 = c_pre_lo4, v29 = c_pre_hi4
 *
 * Performance: Saves 6 × 3 = 18 instructions per round
 *              (6 load_const_addr eliminated)
 */
.macro roundsm16_opt x0, x1, x2, x3, x4, x5, x6, x7, \
                     t0, t1, t2, t3, t4, t5, t6, t7, \
                     mem_cd, key_addr, \
                     c_inv_sr, c_mask4, c_pre_lo1, c_pre_hi1, c_pre_lo4, c_pre_hi4
	/**************************************************************
	 * STAGE 1: S-FUNCTION (Camellia S-boxes via AES SubBytes)
	 **************************************************************/

	/* OPTIMIZATION: Localized constant loading
	 * Load constants here for short lifetime (only during pre-filter stage)
	 * They will be overwritten by post-filter tables, minimizing register pressure
	 * Avoids long-lived v24-v29 occupation that hurts OoO scheduling */
	load_const_addr .Linv_shift_row, x10, \t4
	load_const_addr .L0f0f0f0f, x10, \t7
	load_const_addr .Lpre_tf_lo_s1, x10, \t0
	load_const_addr .Lpre_tf_hi_s1, x10, \t1
	load_const_addr .Lpre_tf_lo_s4, x10, \t2
	load_const_addr .Lpre_tf_hi_s4, x10, \t3

	/* Apply inverse ShiftRows to cancel AESE's ShiftRows */
	tbl	\x0\().16b, {\x0\().16b}, \t4\().16b
	tbl	\x7\().16b, {\x7\().16b}, \t4\().16b
	tbl	\x1\().16b, {\x1\().16b}, \t4\().16b
	tbl	\x4\().16b, {\x4\().16b}, \t4\().16b
	tbl	\x2\().16b, {\x2\().16b}, \t4\().16b
	tbl	\x5\().16b, {\x5\().16b}, \t4\().16b
	tbl	\x3\().16b, {\x3\().16b}, \t4\().16b
	tbl	\x6\().16b, {\x6\().16b}, \t4\().16b

	/* Pre-filter S-boxes 1, 2, 3 (bytes 0,1,2,4,5,7) */
	filter_8bit \x0, \t0, \t1, \t7, \t6  /* s1: byte 0 */
	filter_8bit \x7, \t0, \t1, \t7, \t6  /* s1: byte 7 */
	filter_8bit \x1, \t0, \t1, \t7, \t6  /* s2: byte 1 */
	filter_8bit \x4, \t0, \t1, \t7, \t6  /* s2: byte 4 */
	filter_8bit \x2, \t0, \t1, \t7, \t6  /* s3: byte 2 */
	filter_8bit \x5, \t0, \t1, \t7, \t6  /* s3: byte 5 */

	/* Pre-filter S-box 4 (bytes 3, 6) */
	eor	\t4\().16b, \t4\().16b, \t4\().16b  /* zero for AESE */
	filter_8bit \x3, \t2, \t3, \t7, \t6  /* s4: byte 3 */
	filter_8bit \x6, \t2, \t3, \t7, \t6  /* s4: byte 6 */

	/* AES SubBytes using AESE (matches C reference which doesn't use aesimc)
	 * AESE does: SubBytes + ShiftRows + MixColumns + XOR
	 * Note: C reference uses vaeseq_u8 without inverse MixColumns */
	aese	\x0\().16b, \t4\().16b
	aese	\x7\().16b, \t4\().16b
	aese	\x1\().16b, \t4\().16b
	aese	\x4\().16b, \t4\().16b
	aese	\x2\().16b, \t4\().16b
	aese	\x5\().16b, \t4\().16b
	aese	\x3\().16b, \t4\().16b
	aese	\x6\().16b, \t4\().16b

	/* Post-filter: Map AES SubBytes output to Camellia S-box output
	 * These loads overwrite t0-t5, naturally ending pre-filter constants' lifetime */
	load_const_addr .Lpost_tf_lo_s1, x10, \t0
	load_const_addr .Lpost_tf_hi_s1, x10, \t1
	load_const_addr .Lpost_tf_lo_s3, x10, \t2
	load_const_addr .Lpost_tf_hi_s3, x10, \t3

	filter_8bit \x0, \t0, \t1, \t7, \t6  /* s1: byte 0 */
	filter_8bit \x7, \t0, \t1, \t7, \t6  /* s1: byte 7 */
	filter_8bit \x3, \t0, \t1, \t7, \t6  /* s4: byte 3 */
	filter_8bit \x6, \t0, \t1, \t7, \t6  /* s4: byte 6 */

	load_const_addr .Lpost_tf_lo_s2, x10, \t4
	load_const_addr .Lpost_tf_hi_s2, x10, \t5

	filter_8bit \x2, \t2, \t3, \t7, \t6  /* s3: byte 2 */
	filter_8bit \x5, \t2, \t3, \t7, \t6  /* s3: byte 5 */

	/* Load round key (64-bit) and prepare for broadcasting */
	ldr	x11, [\key_addr]
	dup	v30.2d, x11  /* Broadcast 64-bit key to both lanes */

	filter_8bit \x1, \t4, \t5, \t7, \t2  /* s2: byte 1 */
	filter_8bit \x4, \t4, \t5, \t7, \t2  /* s2: byte 4 */

	/**************************************************************
	 * STAGE 2: P-FUNCTION (Linear Mixing)
	 **************************************************************/

	/* Extract and broadcast individual key bytes for later XOR */
	dup	\t0\().16b, v30.b[0]  /* key byte 0 */
	dup	\t1\().16b, v30.b[1]  /* key byte 1 */
	dup	\t2\().16b, v30.b[2]  /* key byte 2 */
	dup	\t3\().16b, v30.b[3]  /* key byte 3 */

	/* P-function: Series of XORs for linear mixing
	 * This implements the Camellia P permutation */
	eor	\x0\().16b, \x0\().16b, \x5\().16b
	eor	\x1\().16b, \x1\().16b, \x6\().16b
	eor	\x2\().16b, \x2\().16b, \x7\().16b
	eor	\x3\().16b, \x3\().16b, \x4\().16b

	dup	\t4\().16b, v30.b[4]  /* key byte 4 */
	dup	\t5\().16b, v30.b[5]  /* key byte 5 */

	eor	\x4\().16b, \x4\().16b, \x2\().16b
	eor	\x5\().16b, \x5\().16b, \x3\().16b
	eor	\x6\().16b, \x6\().16b, \x0\().16b
	eor	\x7\().16b, \x7\().16b, \x1\().16b

	dup	\t6\().16b, v30.b[6]  /* key byte 6 */
	dup	\t7\().16b, v30.b[7]  /* key byte 7 */

	eor	\x0\().16b, \x0\().16b, \x7\().16b
	eor	\x1\().16b, \x1\().16b, \x4\().16b
	eor	\x2\().16b, \x2\().16b, \x5\().16b
	eor	\x3\().16b, \x3\().16b, \x6\().16b

	eor	\x4\().16b, \x4\().16b, \x3\().16b
	eor	\x5\().16b, \x5\().16b, \x0\().16b
	eor	\x6\().16b, \x6\().16b, \x1\().16b
	eor	\x7\().16b, \x7\().16b, \x2\().16b

	/**************************************************************
	 * STAGE 3: FEISTEL XOR (Combine with old_CD and key)
	 **************************************************************/

	/* XOR key material and CD into state
	 * Correct x86 mapping (as pointed out by user):
	 *   x4 ^= key[3] ^ CD[0]
	 *   x5 ^= key[2] ^ CD[1]
	 *   x6 ^= key[1] ^ CD[2]
	 *   x7 ^= key[0] ^ CD[3]
	 *   x0 ^= key[7] ^ CD[4]
	 *   x1 ^= key[6] ^ CD[5]
	 *   x2 ^= key[5] ^ CD[6]
	 *   x3 ^= key[4] ^ CD[7]
	 */

	/* x4 ^= key[3] ^ CD[0] */
	eor	\x4\().16b, \x4\().16b, \t3\().16b
	ldr	q31, [\mem_cd, #0]
	eor	\x4\().16b, \x4\().16b, v31.16b

	/* x5 ^= key[2] ^ CD[1] */
	eor	\x5\().16b, \x5\().16b, \t2\().16b
	ldr	q31, [\mem_cd, #16]
	eor	\x5\().16b, \x5\().16b, v31.16b

	/* x6 ^= key[1] ^ CD[2] */
	eor	\x6\().16b, \x6\().16b, \t1\().16b
	ldr	q31, [\mem_cd, #32]
	eor	\x6\().16b, \x6\().16b, v31.16b

	/* x7 ^= key[0] ^ CD[3] */
	eor	\x7\().16b, \x7\().16b, \t0\().16b
	ldr	q31, [\mem_cd, #48]
	eor	\x7\().16b, \x7\().16b, v31.16b

	/* x0 ^= key[7] ^ CD[4] */
	eor	\x0\().16b, \x0\().16b, \t7\().16b
	ldr	q31, [\mem_cd, #64]
	eor	\x0\().16b, \x0\().16b, v31.16b

	/* x1 ^= key[6] ^ CD[5] */
	eor	\x1\().16b, \x1\().16b, \t6\().16b
	ldr	q31, [\mem_cd, #80]
	eor	\x1\().16b, \x1\().16b, v31.16b

	/* x2 ^= key[5] ^ CD[6] */
	eor	\x2\().16b, \x2\().16b, \t5\().16b
	ldr	q31, [\mem_cd, #96]
	eor	\x2\().16b, \x2\().16b, v31.16b

	/* x3 ^= key[4] ^ CD[7] */
	eor	\x3\().16b, \x3\().16b, \t4\().16b
	ldr	q31, [\mem_cd, #112]
	eor	\x3\().16b, \x3\().16b, v31.16b

	/* Output: x0-x7 now contain new_CD state (byte-sliced) */
.endm

/*
 * fls16 - FL/FLINV function (whitening layer, 16-way parallel)
 *
 * Implements Camellia FL function:
 *   1. lr ^= ROL32(kll & ll, 1)
 *   2. rl ^= (krr | rr)
 *   3. rr ^= ROL32(krl & rl, 1)
 *   4. ll ^= (klr | lr)
 *
 * Parameters:
 *   l0-l3:     Left low (ll) - INPUT/OUTPUT
 *   l4-l7:     Left high (lr) - INPUT/OUTPUT
 *   mem_r:     Pointer to right state (r[0-7], 128 bytes) in memory
 *   mem_kl:    Pointer to left key (64-bit kl: kll[31:0], klr[63:32])
 *   mem_kr:    Pointer to right key (64-bit kr: krl[31:0], krr[63:32])
 *   t0-t3:     Temporary registers
 *   tt0-tt3:   Additional temporary registers
 */
.macro fls16 l0, l1, l2, l3, l4, l5, l6, l7, mem_r, mem_kl, mem_kr, t0, t1, t2, t3, tt0, tt1, tt2, tt3
	/* Step 1: lr ^= ROL32(kll & ll, 1) */
	/* Load kll (low 32 bits of kl) and broadcast */
	ldr	w11, [\mem_kl]          /* Load kll (32-bit) */
	dup	v30.2s, w11             /* Broadcast to vector */
	dup	\t0\().16b, v30.b[3]    /* Broadcast each byte (REVERSED to match C) */
	dup	\t1\().16b, v30.b[2]
	dup	\t2\().16b, v30.b[1]
	dup	\t3\().16b, v30.b[0]

	/* t0-t3 &= ll (l0-l3) */
	and	\t0\().16b, \t0\().16b, \l0\().16b
	and	\t1\().16b, \t1\().16b, \l1\().16b
	and	\t2\().16b, \t2\().16b, \l2\().16b
	and	\t3\().16b, \t3\().16b, \l3\().16b

	/* rol32(t0-t3, 1) */
	eor	\tt0\().16b, \tt0\().16b, \tt0\().16b  /* zero */
	rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

	/* lr ^= t0-t3 */
	eor	\l4\().16b, \l4\().16b, \t0\().16b
	eor	\l5\().16b, \l5\().16b, \t1\().16b
	eor	\l6\().16b, \l6\().16b, \t2\().16b
	eor	\l7\().16b, \l7\().16b, \t3\().16b

	/* Step 2: rl ^= (krr | rr) */
	/* Load krr (high 32 bits of kr) */
	ldr	w11, [\mem_kr, #4]       /* Load krr (upper 32 bits) */
	dup	v30.2s, w11
	dup	\t0\().16b, v30.b[3]     /* REVERSED to match C */
	dup	\t1\().16b, v30.b[2]
	dup	\t2\().16b, v30.b[1]
	dup	\t3\().16b, v30.b[0]

	/* Load rr (r[4-7]) and OR with krr */
	ldr	q31, [\mem_r, #64]
	orr	\t0\().16b, \t0\().16b, v31.16b
	ldr	q31, [\mem_r, #80]
	orr	\t1\().16b, \t1\().16b, v31.16b
	ldr	q31, [\mem_r, #96]
	orr	\t2\().16b, \t2\().16b, v31.16b
	ldr	q31, [\mem_r, #112]
	orr	\t3\().16b, \t3\().16b, v31.16b

	/* XOR with rl (r[0-3]) and write back */
	ldr	q31, [\mem_r, #0]
	eor	v31.16b, v31.16b, \t0\().16b
	str	q31, [\mem_r, #0]
	ldr	q31, [\mem_r, #16]
	eor	v31.16b, v31.16b, \t1\().16b
	str	q31, [\mem_r, #16]
	ldr	q31, [\mem_r, #32]
	eor	v31.16b, v31.16b, \t2\().16b
	str	q31, [\mem_r, #32]
	ldr	q31, [\mem_r, #48]
	eor	v31.16b, v31.16b, \t3\().16b
	str	q31, [\mem_r, #48]

	/* Step 3: rr ^= ROL32(krl & rl, 1) */
	/* Load krl (low 32 bits of kr) */
	ldr	w11, [\mem_kr]
	dup	v30.2s, w11
	dup	\t0\().16b, v30.b[3]     /* REVERSED to match C */
	dup	\t1\().16b, v30.b[2]
	dup	\t2\().16b, v30.b[1]
	dup	\t3\().16b, v30.b[0]

	/* AND with rl (r[0-3]) */
	ldr	q31, [\mem_r, #0]
	and	\t0\().16b, \t0\().16b, v31.16b
	ldr	q31, [\mem_r, #16]
	and	\t1\().16b, \t1\().16b, v31.16b
	ldr	q31, [\mem_r, #32]
	and	\t2\().16b, \t2\().16b, v31.16b
	ldr	q31, [\mem_r, #48]
	and	\t3\().16b, \t3\().16b, v31.16b

	/* rol32(t0-t3, 1) */
	eor	\tt0\().16b, \tt0\().16b, \tt0\().16b  /* zero */
	rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

	/* XOR with rr (r[4-7]) and write back */
	ldr	q31, [\mem_r, #64]
	eor	v31.16b, v31.16b, \t0\().16b
	str	q31, [\mem_r, #64]
	ldr	q31, [\mem_r, #80]
	eor	v31.16b, v31.16b, \t1\().16b
	str	q31, [\mem_r, #80]
	ldr	q31, [\mem_r, #96]
	eor	v31.16b, v31.16b, \t2\().16b
	str	q31, [\mem_r, #96]
	ldr	q31, [\mem_r, #112]
	eor	v31.16b, v31.16b, \t3\().16b
	str	q31, [\mem_r, #112]

	/* Step 4: ll ^= (klr | lr) */
	/* Load klr (high 32 bits of kl) */
	ldr	w11, [\mem_kl, #4]
	dup	v30.2s, w11
	dup	\t0\().16b, v30.b[3]     /* REVERSED to match C */
	dup	\t1\().16b, v30.b[2]
	dup	\t2\().16b, v30.b[1]
	dup	\t3\().16b, v30.b[0]

	/* OR with lr (l4-l7) */
	orr	\t0\().16b, \t0\().16b, \l4\().16b
	orr	\t1\().16b, \t1\().16b, \l5\().16b
	orr	\t2\().16b, \t2\().16b, \l6\().16b
	orr	\t3\().16b, \t3\().16b, \l7\().16b

	/* XOR with ll (l0-l3) */
	eor	\l0\().16b, \l0\().16b, \t0\().16b
	eor	\l1\().16b, \l1\().16b, \t1\().16b
	eor	\l2\().16b, \l2\().16b, \t2\().16b
	eor	\l3\().16b, \l3\().16b, \t3\().16b
.endm

/*
 * two_roundsm16 - Execute two Feistel rounds with AB/CD swapping
 *
 * Implements:
 *   Round 1: new_CD = AB ⊕ F(old_CD, key[i])
 *   Round 2: new_AB = old_CD ⊕ F(new_CD, key[i+dir])
 *
 * Parameters:
 *   x0-x7:     INPUT: AB state, OUTPUT: new AB state
 *   y0-y7:     Temporary registers
 *   mem_ab:    Pointer to AB state in memory
 *   mem_cd:    Pointer to CD state in memory
 *   i:         Key index for round 1
 *   dir:       Direction (+1 for encryption, -1 for decryption)
 *   store_ab:  0 = don't store AB, 1 = store AB to memory
 *
 * Clobbers: y0-y7, x10-x12, memory at mem_cd (and mem_ab if store_ab=1)
 */
.macro two_roundsm16 x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, y6, y7, mem_ab, mem_cd, i, dir, store_ab
	/* ========== Round 1: F(AB, key_i) ⊕ CD → new_CD ========== */
	add	x12, CTX, #key_table
	add	x12, x12, #(\i * 8)

	roundsm16_opt \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_cd, x12, \
	              v24, v25, v26, v27, v28, v29

	/* Store new_CD with x86 memory layout: cd[4..7]=x0..x3; cd[0..3]=x4..x7 */
	add	x11, \mem_cd, #64
	st1	{\x0\().16b}, [x11]   /* cd[4] = x0 */
	add	x11, \mem_cd, #80
	st1	{\x1\().16b}, [x11]   /* cd[5] = x1 */
	add	x11, \mem_cd, #96
	st1	{\x2\().16b}, [x11]   /* cd[6] = x2 */
	add	x11, \mem_cd, #112
	st1	{\x3\().16b}, [x11]   /* cd[7] = x3 */
	st1	{\x4\().16b}, [\mem_cd]    /* cd[0] = x4 */
	add	x11, \mem_cd, #16
	st1	{\x5\().16b}, [x11]   /* cd[1] = x5 */
	add	x11, \mem_cd, #32
	st1	{\x6\().16b}, [x11]   /* cd[2] = x6 */
	add	x11, \mem_cd, #48
	st1	{\x7\().16b}, [x11]   /* cd[3] = x7 */

	/* ========== Round 2: F(new_CD, key_{i+dir}) ⊕ AB → new_AB ========== */

	/* Key address for round 2 */
	add	x12, CTX, #key_table
	.if \dir == 1
		add	x12, x12, #((\i + 1) * 8)
	.else
		add	x12, x12, #((\i - 1) * 8)
	.endif

	/* Call roundsm16 with reordered parameters: x4,x5,x6,x7,x0,x1,x2,x3
	 * This matches x86's approach - no register moves needed!
	 * The new_CD values are still in x0-x7, we just pass them in different order.
	 * mem_ab is used as the CD source (it contains old AB). */
	roundsm16_opt \x4, \x5, \x6, \x7, \x0, \x1, \x2, \x3, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, x12, \
	              v24, v25, v26, v27, v28, v29

	/* Store new_AB in linear order to match C's store_ab_state(x0,x1,x2,x3,x4,x5,x6,x7)
	 * After roundsm16(x4,x5,x6,x7,x0,x1,x2,x3,...), the new_AB is in those registers.
	 * We store: ab[0]=x0, ab[1]=x1, ..., ab[7]=x7 */
	.if \store_ab == 1
		st1	{\x0\().16b}, [\mem_ab]    /* ab[0] = x0 */
		add	x11, \mem_ab, #16
		st1	{\x1\().16b}, [x11]   /* ab[1] = x1 */
		add	x11, \mem_ab, #32
		st1	{\x2\().16b}, [x11]   /* ab[2] = x2 */
		add	x11, \mem_ab, #48
		st1	{\x3\().16b}, [x11]   /* ab[3] = x3 */
		add	x11, \mem_ab, #64
		st1	{\x4\().16b}, [x11]   /* ab[4] = x4 */
		add	x11, \mem_ab, #80
		st1	{\x5\().16b}, [x11]   /* ab[5] = x5 */
		add	x11, \mem_ab, #96
		st1	{\x6\().16b}, [x11]   /* ab[6] = x6 */
		add	x11, \mem_ab, #112
		st1	{\x7\().16b}, [x11]   /* ab[7] = x7 */
	.endif
.endm

/*
 * enc_rounds16 - Execute 6 encryption rounds (3 × two_roundsm16)
 */
.macro enc_rounds16 x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, y6, y7, mem_ab, mem_cd, i
	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 2), 1, 1

	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 4), 1, 1

	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 6), 1, 0
.endm

/*
 * dec_rounds16 - Execute 6 decryption rounds (reverse order)
 */
.macro dec_rounds16 x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, y6, y7, mem_ab, mem_cd, i
	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 7), -1, 1

	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 5), -1, 1

	two_roundsm16 \x0, \x1, \x2, \x3, \x4, \x5, \x6, \x7, \
	              \y0, \y1, \y2, \y3, \y4, \y5, \y6, \y7, \
	              \mem_ab, \mem_cd, (\i + 3), -1, 0
.endm

/**********************************************************************
 * Main exported functions (stubs commented out - use C implementation)
 ***********************************************************************/

/**********************************************************************
 * camellia_encrypt_16blks_simd128
 *
 * Full encryption for 128-bit keys (18 rounds + FL layers).
 *
 * C signature:
 *   void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
 *                                         void *out, const void *in);
 *
 * Parameters:
 *   x0 (CTX): pointer to context structure
 *   x1: pointer to output (256 bytes = 16 blocks)
 *   x2: pointer to input (256 bytes = 16 blocks)
 *
 * Stack layout:
 *   [sp+0..15]:   saved x19-x20
 *   [sp+16..31]:  saved x21-x22
 *   [sp+32..47]:  temporary space for whitening key construction
 *   [sp+48..175]: AB buffer (128 bytes)
 *   [sp+176..303]: CD buffer (128 bytes)
 *
 * Note: This version only supports 128-bit keys (ctx->key_length == 16).
 **********************************************************************/
.align 4
.global camellia_encrypt_16blks_simd128
.type camellia_encrypt_16blks_simd128, %function

camellia_encrypt_16blks_simd128_asm_temp:
	/* Save frame pointer and link register */
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	/* Save callee-saved registers */
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!

	/* Save input parameters to callee-saved registers FIRST */
	/* Note: CTX (x0) must remain as-is for macros */
	mov x19, x0      /* ctx (but x0 stays as CTX) */
	mov x20, x1      /* out */
	mov x21, x2      /* in */

	/* Allocate stack space: 64 (temp) + 256 (AB+CD) = 320 bytes (aligned to 16) */
	sub sp, sp, #320

	/* Calculate AB/CD buffer addresses */
	add x22, sp, #64      /* x22 = AB buffer base */
	add x23, sp, #192     /* x23 = CD buffer base (64 + 128) */

	/*==================================================================
	 * PHASE 1: inpack16 (input + pre-whitening)
	 *==================================================================*/

	/* Load pre-whitening key: key_table[0] */
	ldr x24, [CTX, #key_table]

	/* Call inpack16_pre: load 16 blocks, XOR with whitening key */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x21, x24, v16, x26

	/* Call inpack16_post: byteslice and store to AB/CD buffers */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x22, x23, v17, v18, x26

	/* 🔴 CRITICAL FIX: inpack16_post modifies x22/x23 with post-increment stores,
	 * so we must reset them to buffer base addresses before enc_rounds16 */
	add x22, sp, #64      /* Reset x22 = AB buffer base */
	add x23, sp, #192     /* Reset x23 = CD buffer base */

	/*==================================================================
	 * PHASE 2: First 6 rounds (keys 2-7)
	 *==================================================================*/

	/* Load AB to v0-v7 */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	/* Load CD to v8-v15 */
	ldp q8, q9, [x23, #0]
	ldp q10, q11, [x23, #32]
	ldp q12, q13, [x23, #64]
	ldp q14, q15, [x23, #96]

	/* Execute 6 rounds: two_roundsm16 with keys 2,3,4,5,6,7 */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x23, 0

	/*==================================================================
	 * PHASE 3: FL1 (keys 8-9)
	 *==================================================================*/

	/* Load AB from buffer (updated by enc_rounds16) */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	/* Calculate key addresses: key_table[8] and key_table[9] */
	add x24, CTX, #key_table
	add x24, x24, #(8*8)      /* x24 = &key_table[8] (kl) */
	add x25, CTX, #key_table
	add x25, x25, #(9*8)      /* x25 = &key_table[9] (kr) */

	/* Call fls16: FL transformation on AB, using CD as mem_r */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x23, x24, x25, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store updated AB back to buffer */
	stp q0, q1, [x22, #0]
	stp q2, q3, [x22, #32]
	stp q4, q5, [x22, #64]
	stp q6, q7, [x22, #96]

	/*==================================================================
	 * PHASE 4: Second 6 rounds (keys 10-15)
	 *==================================================================*/

	/* Load AB/CD (AB was just updated by FL1) */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	ldp q8, q9, [x23, #0]
	ldp q10, q11, [x23, #32]
	ldp q12, q13, [x23, #64]
	ldp q14, q15, [x23, #96]

	/* Execute 6 rounds: keys 10,11,12,13,14,15 (i=8 → i+2=10) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x23, 8

	/*==================================================================
	 * PHASE 5: FL2 (keys 16-17)
	 *==================================================================*/

	/* Load AB from buffer */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	/* Calculate key addresses: key_table[16] and key_table[17] */
	add x24, CTX, #key_table
	add x24, x24, #(16*8)     /* x24 = &key_table[16] (kl) */
	add x25, CTX, #key_table
	add x25, x25, #(17*8)     /* x25 = &key_table[17] (kr) */

	/* Call fls16: FL transformation */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x23, x24, x25, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store updated AB */
	stp q0, q1, [x22, #0]
	stp q2, q3, [x22, #32]
	stp q4, q5, [x22, #64]
	stp q6, q7, [x22, #96]

	/*==================================================================
	 * PHASE 6: Third 6 rounds (keys 18-23)
	 *==================================================================*/

	/* Load AB/CD */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	ldp q8, q9, [x23, #0]
	ldp q10, q11, [x23, #32]
	ldp q12, q13, [x23, #64]
	ldp q14, q15, [x23, #96]

	/* Execute 6 rounds: keys 18,19,20,21,22,23 (i=16 → i+2=18) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x23, 16

	/*==================================================================
	 * PHASE 7: outunpack16 (byteslice back + post-whitening)
	 *==================================================================*/

	/* Load final AB/CD from buffers to v0-v15 */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	ldp q8, q9, [x23, #0]
	ldp q10, q11, [x23, #32]
	ldp q12, q13, [x23, #64]
	ldp q14, q15, [x23, #96]

	/* Load post-whitening key: key_table[24] (for 128-bit) */
	ldr x24, [CTX, #(key_table + 24*8)]

	/* Call outunpack16: byteslice back + XOR with kw_last */
	outunpack16 v0, v1, v2, v3, v4, v5, v6, v7, \
	            v8, v9, v10, v11, v12, v13, v14, v15, \
	            x24, v16, v17, x26

	/* Store 16 output blocks to out */
	st1 {v0.16b},  [x20], #16
	st1 {v1.16b},  [x20], #16
	st1 {v2.16b},  [x20], #16
	st1 {v3.16b},  [x20], #16
	st1 {v4.16b},  [x20], #16
	st1 {v5.16b},  [x20], #16
	st1 {v6.16b},  [x20], #16
	st1 {v7.16b},  [x20], #16
	st1 {v8.16b},  [x20], #16
	st1 {v9.16b},  [x20], #16
	st1 {v10.16b}, [x20], #16
	st1 {v11.16b}, [x20], #16
	st1 {v12.16b}, [x20], #16
	st1 {v13.16b}, [x20], #16
	st1 {v14.16b}, [x20], #16
	st1 {v15.16b}, [x20]

	/*==================================================================
	 * Cleanup and return
	 *==================================================================*/

	/* Restore stack */
	add sp, sp, #320

	/* Restore callee-saved registers */
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16

	/* Restore frame pointer and return */
	ldp x29, x30, [sp], #16
	ret
.size camellia_encrypt_16blks_simd128_asm_temp, .-camellia_encrypt_16blks_simd128_asm_temp

/* Stubs for decrypt and keysetup (TODO: implement later)
 *
 *.align 4
 *.global camellia_decrypt_16blks_simd128
 *.type camellia_decrypt_16blks_simd128, %function
 *
 *camellia_decrypt_16blks_simd128:
 *	ret
 *.size camellia_decrypt_16blks_simd128, .-camellia_decrypt_16blks_simd128
 *
 *.align 4
 *.global camellia_keysetup_simd128
 *.type camellia_keysetup_simd128, %function
 *
 *camellia_keysetup_simd128:
 *	str w2, [x0, #272]
 *	mov w0, #0
 *	ret
 *.size camellia_keysetup_simd128, .-camellia_keysetup_simd128
 */

/**********************************************************************
 * TEST WRAPPER: two_roundsm16_asm_wrapper
 *
 * Simplified wrapper that calls roundsm16 twice manually.
 *
 * C signature:
 *   void two_roundsm16_asm_wrapper(uint8x16_t ab[8],
 *                                   uint8x16_t cd[8],
 *                                   uint64_t key1,
 *                                   uint64_t key2);
 *
 * Parameters:
 *   x0: pointer to AB state (8 x 16 bytes = 128 bytes)
 *   x1: pointer to CD state (8 x 16 bytes = 128 bytes)
 *   x2: key1 (64-bit)
 *   x3: key2 (64-bit)
 **********************************************************************/

.align 4
.global two_roundsm16_asm_wrapper
.type two_roundsm16_asm_wrapper, %function

two_roundsm16_asm_wrapper:
	/* SIMPLIFIED VERSION - just test roundsm16 once */
	/* Save output pointers */
	stp x19, x20, [sp, #-16]!

	mov x19, x0              /* AB pointer */
	mov x20, x1              /* CD pointer */

	/* Allocate stack for key */
	sub sp, sp, #16
	str x2, [sp]             /* key1 */

	/* Load AB into v0-v7 */
	ldp q0, q1, [x19, #0]
	ldp q2, q3, [x19, #32]
	ldp q4, q5, [x19, #64]
	ldp q6, q7, [x19, #96]

	/* Call roundsm16: F(AB, key1) XOR old_CD → new_CD */
	/* mem_cd = x20 (CD pointer), key_addr = sp */
	roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x20, sp

	/* Store new_CD to CD memory */
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Round 2: new_AB = old_AB ⊕ F(new_CD, key2) */
	/* v0-v7 already contain new_CD from round 1 */
	/* Store key2 to stack */
	str x3, [sp]             /* key2 */

	/* Call roundsm16: F(new_CD, key2) XOR old_AB → new_AB */
	/* mem_cd = x19 (AB pointer), key_addr = sp */
	roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x19, sp

	/* Store new_AB to AB output */
	stp q0, q1, [x19, #0]
	stp q2, q3, [x19, #32]
	stp q4, q5, [x19, #64]
	stp q6, q7, [x19, #96]

	/* Clean up */
	add sp, sp, #16
	ldp x19, x20, [sp], #16

	ret
.size two_roundsm16_asm_wrapper, .-two_roundsm16_asm_wrapper

/**********************************************************************
 * TEST WRAPPER: byteslice_16x16b_asm_wrapper
 *
 * Wrapper for testing byteslice_16x16b_fast macro.
 *
 * C signature:
 *   void byteslice_16x16b_asm_wrapper(uint8_t output[16][16],
 *                                     const uint8_t input[16][16]);
 *
 * Parameters:
 *   x0: pointer to output array (16 x 16 bytes)
 *   x1: pointer to input array (16 x 16 bytes)
 *
 * This wrapper:
 * 1. Loads 16 vectors from input array
 * 2. Calls byteslice_16x16b_fast
 * 3. Stores 16 vectors to output array
 **********************************************************************/
.text
.align 4
.globl byteslice_16x16b_asm_wrapper
.type byteslice_16x16b_asm_wrapper, %function
byteslice_16x16b_asm_wrapper:
	/* Save callee-saved registers we'll use */
	stp x19, x20, [sp, #-16]!

	/* Save output pointer */
	mov x19, x0

	/* Load 16 input vectors */
	ld1 {v0.16b}, [x1], #16
	ld1 {v1.16b}, [x1], #16
	ld1 {v2.16b}, [x1], #16
	ld1 {v3.16b}, [x1], #16
	ld1 {v4.16b}, [x1], #16
	ld1 {v5.16b}, [x1], #16
	ld1 {v6.16b}, [x1], #16
	ld1 {v7.16b}, [x1], #16
	ld1 {v8.16b}, [x1], #16
	ld1 {v9.16b}, [x1], #16
	ld1 {v10.16b}, [x1], #16
	ld1 {v11.16b}, [x1], #16
	ld1 {v12.16b}, [x1], #16
	ld1 {v13.16b}, [x1], #16
	ld1 {v14.16b}, [x1], #16
	ld1 {v15.16b}, [x1], #16

	/* Call byteslice_16x16b_fast
	 * Parameters must be interleaved: a0, b0, c0, d0, a1, b1, c1, d1, ...
	 * Loaded vectors: v0=input[0], v1=input[1], ..., v15=input[15]
	 * Mapping: a0,a1,a2,a3 = v0,v1,v2,v3
	 *          b0,b1,b2,b3 = v4,v5,v6,v7
	 *          c0,c1,c2,c3 = v8,v9,v10,v11
	 *          d0,d1,d2,d3 = v12,v13,v14,v15
	 * Interleaved order: v0,v4,v8,v12, v1,v5,v9,v13, v2,v6,v10,v14, v3,v7,v11,v15
	 */
	byteslice_16x16b_fast \
		v0, v4, v8, v12, v1, v5, v9, v13, \
		v2, v6, v10, v14, v3, v7, v11, v15, \
		v16, v17, x20

	/* Store 16 output vectors */
	st1 {v0.16b}, [x19], #16
	st1 {v1.16b}, [x19], #16
	st1 {v2.16b}, [x19], #16
	st1 {v3.16b}, [x19], #16
	st1 {v4.16b}, [x19], #16
	st1 {v5.16b}, [x19], #16
	st1 {v6.16b}, [x19], #16
	st1 {v7.16b}, [x19], #16
	st1 {v8.16b}, [x19], #16
	st1 {v9.16b}, [x19], #16
	st1 {v10.16b}, [x19], #16
	st1 {v11.16b}, [x19], #16
	st1 {v12.16b}, [x19], #16
	st1 {v13.16b}, [x19], #16
	st1 {v14.16b}, [x19], #16
	st1 {v15.16b}, [x19], #16

	/* Restore callee-saved registers */
	ldp x19, x20, [sp], #16

	ret
.size byteslice_16x16b_asm_wrapper, .-byteslice_16x16b_asm_wrapper

/**********************************************************************
 * TEST WRAPPER: inpack16_asm_wrapper
 *
 * Wrapper for testing inpack16_pre + inpack16_post macros.
 *
 * C signature:
 *   void inpack16_asm_wrapper(uint8_t ab_out[128],
 *                              uint8_t cd_out[128],
 *                              const uint8_t input[256],
 *                              uint64_t whitening_key);
 *
 * Parameters:
 *   x0: pointer to AB output buffer (128 bytes = 8 vectors)
 *   x1: pointer to CD output buffer (128 bytes = 8 vectors)
 *   x2: pointer to input array (256 bytes = 16 blocks)
 *   x3: whitening key (64-bit value)
 *
 * This wrapper:
 * 1. Calls inpack16_pre to load and XOR with whitening key
 * 2. Calls inpack16_post to byteslice and store to AB/CD buffers
 **********************************************************************/
.text
.align 4
.globl inpack16_asm_wrapper
.type inpack16_asm_wrapper, %function
inpack16_asm_wrapper:
	/* Save callee-saved registers and allocate stack space */
	/* Need 48 bytes: x19-x22 (32 bytes) + whitening key temp (16 bytes) */
	stp x19, x20, [sp, #-48]!
	stp x21, x22, [sp, #16]

	/* Save output pointers */
	mov x19, x0  /* AB buffer */
	mov x20, x1  /* CD buffer */

	/* Call inpack16_pre */
	/* Parameters: v0-v15 (output), x2 (input), x3 (key), v16 (tmp), x21 (x_tmp) */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x2, x3, v16, x21

	/* Call inpack16_post */
	/* Parameters: v0-v15 (input/output), x19 (AB), x20 (CD), v17, v18 (tmp), x21 (x_tmp) */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x19, x20, v17, v18, x21

	/* Restore callee-saved registers */
	ldp x21, x22, [sp, #16]
	ldp x19, x20, [sp], #48

	ret
.size inpack16_asm_wrapper, .-inpack16_asm_wrapper

/**********************************************************************
 * TEST WRAPPER: outunpack16_asm_wrapper
 *
 * Wrapper for testing outunpack16 macro.
 *
 * C signature:
 *   void outunpack16_asm_wrapper(uint8_t output[256],
 *                                 const uint8_t ab_in[128],
 *                                 const uint8_t cd_in[128],
 *                                 uint64_t whitening_key);
 *
 * Parameters:
 *   x0: pointer to output array (256 bytes = 16 blocks)
 *   x1: pointer to AB input buffer (128 bytes = 8 vectors)
 *   x2: pointer to CD input buffer (128 bytes = 8 vectors)
 *   x3: whitening key (64-bit value)
 *
 * This wrapper:
 * 1. Loads AB/CD buffers to registers
 * 2. Calls outunpack16 to byteslice and XOR with whitening key
 * 3. Stores 16 blocks to output
 **********************************************************************/
.text
.align 4
.globl outunpack16_asm_wrapper
.type outunpack16_asm_wrapper, %function
outunpack16_asm_wrapper:
	/* Save callee-saved registers and allocate stack space */
	/* Need space for 2 temporary vectors (32 bytes) */
	stp x19, x20, [sp, #-48]!
	stp x21, x22, [sp, #16]
	add x19, sp, #32  /* Pointer to stack temp area */

	/* Save output pointer */
	mov x20, x0

	/* Load AB buffer (8 vectors) to v0-v7 */
	ld1 {v0.16b, v1.16b}, [x1], #32
	ld1 {v2.16b, v3.16b}, [x1], #32
	ld1 {v4.16b, v5.16b}, [x1], #32
	ld1 {v6.16b, v7.16b}, [x1]

	/* Load CD buffer (8 vectors) to v8-v15 */
	ld1 {v8.16b, v9.16b}, [x2], #32
	ld1 {v10.16b, v11.16b}, [x2], #32
	ld1 {v12.16b, v13.16b}, [x2], #32
	ld1 {v14.16b, v15.16b}, [x2]

	/* Call outunpack16 */
	/* Parameters: v0-v15 (input/output), x3 (key), v16, v17 (tmp), x21 (x_tmp) */
	outunpack16 v0, v1, v2, v3, v4, v5, v6, v7, \
	            v8, v9, v10, v11, v12, v13, v14, v15, \
	            x3, v16, v17, x21

	/* Store 16 blocks to output (sequential order: v0-v15) */
	st1 {v0.16b}, [x20], #16
	st1 {v1.16b}, [x20], #16
	st1 {v2.16b}, [x20], #16
	st1 {v3.16b}, [x20], #16
	st1 {v4.16b}, [x20], #16
	st1 {v5.16b}, [x20], #16
	st1 {v6.16b}, [x20], #16
	st1 {v7.16b}, [x20], #16
	st1 {v8.16b}, [x20], #16
	st1 {v9.16b}, [x20], #16
	st1 {v10.16b}, [x20], #16
	st1 {v11.16b}, [x20], #16
	st1 {v12.16b}, [x20], #16
	st1 {v13.16b}, [x20], #16
	st1 {v14.16b}, [x20], #16
	st1 {v15.16b}, [x20]

	/* Restore callee-saved registers */
	ldp x21, x22, [sp, #16]
	ldp x19, x20, [sp], #48

	ret
.size outunpack16_asm_wrapper, .-outunpack16_asm_wrapper

/**********************************************************************
 * TEST FUNCTION: test_whitening_key_asm
 *
 * Debug function to test whitening key construction.
 *
 * C signature:
 *   void test_whitening_key_asm(uint64_t key, uint8_t output[16]);
 *
 * Parameters:
 *   x0: whitening key (64-bit value)
 *   x1: pointer to output buffer (16 bytes)
 **********************************************************************/
.text
.align 4
.globl test_whitening_key_asm
.type test_whitening_key_asm, %function
test_whitening_key_asm:
	/* Allocate 16 bytes on stack for whitening key construction */
	sub sp, sp, #16

	/* Store key (8 bytes) to stack */
	str x0, [sp]
	/* Store 8 zero bytes */
	mov x2, xzr
	str x2, [sp, #8]
	/* Load complete 16-byte vector */
	ld1 {v0.16b}, [sp]

	/* Store result to output */
	st1 {v0.16b}, [x1]

	/* Restore stack */
	add sp, sp, #16
	ret
.size test_whitening_key_asm, .-test_whitening_key_asm

/**********************************************************************
 * TEST FUNCTION: inpack16_pre_test
 *
 * Test pre-whitening only (no byteslice).
 *
 * C signature:
 *   void inpack16_pre_test(const uint8_t input[256],
 *                           uint64_t whitening_key,
 *                           uint8_t output[256]);
 *
 * Parameters:
 *   x0: pointer to input array (256 bytes = 16 blocks)
 *   x1: whitening key (64-bit value)
 *   x2: pointer to output array (256 bytes = 16 blocks)
 **********************************************************************/
.text
.align 4
.globl inpack16_pre_test
.type inpack16_pre_test, %function
inpack16_pre_test:
	stp x19, x20, [sp, #-48]!

	/* Construct whitening key on stack */
	str x1, [sp, #32]
	mov x19, xzr
	str x19, [sp, #40]
	add x19, sp, #32
	ld1 {v16.16b}, [x19]

	/* Load 16 blocks, XOR with key, and store */
	ld1 {v0.16b}, [x0], #16
	eor v0.16b, v0.16b, v16.16b
	st1 {v0.16b}, [x2], #16

	ld1 {v1.16b}, [x0], #16
	eor v1.16b, v1.16b, v16.16b
	st1 {v1.16b}, [x2], #16

	ld1 {v2.16b}, [x0], #16
	eor v2.16b, v2.16b, v16.16b
	st1 {v2.16b}, [x2], #16

	ld1 {v3.16b}, [x0], #16
	eor v3.16b, v3.16b, v16.16b
	st1 {v3.16b}, [x2], #16

	ld1 {v4.16b}, [x0], #16
	eor v4.16b, v4.16b, v16.16b
	st1 {v4.16b}, [x2], #16

	ld1 {v5.16b}, [x0], #16
	eor v5.16b, v5.16b, v16.16b
	st1 {v5.16b}, [x2], #16

	ld1 {v6.16b}, [x0], #16
	eor v6.16b, v6.16b, v16.16b
	st1 {v6.16b}, [x2], #16

	ld1 {v7.16b}, [x0], #16
	eor v7.16b, v7.16b, v16.16b
	st1 {v7.16b}, [x2], #16

	ld1 {v8.16b}, [x0], #16
	eor v8.16b, v8.16b, v16.16b
	st1 {v8.16b}, [x2], #16

	ld1 {v9.16b}, [x0], #16
	eor v9.16b, v9.16b, v16.16b
	st1 {v9.16b}, [x2], #16

	ld1 {v10.16b}, [x0], #16
	eor v10.16b, v10.16b, v16.16b
	st1 {v10.16b}, [x2], #16

	ld1 {v11.16b}, [x0], #16
	eor v11.16b, v11.16b, v16.16b
	st1 {v11.16b}, [x2], #16

	ld1 {v12.16b}, [x0], #16
	eor v12.16b, v12.16b, v16.16b
	st1 {v12.16b}, [x2], #16

	ld1 {v13.16b}, [x0], #16
	eor v13.16b, v13.16b, v16.16b
	st1 {v13.16b}, [x2], #16

	ld1 {v14.16b}, [x0], #16
	eor v14.16b, v14.16b, v16.16b
	st1 {v14.16b}, [x2], #16

	ld1 {v15.16b}, [x0], #16
	eor v15.16b, v15.16b, v16.16b
	st1 {v15.16b}, [x2], #16

	ldp x19, x20, [sp], #48
	ret
.size inpack16_pre_test, .-inpack16_pre_test

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack, "", %progbits
#endif


/**********************************************************************
 * DEBUG STAGE FUNCTIONS - For C vs ASM intermediate state comparison
 **********************************************************************/

/*
 * camellia_simd128_step1_asm - Stop after inpack16 (Stage 1)
 *
 * C signature:
 *   void camellia_simd128_step1_asm(struct camellia_simd_ctx *ctx,
 *                                    uint8_t ab_out[128],
 *                                    uint8_t cd_out[128],
 *                                    const uint8_t in[256]);
 *
 * Parameters:
 *   x0 (CTX): pointer to context
 *   x1: pointer to ab_out buffer (128 bytes)
 *   x2: pointer to cd_out buffer (128 bytes)
 *   x3: pointer to input (256 bytes)
 */
.align 4
.text
.global camellia_simd128_step1_asm
.type camellia_simd128_step1_asm, %function
camellia_simd128_step1_asm:
	/* Prologue: identical to full encryption */
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!

	/* Save parameters */
	mov x19, x0      /* ctx (keep in x0 for macros) */
	mov x20, x1      /* ab_out */
	mov x21, x2      /* cd_out */
	mov x24, x3      /* in */

	/* Allocate stack: 320 bytes (64 temp + 128 AB + 128 CD) */
	sub sp, sp, #320
	add x22, sp, #64      /* x22 = AB buffer base */
	add x23, sp, #192     /* x23 = CD buffer base */

	/* PHASE 1: inpack16 (load + pre-whitening + byteslice) */
	ldr x25, [CTX, #key_table]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x24, x25, v16, x26

	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x22, x23, v17, v18, x26

	/* Reset pointers after inpack16_post (it uses post-increment) */
	add x22, sp, #64
	add x23, sp, #192

	/* Copy AB buffer (128 bytes) to ab_out */
	ldp q0, q1, [x22, #0]
	stp q0, q1, [x20, #0]
	ldp q2, q3, [x22, #32]
	stp q2, q3, [x20, #32]
	ldp q4, q5, [x22, #64]
	stp q4, q5, [x20, #64]
	ldp q6, q7, [x22, #96]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer (128 bytes) to cd_out */
	ldp q8, q9, [x23, #0]
	stp q8, q9, [x21, #0]
	ldp q10, q11, [x23, #32]
	stp q10, q11, [x21, #32]
	ldp q12, q13, [x23, #64]
	stp q12, q13, [x21, #64]
	ldp q14, q15, [x23, #96]
	stp q14, q15, [x21, #96]

	/* Cleanup and return */
	add sp, sp, #320
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16
	ldp x29, x30, [sp], #16
	ret
.size camellia_simd128_step1_asm, .-camellia_simd128_step1_asm

/*
 * camellia_simd128_step2_asm - Stop after first enc_rounds16 (Stage 2)
 *
 * C signature:
 *   void camellia_simd128_step2_asm(struct camellia_simd_ctx *ctx,
 *                                    uint8_t ab_out[128],
 *                                    uint8_t cd_out[128],
 *                                    const uint8_t in[256]);
 */
.align 4
.text
.global camellia_simd128_step2_asm
.type camellia_simd128_step2_asm, %function
camellia_simd128_step2_asm:
	/* Prologue */
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!

	/* Save parameters */
	mov x19, x0
	mov x20, x1      /* ab_out */
	mov x21, x2      /* cd_out */
	mov x24, x3      /* in */

	/* Allocate stack */
	sub sp, sp, #320
	add x22, sp, #64
	add x23, sp, #192

	/* PHASE 1: inpack16 */
	ldr x25, [CTX, #key_table]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x24, x25, v16, x26

	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x22, x23, v17, v18, x26

	add x22, sp, #64
	add x23, sp, #192

	/* PHASE 2: First 6 rounds (keys 2-7) */
	ldp q0, q1, [x22, #0]
	ldp q2, q3, [x22, #32]
	ldp q4, q5, [x22, #64]
	ldp q6, q7, [x22, #96]

	ldp q8, q9, [x23, #0]
	ldp q10, q11, [x23, #32]
	ldp q12, q13, [x23, #64]
	ldp q14, q15, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x23, 0

	/* Copy AB buffer to ab_out */
	ldp q0, q1, [x22, #0]
	stp q0, q1, [x20, #0]
	ldp q2, q3, [x22, #32]
	stp q2, q3, [x20, #32]
	ldp q4, q5, [x22, #64]
	stp q4, q5, [x20, #64]
	ldp q6, q7, [x22, #96]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer to cd_out */
	ldp q8, q9, [x23, #0]
	stp q8, q9, [x21, #0]
	ldp q10, q11, [x23, #32]
	stp q10, q11, [x21, #32]
	ldp q12, q13, [x23, #64]
	stp q12, q13, [x21, #64]
	ldp q14, q15, [x23, #96]
	stp q14, q15, [x21, #96]

	/* Cleanup */
	add sp, sp, #320
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16
	ldp x29, x30, [sp], #16
	ret
.size camellia_simd128_step2_asm, .-camellia_simd128_step2_asm

/*
 * camellia_inpack16_pre_stage_asm - Stage 1a: Only pre-whitening, no byteslice
 *
 * C signature:
 *   void camellia_inpack16_pre_stage_asm(struct camellia_simd_ctx *ctx,
 *                                         const uint8_t in[256],
 *                                         uint8_t out[256]);
 */
.text
.align 4
.global camellia_inpack16_pre_stage_asm
.type camellia_inpack16_pre_stage_asm, %function
camellia_inpack16_pre_stage_asm:
	/* Prologue: match main function's stack layout */
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!

	/* Save parameters */
	mov x19, x0      /* ctx (keep in x0 for CTX macro) */
	mov x20, x1      /* in */
	mov x21, x2      /* out */

	/* Allocate stack: 320 bytes (same as main function) */
	sub sp, sp, #320

	/* Load whitening key: key_table[0] */
	ldr x24, [CTX, #key_table]

	/* Call inpack16_pre: load 16 blocks + apply whitening */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x20, x24, v16, x26

	/* Write output in correct order matching C inpack16_pre macro:
	 * C stores: y7, y6, ..., y0, x7, x6, ..., x0
	 * Which maps to: v15, v14, ..., v8, v7, v6, ..., v0 */
	st1 {v15.16b}, [x21], #16  /* y7 */
	st1 {v14.16b}, [x21], #16  /* y6 */
	st1 {v13.16b}, [x21], #16  /* y5 */
	st1 {v12.16b}, [x21], #16  /* y4 */
	st1 {v11.16b}, [x21], #16  /* y3 */
	st1 {v10.16b}, [x21], #16  /* y2 */
	st1 {v9.16b},  [x21], #16  /* y1 */
	st1 {v8.16b},  [x21], #16  /* y0 */
	st1 {v7.16b},  [x21], #16  /* x7 */
	st1 {v6.16b},  [x21], #16  /* x6 */
	st1 {v5.16b},  [x21], #16  /* x5 */
	st1 {v4.16b},  [x21], #16  /* x4 */
	st1 {v3.16b},  [x21], #16  /* x3 */
	st1 {v2.16b},  [x21], #16  /* x2 */
	st1 {v1.16b},  [x21], #16  /* x1 */
	st1 {v0.16b},  [x21]        /* x0 */

	/* Cleanup */
	add sp, sp, #320
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16
	ldp x29, x30, [sp], #16
	ret
.size camellia_inpack16_pre_stage_asm, .-camellia_inpack16_pre_stage_asm

/*
 * camellia_inpack16_post_stage_asm - Stage 1b: Only byteslice (no pre-whitening)
 *
 * C signature:
 *   void camellia_inpack16_post_stage_asm(const uint8_t in[256],
 *                                          uint8_t ab_out[128],
 *                                          uint8_t cd_out[128]);
 *
 * Parameters:
 *   x0: pointer to pre-whitened input (256 bytes = 16 blocks)
 *   x1: pointer to AB output buffer (128 bytes)
 *   x2: pointer to CD output buffer (128 bytes)
 *
 * This tests ONLY the byteslicing operation (inpack16_post).
 */
.text
.align 4
.global camellia_inpack16_post_stage_asm
.type camellia_inpack16_post_stage_asm, %function
camellia_inpack16_post_stage_asm:
	/* Prologue */
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	stp x19, x20, [sp, #-16]!
	stp x21, x22, [sp, #-16]!
	stp x23, x24, [sp, #-16]!
	stp x25, x26, [sp, #-16]!

	/* Save parameters */
	mov x19, x0      /* in */
	mov x20, x1      /* ab_out */
	mov x21, x2      /* cd_out */

	/* Allocate stack: 320 bytes (64 temp + 128 AB + 128 CD) */
	sub sp, sp, #320
	add x22, sp, #64      /* x22 = AB buffer base */
	add x23, sp, #192     /* x23 = CD buffer base */

	/* Load 16 pre-whitened blocks into v0-v15 */
	ld1 {v0.16b},  [x19], #16
	ld1 {v1.16b},  [x19], #16
	ld1 {v2.16b},  [x19], #16
	ld1 {v3.16b},  [x19], #16
	ld1 {v4.16b},  [x19], #16
	ld1 {v5.16b},  [x19], #16
	ld1 {v6.16b},  [x19], #16
	ld1 {v7.16b},  [x19], #16
	ld1 {v8.16b},  [x19], #16
	ld1 {v9.16b},  [x19], #16
	ld1 {v10.16b}, [x19], #16
	ld1 {v11.16b}, [x19], #16
	ld1 {v12.16b}, [x19], #16
	ld1 {v13.16b}, [x19], #16
	ld1 {v14.16b}, [x19], #16
	ld1 {v15.16b}, [x19]

	/* Call inpack16_post to perform byteslicing */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x22, x23, v17, v18, x26

	/* Reset pointers (inpack16_post may use post-increment) */
	add x22, sp, #64
	add x23, sp, #192

	/* Copy AB buffer (128 bytes) to ab_out */
	ldp q0, q1, [x22, #0]
	stp q0, q1, [x20, #0]
	ldp q2, q3, [x22, #32]
	stp q2, q3, [x20, #32]
	ldp q4, q5, [x22, #64]
	stp q4, q5, [x20, #64]
	ldp q6, q7, [x22, #96]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer (128 bytes) to cd_out */
	ldp q8, q9, [x23, #0]
	stp q8, q9, [x21, #0]
	ldp q10, q11, [x23, #32]
	stp q10, q11, [x21, #32]
	ldp q12, q13, [x23, #64]
	stp q12, q13, [x21, #64]
	ldp q14, q15, [x23, #96]
	stp q14, q15, [x21, #96]

	/* Cleanup */
	add sp, sp, #320
	ldp x25, x26, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x19, x20, [sp], #16
	ldp x29, x30, [sp], #16
	ret
.size camellia_inpack16_post_stage_asm, .-camellia_inpack16_post_stage_asm

/**********************************************************************
 * write_output macro - Store 16 vector registers to memory
 *
 * Parameters:
 *   x0-x15: 16 source registers to store
 *   mem: destination memory address
 **********************************************************************/
.macro write_output x0, x1, x2, x3, x4, x5, x6, x7, \
                    x8, x9, x10, x11, x12, x13, x14, x15, mem
	st1 {\x0\().16b}, [\mem], #16
	st1 {\x1\().16b}, [\mem], #16
	st1 {\x2\().16b}, [\mem], #16
	st1 {\x3\().16b}, [\mem], #16
	st1 {\x4\().16b}, [\mem], #16
	st1 {\x5\().16b}, [\mem], #16
	st1 {\x6\().16b}, [\mem], #16
	st1 {\x7\().16b}, [\mem], #16
	st1 {\x8\().16b}, [\mem], #16
	st1 {\x9\().16b}, [\mem], #16
	st1 {\x10\().16b}, [\mem], #16
	st1 {\x11\().16b}, [\mem], #16
	st1 {\x12\().16b}, [\mem], #16
	st1 {\x13\().16b}, [\mem], #16
	st1 {\x14\().16b}, [\mem], #16
	st1 {\x15\().16b}, [\mem]
.endm

/**********************************************************************
 * camellia_encrypt_16blks_simd128 - Encrypt 16 blocks using SIMD
 *
 * C signature:
 *   void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
 *                                         void *out, const void *in);
 *
 * Parameters:
 *   x0: ctx (struct camellia_simd_ctx *)
 *   x1: dst (output buffer, 16 * 16 = 256 bytes)
 *   x2: src (input buffer, 16 * 16 = 256 bytes)
 **********************************************************************/
.text
.align 4
/*
 * camellia_encrypt_16blks_simd128 - Main 16-block encryption entry point
 *
 * This is now just a wrapper that jumps to the Step7 implementation,
 * which is the complete, verified encryption function.
 *
 * Parameters:
 *   x0 = ctx (struct camellia_simd_ctx *)
 *   x1 = out (256 bytes)
 *   x2 = in (256 bytes)
 */
.globl camellia_encrypt_16blks_simd128
.type camellia_encrypt_16blks_simd128, %function
camellia_encrypt_16blks_simd128:
	/* Jump to Step7 implementation - parameters already in correct registers */
	b camellia_simd128_step7_asm
.size camellia_encrypt_16blks_simd128, .-camellia_encrypt_16blks_simd128

/**********************************************************************
 * TEST WRAPPER: fls16_asm_wrapper
 *
 * C signature:
 *   void fls16_asm_wrapper(uint8_t l[128],
 *                          uint8_t r[128],
 *                          uint64_t kl,
 *                          uint64_t kr);
 *
 * Parameters:
 *   x0: pointer to L state (128 bytes, 8 x 16-byte vectors)
 *   x1: pointer to R state (128 bytes, 8 x 16-byte vectors)
 *   x2: 64-bit FL key kl
 *   x3: 64-bit FL key kr
 **********************************************************************/
.text
.align 4
.globl fls16_asm_wrapper
.type fls16_asm_wrapper, %function
fls16_asm_wrapper:
	/* Save registers and allocate stack space:
	 * - 16 bytes for kl/kr storage
	 * - 16 bytes for saved registers
	 */
	stp x19, x20, [sp, #-32]!
	stp x21, x22, [sp, #16]

	/* Allocate 16 bytes for kl/kr on stack */
	sub sp, sp, #16
	str x2, [sp, #0]    /* kl at [sp+0] */
	str x3, [sp, #8]    /* kr at [sp+8] */

	/* Save L and R pointers */
	mov x19, x0   /* L pointer */
	mov x20, x1   /* R pointer */

	/* Load L state (AB) into v0-v7 */
	ldp q0, q1, [x19, #0]
	ldp q2, q3, [x19, #32]
	ldp q4, q5, [x19, #64]
	ldp q6, q7, [x19, #96]

	/* Call fls16 macro
	 * L state: v0-v7 (modified in-place)
	 * R buffer: x20 (modified in memory)
	 * mem_kl: pointer to kl on stack [sp+0]
	 * mem_kr: pointer to kr on stack [sp+8]
	 * Temps: v16-v23
	 */
	mov x21, x20        /* mem_r = R pointer */
	mov x22, sp         /* mem_kl = &kl_stack */
	add x23, sp, #8     /* mem_kr = &kr_stack */

	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x21, x22, x23, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store L state back */
	stp q0, q1, [x19, #0]
	stp q2, q3, [x19, #32]
	stp q4, q5, [x19, #64]
	stp q6, q7, [x19, #96]

	/* R state was modified in-place at [x20], no need to store back */

	/* Release kl/kr stack space */
	add sp, sp, #16

	/* Restore registers */
	ldp x21, x22, [sp, #16]
	ldp x19, x20, [sp], #32
	ret
.size fls16_asm_wrapper, .-fls16_asm_wrapper

/**********************************************************************
 * TEST WRAPPER: test_roundsm16_asm
 *
 * C signature:
 *   void test_roundsm16_asm(uint8_t x_inout[128],
 *                           uint8_t cd_in[128],
 *                           uint64_t key);
 *
 * Parameters:
 *   x0: pointer to X input/output (128 bytes, 8 x 16-byte vectors)
 *   x1: pointer to CD input (128 bytes, 8 x 16-byte vectors)
 *   x2: 64-bit round key
 **********************************************************************/
.text
.align 4
.globl test_roundsm16_asm
.type test_roundsm16_asm, %function
test_roundsm16_asm:
	stp x19, x20, [sp, #-32]!
	str x2, [sp, #16]

	/* Load X input */
	ldp q0, q1, [x0, #0]
	ldp q2, q3, [x0, #32]
	ldp q4, q5, [x0, #64]
	ldp q6, q7, [x0, #96]

	/* x1 points to CD */
	mov x19, x0          /* Save X output pointer */
	add x12, sp, #16     /* Key address */

	/* Call roundsm16 */
	roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	          v16, v17, v18, v19, v20, v21, v22, v23, \
	          x1, x12

	/* Store X output */
	stp q0, q1, [x19, #0]
	stp q2, q3, [x19, #32]
	stp q4, q5, [x19, #64]
	stp q6, q7, [x19, #96]

	ldp x19, x20, [sp], #32
	ret
.size test_roundsm16_asm, .-test_roundsm16_asm

/**********************************************************************
 * TEST FUNCTION: camellia_simd128_step3_asm
 *
 * Step3: Encryption up to FL1 (for debugging)
 *   1. inpack16 (pre-whitening + byteslice)
 *   2. enc_rounds16: 6 rounds using key_table[2..7]
 *   3. fls16: FL1 using key_table[8], key_table[9]
 *   4. Output AB/CD buffers
 *
 * C signature:
 *   void camellia_simd128_step3_asm(struct camellia_simd_ctx *ctx,
 *                                   uint8_t ab_out[128],
 *                                   uint8_t cd_out[128],
 *                                   const uint8_t in[256]);
 *
 * Parameters:
 *   x0: CTX pointer
 *   x1: ab_out pointer
 *   x2: cd_out pointer
 *   x3: input pointer (256 bytes = 16 blocks)
 **********************************************************************/
.text
.align 4
.globl camellia_simd128_step3_asm
.type camellia_simd128_step3_asm, %function
camellia_simd128_step3_asm:
	/* Save registers and allocate stack:
	 * Layout:
	 *   sp+0:    x29, x30 (16 bytes)
	 *   sp+16:   (reserved for inpack16_pre temp space, 32 bytes)
	 *   sp+48:   x19-x26 (64 bytes) - callee-saved registers
	 *   sp+112:  AB buffer (128 bytes)
	 *   sp+240:  CD buffer (128 bytes)
	 * Total: 368 bytes (0+16+32+64+128+128)
	 */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0   /* CTX */
	mov x20, x1   /* ab_out */
	mov x21, x2   /* cd_out */
	mov x22, x3   /* input */

	/* Setup buffer pointers:
	 * x23 = mem_ab (sp + 112)
	 * x24 = mem_cd (sp + 240)
	 */
	add x23, sp, #112
	add x24, sp, #240

	/* ========== STAGE 1: inpack16 ========== */

	/* Pre-whitening with key_table[0] */
	ldr x10, [x19, #key_table]   /* x10 = key_table[0] */

	/* Call inpack16_pre: loads 16 blocks from x22 and XORs with key */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x10, v16, x26

	/* Byteslice and store to AB/CD buffers */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10

	/* ✅ CRITICAL: Reset AB/CD pointers after inpack16_post's post-increment */
	add x23, sp, #112     /* x23 = AB buffer base */
	add x24, sp, #240     /* x24 = CD buffer base */

	/* ========== STAGE 2: First 6 rounds (keys 2-7) ========== */

	/* Restore CTX to x0 (required by enc_rounds16 macro) */
	mov x0, x19

	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Execute 6 rounds using enc_rounds16 macro with i=0
	 * This uses keys 2-7 (i+2, i+4, i+6 for 3 pairs of rounds)
	 */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* CRITICAL: enc_rounds16's last pair doesn't write back AB (store_ab=0).
	 * But fls16 needs AB in memory for step 4 (reads lr from AB[4-7]).
	 * So we must write v0-v7 back to AB buffer before calling fls16. */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ========== STAGE 3: FL1 (keys 8, 9) ========== */

	/* AB is already in v0-v7 and written to memory, ready for fls16 */

	/* Setup FL key addresses:
	 * x25 = &key_table[8] (kl)
	 * x26 = &key_table[9] (kr)
	 * Use x0 (CTX) which was restored before enc_rounds16
	 * NOTE: Cannot use x11/x12 because fls16 macro uses w11 internally!
	 */
	add x10, x0, #key_table
	add x25, x10, #(8*8)
	add x26, x10, #(9*8)

	/* Call fls16: AB in v0-v7, CD at x24 */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store AB back to buffer */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ========== OUTPUT: Copy AB/CD to output buffers ========== */

	/* Copy AB buffer (128 bytes) to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer (128 bytes) to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Restore registers and return */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size camellia_simd128_step3_asm, .-camellia_simd128_step3_asm

/**********************************************************************
 * camellia_simd128_step4_asm - Step4: Step3 + second enc_rounds16
 *
 * Flow: inpack → enc_rounds16(2-7) → FL1(8,9) → enc_rounds16(10-15)
 *
 * C signature:
 *   void camellia_simd128_step4_asm(struct camellia_simd_ctx *ctx,
 *                                   uint8_t ab_out[128],
 *                                   uint8_t cd_out[128],
 *                                   const uint8_t in[256]);
 *
 * Parameters:
 *   x0: CTX pointer
 *   x1: ab_out pointer
 *   x2: cd_out pointer
 *   x3: input pointer (256 bytes = 16 blocks)
 **********************************************************************/
.text
.align 4
.globl camellia_simd128_step4_asm
.type camellia_simd128_step4_asm, %function
camellia_simd128_step4_asm:
	/* Save registers and allocate stack (same as Step3) */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0   /* CTX */
	mov x20, x1   /* ab_out */
	mov x21, x2   /* cd_out */
	mov x22, x3   /* input */

	/* Setup buffer pointers */
	add x23, sp, #112
	add x24, sp, #240

	/* ========== STAGE 1: inpack16 ========== */
	ldr x10, [x19, #key_table]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x10, v16, x26

	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10

	/* Reset pointers */
	add x23, sp, #112
	add x24, sp, #240

	/* ========== STAGE 2: First enc_rounds16 (keys 2-7) ========== */
	mov x0, x19

	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* Write AB back for FL1 */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ========== STAGE 3: FL1 (keys 8, 9) ========== */
	add x10, x0, #key_table
	add x25, x10, #(8*8)
	add x26, x10, #(9*8)

	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Write AB back */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ========== STAGE 4: Second enc_rounds16 (keys 10-15) ========== */

	/* Restore CTX to x0 (required by enc_rounds16 macro) */
	mov x0, x19

	/* Reload AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Execute second 6 rounds with i=8 (keys 10-15) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8

	/* Write AB back to memory */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ========== OUTPUT: Copy AB/CD to output buffers ========== */

	/* Copy AB buffer to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Restore registers and return */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size camellia_simd128_step4_asm, .-camellia_simd128_step4_asm


/* Test function: double enc_rounds16 */
.text
.align 4
.globl test_double_asm
.type test_double_asm, %function
test_double_asm:
	/* Parameters: x0=ctx, x1=ab_in, x2=cd_in, x3=ab_out, x4=cd_out */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	mov x19, x0   /* CTX */
	mov x20, x3   /* ab_out */
	mov x21, x4   /* cd_out */

	/* Setup AB/CD buffers on stack */
	add x23, sp, #112  /* AB buffer */
	add x24, sp, #240  /* CD buffer */

	/* Copy ab_in to AB buffer */
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Copy cd_in to CD buffer */
	ldp q0, q1, [x2, #0]
	ldp q2, q3, [x2, #32]
	ldp q4, q5, [x2, #64]
	ldp q6, q7, [x2, #96]
	stp q0, q1, [x24, #0]
	stp q2, q3, [x24, #32]
	stp q4, q5, [x24, #64]
	stp q6, q7, [x24, #96]

	/* First enc_rounds16 (keys 2-7) */
	mov x0, x19
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* Write back AB */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Second enc_rounds16 (keys 10-15) */
	mov x0, x19
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8

	/* Write back AB */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Copy AB to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size test_double_asm, .-test_double_asm

/*
 * enc_rounds16_i8_asm_wrapper - Test ONLY enc_rounds16(i=8, keys 10-15)
 *
 * Parameters:
 *   x0 = ctx
 *   x1 = ab_in (128 bytes)
 *   x2 = cd_in (128 bytes)
 *   x3 = ab_out (128 bytes)
 *   x4 = cd_out (128 bytes)
 *
 * This isolates the second enc_rounds16 call to debug Step4's AB mismatch.
 */
.text
.align 4
.globl enc_rounds16_i8_asm_wrapper
.type enc_rounds16_i8_asm_wrapper, %function
enc_rounds16_i8_asm_wrapper:
	/* Prologue */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0      /* CTX */
	mov x20, x3      /* ab_out */
	mov x21, x4      /* cd_out */

	/* Setup AB/CD buffers on stack */
	add x23, sp, #112  /* AB buffer: sp+112 */
	add x24, sp, #240  /* CD buffer: sp+240 */

	/* Copy ab_in to AB buffer */
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Copy cd_in to CD buffer */
	ldp q0, q1, [x2, #0]
	ldp q2, q3, [x2, #32]
	ldp q4, q5, [x2, #64]
	ldp q6, q7, [x2, #96]
	stp q0, q1, [x24, #0]
	stp q2, q3, [x24, #32]
	stp q4, q5, [x24, #64]
	stp q6, q7, [x24, #96]

	/* ===== Call enc_rounds16(i=8) ONLY ===== */
	mov x0, x19            /* Restore CTX */

	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Execute enc_rounds16 with i=8 (keys 10-15) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8

	/* Write final AB back to memory (last pair has store_ab=0) */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Copy AB buffer to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Epilogue */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size enc_rounds16_i8_asm_wrapper, .-enc_rounds16_i8_asm_wrapper

/*
 * dump_enc_rounds16_keys_i0_asm - Dump 6 keys used by enc_rounds16(i=0)
 *
 * Parameters:
 *   x0 = ctx
 *   x1 = out (uint64_t out[6])
 *
 * enc_rounds16(i=0) calls:
 *   two_roundsm16(..., i=2, dir=1)  → key[2], key[3]
 *   two_roundsm16(..., i=4, dir=1)  → key[4], key[5]
 *   two_roundsm16(..., i=6, dir=1)  → key[6], key[7]
 */
.text
.align 4
.globl dump_enc_rounds16_keys_i0_asm
.type dump_enc_rounds16_keys_i0_asm, %function
dump_enc_rounds16_keys_i0_asm:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	mov x19, x0      /* CTX */
	mov x20, x1      /* out */

	/* First pair: two_roundsm16(i=2) → key[2], key[3] */
	add x12, x19, #key_table
	add x12, x12, #(2 * 8)
	ldr x11, [x12]
	str x11, [x20, #0]

	add x12, x19, #key_table
	add x12, x12, #(3 * 8)
	ldr x11, [x12]
	str x11, [x20, #8]

	/* Second pair: two_roundsm16(i=4) → key[4], key[5] */
	add x12, x19, #key_table
	add x12, x12, #(4 * 8)
	ldr x11, [x12]
	str x11, [x20, #16]

	add x12, x19, #key_table
	add x12, x12, #(5 * 8)
	ldr x11, [x12]
	str x11, [x20, #24]

	/* Third pair: two_roundsm16(i=6) → key[6], key[7] */
	add x12, x19, #key_table
	add x12, x12, #(6 * 8)
	ldr x11, [x12]
	str x11, [x20, #32]

	add x12, x19, #key_table
	add x12, x12, #(7 * 8)
	ldr x11, [x12]
	str x11, [x20, #40]

	ldp x29, x30, [sp], #16
	ret
.size dump_enc_rounds16_keys_i0_asm, .-dump_enc_rounds16_keys_i0_asm

/*
 * dump_enc_rounds16_keys_i8_asm - Dump 6 keys used by enc_rounds16(i=8)
 *
 * Parameters:
 *   x0 = ctx
 *   x1 = out (uint64_t out[6])
 *
 * enc_rounds16(i=8) calls:
 *   two_roundsm16(..., i=10, dir=1) → key[10], key[11]
 *   two_roundsm16(..., i=12, dir=1) → key[12], key[13]
 *   two_roundsm16(..., i=14, dir=1) → key[14], key[15]
 */
.text
.align 4
.globl dump_enc_rounds16_keys_i8_asm
.type dump_enc_rounds16_keys_i8_asm, %function
dump_enc_rounds16_keys_i8_asm:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	mov x19, x0      /* CTX */
	mov x20, x1      /* out */

	/* First pair: two_roundsm16(i=10) → key[10], key[11] */
	add x12, x19, #key_table
	add x12, x12, #(10 * 8)
	ldr x11, [x12]
	str x11, [x20, #0]

	add x12, x19, #key_table
	add x12, x12, #(11 * 8)
	ldr x11, [x12]
	str x11, [x20, #8]

	/* Second pair: two_roundsm16(i=12) → key[12], key[13] */
	add x12, x19, #key_table
	add x12, x12, #(12 * 8)
	ldr x11, [x12]
	str x11, [x20, #16]

	add x12, x19, #key_table
	add x12, x12, #(13 * 8)
	ldr x11, [x12]
	str x11, [x20, #24]

	/* Third pair: two_roundsm16(i=14) → key[14], key[15] */
	add x12, x19, #key_table
	add x12, x12, #(14 * 8)
	ldr x11, [x12]
	str x11, [x20, #32]

	add x12, x19, #key_table
	add x12, x12, #(15 * 8)
	ldr x11, [x12]
	str x11, [x20, #40]

	ldp x29, x30, [sp], #16
	ret
.size dump_enc_rounds16_keys_i8_asm, .-dump_enc_rounds16_keys_i8_asm
/*
 * enc_rounds16_i8_2rounds_asm - Execute only FIRST pair (2 rounds) of enc_rounds16(i=8)
 */
.text
.align 4
.globl enc_rounds16_i8_2rounds_asm
.type enc_rounds16_i8_2rounds_asm, %function
enc_rounds16_i8_2rounds_asm:
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	mov x19, x0
	mov x20, x3
	mov x21, x4

	add x23, sp, #112
	add x24, sp, #240

	/* Copy inputs */
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	ldp q0, q1, [x2, #0]
	ldp q2, q3, [x2, #32]
	ldp q4, q5, [x2, #64]
	ldp q6, q7, [x2, #96]
	stp q0, q1, [x24, #0]
	stp q2, q3, [x24, #32]
	stp q4, q5, [x24, #64]
	stp q6, q7, [x24, #96]

	/* Execute ONLY first pair: two_roundsm16(i=10) */
	mov x0, x19
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	two_roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	              v16, v17, v18, v19, v20, v21, v22, v23, \
	              x23, x24, 10, 1, 1

	/* Copy outputs */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size enc_rounds16_i8_2rounds_asm, .-enc_rounds16_i8_2rounds_asm

/*
 * enc_rounds16_i8_4rounds_asm - Execute FIRST TWO pairs (4 rounds) of enc_rounds16(i=8)
 */
.text
.align 4
.globl enc_rounds16_i8_4rounds_asm
.type enc_rounds16_i8_4rounds_asm, %function
enc_rounds16_i8_4rounds_asm:
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	mov x19, x0
	mov x20, x3
	mov x21, x4

	add x23, sp, #112
	add x24, sp, #240

	/* Copy inputs */
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	ldp q0, q1, [x2, #0]
	ldp q2, q3, [x2, #32]
	ldp q4, q5, [x2, #64]
	ldp q6, q7, [x2, #96]
	stp q0, q1, [x24, #0]
	stp q2, q3, [x24, #32]
	stp q4, q5, [x24, #64]
	stp q6, q7, [x24, #96]

	/* Execute first two pairs */
	mov x0, x19
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	two_roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	              v16, v17, v18, v19, v20, v21, v22, v23, \
	              x23, x24, 10, 1, 1

	mov x0, x19
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	two_roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	              v16, v17, v18, v19, v20, v21, v22, v23, \
	              x23, x24, 12, 1, 1

	/* Copy outputs */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size enc_rounds16_i8_4rounds_asm, .-enc_rounds16_i8_4rounds_asm

/*
 * test_round1_with_cd_writeback_asm - Test Round1 and CD writeback
 * 
 * Parameters:
 *   x0: ctx (struct camellia_simd_ctx *)
 *   x1: ab_in (uint8_t[128])
 *   x2: cd_in (uint8_t[128])
 *   x3: cd_after_round1 (uint8_t[128]) - output
 *   x4: key (uint64_t)
 */
.text
.align 4
.globl test_round1_with_cd_writeback_asm
.type test_round1_with_cd_writeback_asm, %function

test_round1_with_cd_writeback_asm:
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0  /* CTX */
	mov x20, x1  /* ab_in */
	mov x21, x2  /* cd_in */
	mov x22, x3  /* cd_after_round1 (output) */
	mov x10, x4  /* key */

	/* Setup mem_cd and mem_ab pointers on stack */
	add x23, sp, #112  /* mem_ab (128 bytes) */
	add x24, sp, #240  /* mem_cd (128 bytes) */

	/* Load AB from ab_in to registers */
	ldp q0, q1, [x20, #0]
	ldp q2, q3, [x20, #32]
	ldp q4, q5, [x20, #64]
	ldp q6, q7, [x20, #96]

	/* Load CD from cd_in to mem_cd on stack */
	ldp q8, q9, [x21, #0]
	ldp q10, q11, [x21, #32]
	ldp q12, q13, [x21, #64]
	ldp q14, q15, [x21, #96]
	stp q8, q9, [x24, #0]
	stp q10, q11, [x24, #32]
	stp q12, q13, [x24, #64]
	stp q14, q15, [x24, #96]

	/* Prepare key address for roundsm16 */
	mov x11, x10
	stp x11, x11, [sp, #16]  /* Store key on stack */
	add x12, sp, #16  /* x12 = address of key */

	/* Call roundsm16: AB (v0-v7) → new_CD (v0-v7) */
	roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	          v16, v17, v18, v19, v20, v21, v22, v23, \
	          x24, x12

	/* NOW: v0-v7 contain new_CD in order v4,v5,v6,v7,v0,v1,v2,v3 = CD[0-7] */
	/* Write back to mem_cd using the same pattern as two_roundsm16 */
	add x11, x24, #64
	st1 {v0.16b}, [x11]   /* cd[4] = v0 */
	add x11, x24, #80
	st1 {v1.16b}, [x11]   /* cd[5] = v1 */
	add x11, x24, #96
	st1 {v2.16b}, [x11]   /* cd[6] = v2 */
	add x11, x24, #112
	st1 {v3.16b}, [x11]   /* cd[7] = v3 */
	st1 {v4.16b}, [x24]    /* cd[0] = v4 */
	add x11, x24, #16
	st1 {v5.16b}, [x11]   /* cd[1] = v5 */
	add x11, x24, #32
	st1 {v6.16b}, [x11]   /* cd[2] = v6 */
	add x11, x24, #48
	st1 {v7.16b}, [x11]   /* cd[3] = v7 */

	/* Copy mem_cd to output cd_after_round1 */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x22, #0]
	stp q2, q3, [x22, #32]
	stp q4, q5, [x22, #64]
	stp q6, q7, [x22, #96]

	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size test_round1_with_cd_writeback_asm, .-test_round1_with_cd_writeback_asm

/*
 * test_two_roundsm16_asm - Test complete two_roundsm16(i=10)
 * 
 * Parameters:
 *   x0: ctx (struct camellia_simd_ctx *)
 *   x1: ab_in (uint8_t[128])
 *   x2: cd_in (uint8_t[128])
 *   x3: ab_out (uint8_t[128]) - output
 *   x4: cd_out (uint8_t[128]) - output
 */
.text
.align 4
.globl test_two_roundsm16_asm
.type test_two_roundsm16_asm, %function

test_two_roundsm16_asm:
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0  /* CTX */
	mov x20, x1  /* ab_in */
	mov x21, x2  /* cd_in */
	mov x22, x3  /* ab_out (output) */
	mov x25, x4  /* cd_out (output) */

	/* Setup mem_cd and mem_ab pointers on stack */
	add x23, sp, #112  /* mem_ab (128 bytes) */
	add x24, sp, #240  /* mem_cd (128 bytes) */

	/* Load AB from ab_in to mem_ab */
	ldp q0, q1, [x20, #0]
	ldp q2, q3, [x20, #32]
	ldp q4, q5, [x20, #64]
	ldp q6, q7, [x20, #96]
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* Load CD from cd_in to mem_cd */
	ldp q0, q1, [x21, #0]
	ldp q2, q3, [x21, #32]
	ldp q4, q5, [x21, #64]
	ldp q6, q7, [x21, #96]
	stp q0, q1, [x24, #0]
	stp q2, q3, [x24, #32]
	stp q4, q5, [x24, #64]
	stp q6, q7, [x24, #96]

	/* Load AB to registers for two_roundsm16 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Call two_roundsm16(i=10, dir=1, store_ab=1) */
	mov x0, x19  /* Restore CTX */
	two_roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, \
	              v16, v17, v18, v19, v20, v21, v22, v23, \
	              x23, x24, 10, 1, 1

	/* Copy mem_ab to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x22, #0]
	stp q2, q3, [x22, #32]
	stp q4, q5, [x22, #64]
	stp q6, q7, [x22, #96]

	/* Copy mem_cd to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x25, #0]
	stp q2, q3, [x25, #32]
	stp q4, q5, [x25, #64]
	stp q6, q7, [x25, #96]

	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size test_two_roundsm16_asm, .-test_two_roundsm16_asm

/*
 * camellia_simd128_step5_asm - Step5: inpack16 + enc_rounds16(0) + FL1 + enc_rounds16(8) + FL2
 * 
 * Parameters:
 *   x0: ctx (struct camellia_simd_ctx *)
 *   x1: ab_out (uint8_t[128]) - output AB buffer
 *   x2: cd_out (uint8_t[128]) - output CD buffer
 *   x3: in (const uint8_t[256]) - input plaintext (16 blocks)
 */
.text
.align 4
.globl camellia_simd128_step5_asm
.type camellia_simd128_step5_asm, %function

camellia_simd128_step5_asm:
	/* Prologue - 368 byte stack frame */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	
	/* Save callee-saved registers */
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters */
	mov x19, x0  /* CTX */
	mov x20, x1  /* ab_out */
	mov x21, x2  /* cd_out */
	mov x22, x3  /* in */

	/* Setup AB/CD buffers on stack */
	add x23, sp, #112  /* AB buffer: sp+112 (128 bytes) */
	add x24, sp, #240  /* CD buffer: sp+240 (128 bytes) */

	/* ===== STAGE 1: inpack16 ===== */
	
	/* Load key_table[0] for pre-whitening */
	add x10, x19, #key_table
	ldr x11, [x10]  /* key_table[0] */

	/* inpack16_pre: pre-whitening + transpose */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x11, v16, x26

	/* inpack16_post: byteslice into AB/CD */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10

	/* Reset pointers (inpack16_post modifies them with post-increment) */
	add x23, sp, #112  /* AB buffer: sp+112 */
	add x24, sp, #240  /* CD buffer: sp+240 */

	/* ===== STAGE 2: First enc_rounds16 (keys 2-7) ===== */

	mov x0, x19  /* Restore CTX */
	
	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Execute enc_rounds16 with i=0 (keys 2-7) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* Write final AB back to memory (last pair has store_ab=0) */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 3: FL1 (keys 8, 9) ===== */
	
	/* Prepare key addresses */
	add x10, x19, #key_table
	add x25, x10, #(8 * 8)   /* &key_table[8] */
	add x26, x10, #(9 * 8)   /* &key_table[9] */

	/* Execute FL layer */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* fls16 updated AB only in v0-v7, write it back to mem_ab */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 4: Second enc_rounds16 (keys 10-15) ===== */

	mov x0, x19  /* Restore CTX */

	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	/* Execute enc_rounds16 with i=8 (keys 10-15) */
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8

	/* Write final AB back to memory (last pair has store_ab=0) */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 5: FL2 (keys 16, 17) ===== */
	
	/* Prepare key addresses */
	add x10, x19, #key_table
	add x25, x10, #(16 * 8)   /* &key_table[16] */
	add x26, x10, #(17 * 8)   /* &key_table[17] */

	/* Execute FL layer */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store final AB (after FL2) back to mem_ab for output */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== OUTPUT: Copy AB/CD to output buffers ===== */
	
	/* Copy AB buffer to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Epilogue */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size camellia_simd128_step5_asm, .-camellia_simd128_step5_asm

/*
 * Step6: camellia_simd128_step6_asm
 * Input: ctx, ab_out[128], cd_out[128], in[256]
 * Flow: inpack16 → enc_rounds16(0) → FL1 → enc_rounds16(8) → FL2 → enc_rounds16(16)
 * Output: AB/CD after third enc_rounds16
 */
.globl camellia_simd128_step6_asm
.type camellia_simd128_step6_asm, %function
camellia_simd128_step6_asm:
	/* Prologue: 368 bytes
	 * sp+0:   x29, x30
	 * sp+48:  x19-x26 (callee-saved)
	 * sp+112: AB buffer (128 bytes) - mem_ab
	 * sp+240: CD buffer (128 bytes) - mem_cd
	 */
	stp x29, x30, [sp, #-368]!
	mov x29, sp

	/* Save callee-saved registers */
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	/* Save parameters:
	 * x19 = ctx
	 * x20 = ab_out
	 * x21 = cd_out
	 * x22 = in
	 */
	mov x19, x0
	mov x20, x1
	mov x21, x2
	mov x22, x3

	/* Setup buffer pointers */
	add x23, sp, #112   /* mem_ab: sp+112 */
	add x24, sp, #240   /* mem_cd: sp+240 */

	/* ===== STAGE 1: inpack16 (pre-whitening + byteslice) ===== */

	/* Load key_table[0] for pre-whitening (⚠️ must load VALUE, not address) */
	add x10, x19, #key_table
	ldr x11, [x10]              /* x11 = key_table[0] */

	/* inpack16_pre: pre-whitening */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x11, v16, x26

	/* inpack16_post: finalize byteslice and store to mem_ab/mem_cd */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10

	/* Reset pointers (inpack16_post modifies them with post-increment) */
	add x23, sp, #112   /* AB buffer: sp+112 */
	add x24, sp, #240   /* CD buffer: sp+240 */

	/* ===== STAGE 2: First enc_rounds16 (keys 2-7, k=0) ===== */

	mov x0, x19  /* Restore CTX to x0 (CTX macro expects it there) */

	/* Load AB into v0-v7 (inpack16_post wrote it to memory) */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* enc_rounds16's last pair uses dummy_store - write back AB */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 3: FL1 (keys 8, 9) ===== */

	/* Prepare key addresses */
	add x10, x19, #key_table
	add x25, x10, #(8 * 8)   /* &key_table[8] */
	add x26, x10, #(9 * 8)   /* &key_table[9] */

	/* Execute FL layer */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* fls16 updated AB only in v0-v7, write it back to mem_ab */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 4: Second enc_rounds16 (keys 10-15, k=8) ===== */

	mov x0, x19  /* Restore CTX */

	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8

	/* enc_rounds16's last pair uses dummy_store - write back AB */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 5: FL2 (keys 16, 17) ===== */

	/* Prepare key addresses */
	add x10, x19, #key_table
	add x25, x10, #(16 * 8)   /* &key_table[16] */
	add x26, x10, #(17 * 8)   /* &key_table[17] */

	/* Execute FL layer */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23

	/* Store final AB (after FL2) back to mem_ab */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== STAGE 6: Third enc_rounds16 (keys 18-23, k=16) ===== */

	mov x0, x19  /* Restore CTX */

	/* Load AB into v0-v7 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]

	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16

	/* enc_rounds16's last pair uses dummy_store - write back AB */
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* ===== OUTPUT: Copy AB/CD to output buffers ===== */

	/* Copy AB buffer to ab_out */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD buffer to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Epilogue */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size camellia_simd128_step6_asm, .-camellia_simd128_step6_asm

/*
 * Step7: camellia_simd128_step7_asm - Complete encryption
 * Input: ctx, out[256], in[256]
 * Flow: Full Camellia encryption with all rounds based on key length
 */
.globl camellia_simd128_step7_asm
.type camellia_simd128_step7_asm, %function
camellia_simd128_step7_asm:
	/* Prologue: 496 bytes (ABI-compliant)
	 * sp+0:   x29, x30 (16 bytes)
	 * sp+16:  q8-q15 (callee-saved vector registers, 128 bytes)
	 * sp+144: x19-x26 (callee-saved GPRs, 64 bytes)
	 * sp+208: [padding 32 bytes]
	 * sp+240: AB buffer (128 bytes) - mem_ab
	 * sp+368: CD buffer (128 bytes) - mem_cd
	 */
	stp x29, x30, [sp, #-496]!
	mov x29, sp

	/* Save callee-saved vector registers (ABI requirement) */
	stp q8,  q9,  [sp, #16]
	stp q10, q11, [sp, #48]
	stp q12, q13, [sp, #80]
	stp q14, q15, [sp, #112]

	/* Save callee-saved GPRs */
	stp x19, x20, [sp, #144]
	stp x21, x22, [sp, #160]
	stp x23, x24, [sp, #176]
	stp x25, x26, [sp, #192]

	/* Save parameters:
	 * x19 = ctx
	 * x20 = out
	 * x21 = in
	 */
	mov x19, x0
	mov x20, x1
	mov x21, x2

	/* Setup buffer pointers (shifted by 128 bytes for q8-q15 save area) */
	add x23, sp, #240   /* mem_ab: sp+240 (was sp+112 before q8-q15) */
	add x24, sp, #368   /* mem_cd: sp+368 (was sp+240 before q8-q15) */

	/* OPTIMIZATION: Localized constant loading strategy
	 * Constants are now loaded within roundsm16_opt macro for short lifetime
	 * This avoids long-term occupation of v24-v29 which hurts OoO scheduling
	 * Better register renaming pool = better ILP (Instruction Level Parallelism) */

	/* Determine lastk based on key_length */
	ldr w22, [x19, #key_length]  /* w22 = ctx->key_length */
	cmp w22, #16
	bgt .L256bit_path           /* key_length > 16: use 192/256-bit path */

	/* ===== 128-bit KEY PATH (lastk = 24) ===== */
.L128bit_path:
	/* ===== STAGE 1: inpack16 (pre-whitening + byteslice) ===== */
	/* Load key_table[0] for pre-whitening (⚠️ must load VALUE, not address) */
	add x10, x19, #key_table
	ldr x11, [x10]              /* x11 = key_table[0] */

	/* inpack16_pre: pre-whitening */
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x21, x11, v16, x26

	/* inpack16_post: byteslice into AB/CD */
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10

	/* Reset pointers (inpack16_post modifies them with post-increment) */
	add x23, sp, #240
	add x24, sp, #368
	mov x0, x19

	/* Round 1: enc_rounds16(0) + FL1 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* FL1 (keys 8, 9) */
	add x10, x19, #key_table
	add x25, x10, #(8 * 8)
	add x26, x10, #(9 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 2: enc_rounds16(8) + FL2 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	/* FL2 (keys 16, 17) */
	add x10, x19, #key_table
	add x25, x10, #(16 * 8)
	add x26, x10, #(17 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 3: enc_rounds16(16) - LAST, no FL */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16
	/* ⚠️ DO NOT write back AB - keep it in v0-v7 for outunpack16 */

	/* Prepare for outunpack16: lastk = 24 */
	mov w25, #24
	b .Loutunpack

	/* ===== 192/256-bit KEY PATH (lastk = 32) ===== */
.L256bit_path:
	/* inpack16 */
	add x10, x19, #key_table
	ldr x11, [x10]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x21, x11, v16, x26
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10
	add x23, sp, #240
	add x24, sp, #368
	mov x0, x19

	/* Round 1: enc_rounds16(0) + FL1 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(8 * 8)
	add x26, x10, #(9 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 2: enc_rounds16(8) + FL2 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(16 * 8)
	add x26, x10, #(17 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 3: enc_rounds16(16) + FL3 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(24 * 8)
	add x26, x10, #(25 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 4: enc_rounds16(24) - LAST, no FL */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 24
	/* ⚠️ DO NOT write back AB - keep it in v0-v7 for outunpack16 */

	/* Prepare for outunpack16: lastk = 32 */
	mov w25, #32

	/* ===== OUTPUT PHASE (common for both paths) ===== */
.Loutunpack:
	/* AB is already in v0-v7 from last enc_rounds16 (dummy_store) */
	/* Load CD to v8-v15 for outunpack16 */
	ldp q8, q9, [x24, #0]
	ldp q10, q11, [x24, #32]
	ldp q12, q13, [x24, #64]
	ldp q14, q15, [x24, #96]

	/* Get post-whitening key: key_table[lastk] */
	add x10, x19, #key_table
	add x11, x10, x25, lsl #3
	ldr x11, [x11]

	/* outunpack16: post-whitening + reverse byteslice */
	outunpack16 v0, v1, v2, v3, v4, v5, v6, v7, \
	            v8, v9, v10, v11, v12, v13, v14, v15, \
	            x11, v16, v17, x10

	/* write_output: write 16 blocks in REVERSED order (match C reference) */
	mov x11, x20
	write_output v7, v6, v5, v4, v3, v2, v1, v0, \
	             v15, v14, v13, v12, v11, v10, v9, v8, x11

	/* Epilogue (ABI-compliant) */
	ldp x25, x26, [sp, #192]
	ldp x23, x24, [sp, #176]
	ldp x21, x22, [sp, #160]
	ldp x19, x20, [sp, #144]

	/* Restore callee-saved vector registers */
	ldp q14, q15, [sp, #112]
	ldp q12, q13, [sp, #80]
	ldp q10, q11, [sp, #48]
	ldp q8,  q9,  [sp, #16]

	ldp x29, x30, [sp], #496
	ret
.size camellia_simd128_step7_asm, .-camellia_simd128_step7_asm

/*
 * Debug wrapper: Output AB/CD just before outunpack16 (128-bit key only)
 */
.globl camellia_simd128_step7_before_outunpack
.type camellia_simd128_step7_before_outunpack, %function
camellia_simd128_step7_before_outunpack:
	/* Same prologue as Step7 */
	stp x29, x30, [sp, #-368]!
	mov x29, sp
	stp x19, x20, [sp, #48]
	stp x21, x22, [sp, #64]
	stp x23, x24, [sp, #80]
	stp x25, x26, [sp, #96]

	mov x19, x0  /* ctx */
	mov x20, x1  /* ab_out */
	mov x21, x2  /* cd_out */
	mov x22, x3  /* in */

	add x23, sp, #112
	add x24, sp, #240

	/* Execute 128-bit path (same as Step7) */
	add x10, x19, #key_table
	ldr x11, [x10]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x22, x11, v16, x26
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10
	add x23, sp, #112
	add x24, sp, #240
	mov x0, x19

	/* Round 1: enc_rounds16(0) + FL1 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(8 * 8)
	add x26, x10, #(9 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 2: enc_rounds16(8) + FL2 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(16 * 8)
	add x26, x10, #(17 * 8)
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 3: enc_rounds16(16) - LAST */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	enc_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16

	/* NOW: AB is in v0-v7, CD is in mem_cd (x24) */
	/* Output AB and CD instead of continuing to outunpack16 */

	/* Copy AB to ab_out */
	stp q0, q1, [x20, #0]
	stp q2, q3, [x20, #32]
	stp q4, q5, [x20, #64]
	stp q6, q7, [x20, #96]

	/* Copy CD to cd_out */
	ldp q0, q1, [x24, #0]
	ldp q2, q3, [x24, #32]
	ldp q4, q5, [x24, #64]
	ldp q6, q7, [x24, #96]
	stp q0, q1, [x21, #0]
	stp q2, q3, [x21, #32]
	stp q4, q5, [x21, #64]
	stp q6, q7, [x21, #96]

	/* Epilogue */
	ldp x25, x26, [sp, #96]
	ldp x23, x24, [sp, #80]
	ldp x21, x22, [sp, #64]
	ldp x19, x20, [sp, #48]
	ldp x29, x30, [sp], #368
	ret
.size camellia_simd128_step7_before_outunpack, .-camellia_simd128_step7_before_outunpack
/*
 * test_outunpack_after - Debug wrapper to test outunpack16
 * Parameters: x0=regs_out, x1=ab_in, x2=cd_in, x3=key
 */
.globl test_outunpack_after
.type test_outunpack_after, %function
test_outunpack_after:
	stp x29, x30, [sp, #-48]!
	mov x29, sp
	stp x19, x20, [sp, #16]

	mov x19, x0  /* regs_out */

	/* Load AB to v0-v7 */
	ldp q0, q1, [x1, #0]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x1, #64]
	ldp q6, q7, [x1, #96]

	/* Load CD to v8-v15 */
	ldp q8, q9, [x2, #0]
	ldp q10, q11, [x2, #32]
	ldp q12, q13, [x2, #64]
	ldp q14, q15, [x2, #96]

	/* Call outunpack16 */
	mov x11, x3  /* key */
	outunpack16 v0, v1, v2, v3, v4, v5, v6, v7, \
	            v8, v9, v10, v11, v12, v13, v14, v15, \
	            x11, v16, v17, x10

	/* Output registers v0-v15 in sequential order */
	stp q0, q1, [x19, #0]
	stp q2, q3, [x19, #32]
	stp q4, q5, [x19, #64]
	stp q6, q7, [x19, #96]
	stp q8, q9, [x19, #128]
	stp q10, q11, [x19, #160]
	stp q12, q13, [x19, #192]
	stp q14, q15, [x19, #224]

	ldp x19, x20, [sp, #16]
	ldp x29, x30, [sp], #48
	ret
.size test_outunpack_after, .-test_outunpack_after


/*
 * camellia_decrypt_16blks_simd128 - Main 16-block decryption entry point
 *
 * Implements full Camellia decryption for all key lengths.
 * Key differences from encryption:
 * - Pre-whitening uses key[lastk] instead of key[0]
 * - FL uses SWAPPED key pointers: key[k+1], key[k] instead of key[k], key[k+1]
 * - dec_rounds16 executes rounds in reverse: i=16,8,0 (or i=24,16,8,0)
 * - Post-whitening uses key[0] instead of key[lastk]
 *
 * Parameters:
 *   x0 = ctx (struct camellia_simd_ctx *)
 *   x1 = out (256 bytes)
 *   x2 = in (256 bytes)
 */
.globl camellia_decrypt_16blks_simd128
.type camellia_decrypt_16blks_simd128, %function
.align 4
camellia_decrypt_16blks_simd128:
	/* Prologue: 496 bytes (ABI-compliant) */
	stp x29, x30, [sp, #-496]!
	mov x29, sp

	/* Save callee-saved vector registers (ABI requirement) */
	stp q8,  q9,  [sp, #16]
	stp q10, q11, [sp, #48]
	stp q12, q13, [sp, #80]
	stp q14, q15, [sp, #112]

	/* Save callee-saved GPRs */
	stp x19, x20, [sp, #144]
	stp x21, x22, [sp, #160]
	stp x23, x24, [sp, #176]
	stp x25, x26, [sp, #192]

	mov x19, x0
	mov x20, x1
	mov x21, x2

	add x23, sp, #240
	add x24, sp, #368

	/* Determine lastk */
	ldr w22, [x19, #key_length]
	cmp w22, #16
	bgt .Ldec_256bit_path

	/* ===== 128-bit KEY PATH ===== */
.Ldec_128bit_path:
	/* inpack16 with key[24] */
	add x10, x19, #key_table
	add x10, x10, #(24 * 8)
	ldr x11, [x10]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x21, x11, v16, x26
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10
	add x23, sp, #240
	add x24, sp, #368
	mov x0, x19

	/* Round 1: dec_rounds16(i=16) + FL with SWAPPED keys */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(17 * 8)    /* SWAPPED: key[k+1] first */
	add x26, x10, #(16 * 8)    /* SWAPPED: key[k] second */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 2: dec_rounds16(i=8) + FL with SWAPPED keys */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(9 * 8)     /* SWAPPED */
	add x26, x10, #(8 * 8)     /* SWAPPED */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 3: dec_rounds16(i=0) - LAST */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	b .Ldec_outunpack

	/* ===== 192/256-bit KEY PATH ===== */
.Ldec_256bit_path:
	add x10, x19, #key_table
	add x10, x10, #(32 * 8)
	ldr x11, [x10]
	inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
	             v8, v9, v10, v11, v12, v13, v14, v15, \
	             x21, x11, v16, x26
	inpack16_post v0, v1, v2, v3, v4, v5, v6, v7, \
	              v8, v9, v10, v11, v12, v13, v14, v15, \
	              x23, x24, v16, v17, x10
	add x23, sp, #240
	add x24, sp, #368
	mov x0, x19

	/* Round 1 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 24
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(25 * 8)    /* SWAPPED */
	add x26, x10, #(24 * 8)    /* SWAPPED */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 2 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 16
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(17 * 8)    /* SWAPPED */
	add x26, x10, #(16 * 8)    /* SWAPPED */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 3 */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 8
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]

	add x10, x19, #key_table
	add x25, x10, #(9 * 8)     /* SWAPPED */
	add x26, x10, #(8 * 8)     /* SWAPPED */
	fls16 v0, v1, v2, v3, v4, v5, v6, v7, \
	      x24, x25, x26, \
	      v16, v17, v18, v19, v20, v21, v22, v23
	stp q0, q1, [x23, #0]
	stp q2, q3, [x23, #32]
	stp q4, q5, [x23, #64]
	stp q6, q7, [x23, #96]
	mov x0, x19

	/* Round 4: LAST */
	ldp q0, q1, [x23, #0]
	ldp q2, q3, [x23, #32]
	ldp q4, q5, [x23, #64]
	ldp q6, q7, [x23, #96]
	dec_rounds16 v0, v1, v2, v3, v4, v5, v6, v7, \
	             v16, v17, v18, v19, v20, v21, v22, v23, \
	             x23, x24, 0

	/* ===== OUTPUT PHASE ===== */
.Ldec_outunpack:
	ldp q8, q9, [x24, #0]
	ldp q10, q11, [x24, #32]
	ldp q12, q13, [x24, #64]
	ldp q14, q15, [x24, #96]

	/* Post-whitening with key[0] */
	add x10, x19, #key_table
	ldr x11, [x10]

	outunpack16 v0, v1, v2, v3, v4, v5, v6, v7, \
	            v8, v9, v10, v11, v12, v13, v14, v15, \
	            x11, v16, v17, x10

	mov x11, x20
	write_output v7, v6, v5, v4, v3, v2, v1, v0, \
	             v15, v14, v13, v12, v11, v10, v9, v8, x11

	/* Epilogue (ABI-compliant) */
	ldp x25, x26, [sp, #192]
	ldp x23, x24, [sp, #176]
	ldp x21, x22, [sp, #160]
	ldp x19, x20, [sp, #144]

	/* Restore callee-saved vector registers */
	ldp q14, q15, [sp, #112]
	ldp q12, q13, [sp, #80]
	ldp q10, q11, [sp, #48]
	ldp q8,  q9,  [sp, #16]

	ldp x29, x30, [sp], #496
	ret
.size camellia_decrypt_16blks_simd128, .-camellia_decrypt_16blks_simd128

