/*
 bits_array.h
 project: bit array C library
 url: https://github.com/noporpoise/BitArray/
 maintainer: Isaac Turner <turner.isaac@gmail.com>
 license: Public Domain, no warranty
 date: Sep 2014
*/

#ifndef BIT_ARRAY_HEADER_SEEN
#define BIT_ARRAY_HEADER_SEEN

#include <inttypes.h>
#include <stdio.h>

#include "bit_macros.h"

typedef struct bits_array bits_array_t;

// 64 bit words
typedef uint64_t word_t, word_addr_t, bit_index_t;
typedef uint8_t word_offset_t; // Offset within a 64 bit word

#define BIT_INDEX_MIN 0
#define BIT_INDEX_MAX (~(bit_index_t)0)

#ifdef __cplusplus
extern "C" {
#endif

//
// Structs
//

struct bits_array {
	word_t *words;
	bit_index_t num_of_bits;
	// Number of words used -- this is just round_up(num_of_bits / 64)
	// if num_of_bits == 0, this is 0
	word_addr_t num_of_words;
	// For more efficient allocation we use realloc only to double size --
	// not for adding every word.  Initial size is INIT_CAPACITY_WORDS.
	word_addr_t capacity_in_words;
};

//
// Basics: Constructor, destructor, get length, resize
//

// Constructor - create a new bit array of length nbits
bits_array_t *bit_array_create(bit_index_t nbits);

// Destructor - free the memory used for a bit array
void bit_array_free(bits_array_t *bitarray);

// Allocate using existing struct
bits_array_t *bit_array_alloc(bits_array_t *bitarr, bit_index_t nbits);
void bit_array_dealloc(bits_array_t *bitarr);

// Get length of bit array
bit_index_t bit_array_length(const bits_array_t *bit_arr);

// Change the size of a bit array. Enlarging an array will add zeros
// to the end of it. Returns 1 on success, 0 on failure (e.g. not enough memory)
char bit_array_resize(bits_array_t *bitarr, bit_index_t new_num_of_bits);

// If bitarr length < num_bits, resizes to num_bits
char bit_array_ensure_size(
    bits_array_t *bitarr, bit_index_t ensure_num_of_bits);

// Same as above but exit with an error message if out of memory
void bit_array_resize_critical(bits_array_t *bitarr, bit_index_t num_of_bits);
void bit_array_ensure_size_critical(
    bits_array_t *bitarr, bit_index_t num_of_bits);

//
// Macros
//

//
// Get, set, clear, assign and toggle individual bits
// Macros for fast access -- beware: no bounds checking
//

#define bit_array_get(arr, i) bitset_get((arr)->words, i)
#define bit_array_set(arr, i) bitset_set((arr)->words, i)
#define bit_array_clear(arr, i) bitset_del((arr)->words, i)
#define bit_array_toggle(arr, i) bitset_tgl((arr)->words, i)
// c must be 0 or 1
#define bit_array_assign(arr, i, c) bitset_cpy((arr)->words, i, c)

#define bit_array_len(arr) ((arr)->num_of_bits)

//
// Get, set, clear, assign and toggle individual bits
// "Safe": use assert() to check bounds
//

// Get the value of a bit (returns 0 or 1)
char bit_array_get_bit(const bits_array_t *bitarr, bit_index_t b);
void bit_array_set_bit(bits_array_t *bitarr, bit_index_t b);
void bit_array_clear_bit(bits_array_t *bitarr, bit_index_t b);
void bit_array_toggle_bit(bits_array_t *bitarr, bit_index_t b);
// If char c != 0, set bit; otherwise clear bit
void bit_array_assign_bit(bits_array_t *bitarr, bit_index_t b, char c);

//
// "Resizing": enlarge array if needed
//

char bit_array_rget(bits_array_t *bitarr, bit_index_t b);
void bit_array_rset(bits_array_t *bitarr, bit_index_t b);
void bit_array_rclear(bits_array_t *bitarr, bit_index_t b);
void bit_array_rtoggle(bits_array_t *bitarr, bit_index_t b);
void bit_array_rassign(bits_array_t *bitarr, bit_index_t b, char c);

//
// Get, set, clear and toggle several bits at once
//

// Get the offsets of the set bits (for offsets start<=offset<end)
// Returns the number of bits set
// It is assumed that dst is at least of length (end-start)
bit_index_t bit_array_get_bits(const bits_array_t *bitarr, bit_index_t start,
    bit_index_t end, bit_index_t *dst);

// Set multiple bits at once.
// e.g. set bits 1, 20 & 31: bit_array_set_bits(bitarr, 3, 1,20,31);
// Note: variable args are of type unsigned int
void bit_array_set_bits(bits_array_t *bitarr, size_t n, ...);

// Clear multiple bits at once.
// e.g. clear bits 1, 20 & 31: bit_array_clear_bits(bitarr, 3, 1,20,31);
// Note: variable args are of type unsigned int
void bit_array_clear_bits(bits_array_t *bitarr, size_t n, ...);

// Toggle multiple bits at once
// e.g. toggle bits 1, 20 & 31: bit_array_toggle_bits(bitarr, 3, 1,20,31);
// Note: variable args are of type unsigned int
void bit_array_toggle_bits(bits_array_t *bitarr, size_t n, ...);

//
// Set, clear and toggle all bits in a region
//

// Set all the bits in a region
void bit_array_set_region(
    bits_array_t *bitarr, bit_index_t start, bit_index_t len);

// Clear all the bits in a region
void bit_array_clear_region(
    bits_array_t *bitarr, bit_index_t start, bit_index_t len);

// Toggle all the bits in a region
void bit_array_toggle_region(
    bits_array_t *bitarr, bit_index_t start, bit_index_t len);

//
// Set, clear and toggle all bits at once
//

// Set all bits in this array to 1
void bit_array_set_all(bits_array_t *bitarr);

// Set all bits in this array to 0
void bit_array_clear_all(bits_array_t *bitarr);

// Set all 1 bits to 0, and all 0 bits to 1
void bit_array_toggle_all(bits_array_t *bitarr);

//
// Get / set a word of a given size
//

// First bit is in the least significant bit position
// start index must be within the range of the bit array (0 <= x < length)
uint64_t bit_array_get_word64(const bits_array_t *bitarr, bit_index_t start);
uint32_t bit_array_get_word32(const bits_array_t *bitarr, bit_index_t start);
uint16_t bit_array_get_word16(const bits_array_t *bitarr, bit_index_t start);
uint8_t bit_array_get_word8(const bits_array_t *bitarr, bit_index_t start);
uint64_t bit_array_get_wordn(
    const bits_array_t *bitarr, bit_index_t start, int n);

// Set 64 bits at once from a particular start position
// Doesn't extend bit array. However it is safe to TRY to set bits beyond the
// end of the array, as long as: `start` is < `bit_array_length(arr)`
void bit_array_set_word64(
    bits_array_t *bitarr, bit_index_t start, uint64_t word);
void bit_array_set_word32(
    bits_array_t *bitarr, bit_index_t start, uint32_t word);
void bit_array_set_word16(
    bits_array_t *bitarr, bit_index_t start, uint16_t word);
void bit_array_set_word8(bits_array_t *bitarr, bit_index_t start, uint8_t byte);
void bit_array_set_wordn(
    bits_array_t *bitarr, bit_index_t start, uint64_t word, int n);

//
// Number of bits set
//

// Get the number of bits set (hamming weight)
bit_index_t bit_array_num_bits_set(const bits_array_t *bitarr);

// Get the number of bits not set (length - hamming weight)
bit_index_t bit_array_num_bits_cleared(const bits_array_t *bitarr);

// Get the number of bits set in on array and not the other.  This is equivalent
// to hamming weight of the XOR when the two arrays are the same length.
// e.g. 10101 vs 00111 => hamming distance 2 (XOR is 10010)
bit_index_t bit_array_hamming_distance(
    const bits_array_t *arr1, const bits_array_t *arr2);

// Parity - returns 1 if odd number of bits set, 0 if even
char bit_array_parity(const bits_array_t *bitarr);

//
// Find indices of set/clear bits
//

// Find the index of the next bit that is set, at or after `offset`
// Returns 1 if a bit is set, otherwise 0
// Index of next set bit is stored in the integer pointed to by result
// If no next bit is set result is not changed
char bit_array_find_next_set_bit(
    const bits_array_t *bitarr, bit_index_t offset, bit_index_t *result);

// Find the index of the next bit that is NOT set, at or after `offset`
// Returns 1 if a bit is NOT set, otherwise 0
// Index of next zero bit is stored in the integer pointed to by `result`
// If no next bit is zero, value at `result` is not changed
char bit_array_find_next_clear_bit(
    const bits_array_t *bitarr, bit_index_t offset, bit_index_t *result);

// Find the index of the previous bit that is set, before offset.
// Returns 1 if a bit is set, otherwise 0
// Index of previous set bit is stored in the integer pointed to by `result`
// If no previous bit is set result is not changed
char bit_array_find_prev_set_bit(
    const bits_array_t *bitarr, bit_index_t offset, bit_index_t *result);

// Find the index of the previous bit that is NOT set, before offset.
// Returns 1 if a bit is clear, otherwise 0
// Index of previous zero bit is stored in the integer pointed to by `result`
// If no previous bit is zero result is not changed
char bit_array_find_prev_clear_bit(
    const bits_array_t *bitarr, bit_index_t offset, bit_index_t *result);

// Find the index of the first bit that is set.
// Returns 1 if a bit is set, otherwise 0
// Index of first set bit is stored in the integer pointed to by `result`
// If no bit is set result is not changed
char bit_array_find_first_set_bit(
    const bits_array_t *bitarr, bit_index_t *result);

// Find the index of the first bit that is NOT set.
// Returns 1 if a bit is clear, otherwise 0
// Index of first zero bit is stored in the integer pointed to by `result`
// If no bit is zero result is not changed
char bit_array_find_first_clear_bit(
    const bits_array_t *bitarr, bit_index_t *result);

// Find the index of the last bit that is set.
// Returns 1 if a bit is set, otherwise 0
// Index of last set bit is stored in the integer pointed to by `result`
// If no bit is set result is not changed
char bit_array_find_last_set_bit(
    const bits_array_t *bitarr, bit_index_t *result);

// Find the index of the last bit that is NOT set.
// Returns 1 if a bit is clear, otherwise 0
// Index of last zero bit is stored in the integer pointed to by `result`
// If no bit is zero result is not changed
char bit_array_find_last_clear_bit(
    const bits_array_t *bitarr, bit_index_t *result);

//
// Sorting
//

// Put all the 0s before all the 1s
void bit_array_sort_bits(bits_array_t *bitarr);

// Put all the 1s before all the 0s
void bit_array_sort_bits_rev(bits_array_t *bitarr);

//
// String and printing methods
//

// Construct a bits_array_t from a string.
void bit_array_from_str(bits_array_t *bitarr, const char *bitstr);

// Construct a bits_array_t from a substring with given on and off characters.
void bit_array_from_substr(bits_array_t *bitarr, bit_index_t offset,
    const char *str, size_t len, const char *on, const char *off,
    char left_to_right);

// Takes a char array to write to.  `str` must be bitarr->num_of_bits+1 in
// length. Terminates string with '\0'
char *bit_array_to_str(const bits_array_t *bitarr, char *str);
char *bit_array_to_str_rev(const bits_array_t *bitarr, char *str);

// Get a string representations for a given region, using given on/off
// characters.
// Note: does not null-terminate
void bit_array_to_substr(const bits_array_t *bitarr, bit_index_t start,
    bit_index_t length, char *str, char on, char off, char left_to_right);

// Print this array to a file stream.  Prints '0's and '1'.  Doesn't print
// newline.
void bit_array_print(const bits_array_t *bitarr, FILE *fout);

// Print a string representations for a given region, using given on/off
// characters. Reverse prints from highest to lowest -- this is useful for
// printing binary numbers
void bit_array_print_substr(const bits_array_t *bitarr, bit_index_t start,
    bit_index_t length, FILE *fout, char on, char off, char left_to_right);

//
// Decimal
//

// Get bit array as decimal str (e.g. 0b1101 -> "13")
size_t bit_array_to_decimal(const bits_array_t *bitarr, char *str, size_t len);

// Return number of characters used
size_t bit_array_from_decimal(bits_array_t *bitarr, const char *decimal);

//
// Hexidecimal
//

// Loads array from hex string
// Returns the number of bits loaded (will be chars rounded up to multiple of 8)
// (0 on failure)
bit_index_t bit_array_from_hex(
    bits_array_t *bitarr, bit_index_t offset, const char *str, size_t len);

// Returns number of characters written
size_t bit_array_to_hex(const bits_array_t *bitarr, bit_index_t start,
    bit_index_t length, char *str, char uppercase);

// Print bit array as hex
size_t bit_array_print_hex(const bits_array_t *bitarr, bit_index_t start,
    bit_index_t length, FILE *fout, char uppercase);

//
// Clone and copy
//

// Copy a bits_array_t struct and the data it holds - returns pointer to new
// object
#define bit_array_dup bit_array_clone
bits_array_t *bit_array_clone(const bits_array_t *bitarr);

// Copy bits from one array to another
// Note: use MACRO bit_array_copy
// Destination and source can be the same bits_array_t and
// src/dst regions can overlap
void bit_array_copy(bits_array_t *dst, bit_index_t dstindx,
    const bits_array_t *src, bit_index_t srcindx, bit_index_t length);

// copy all of src to dst. dst is resized to match src.
void bit_array_copy_all(bits_array_t *dst, const bits_array_t *src);

//
// Logic operators
//

// BIT_ARRAYs can all be different or the same object
// dest array will be resized if it is too short
//
void bit_array_and(
    bits_array_t *dest, const bits_array_t *src1, const bits_array_t *src2);
void bit_array_or(
    bits_array_t *dest, const bits_array_t *src1, const bits_array_t *src2);
void bit_array_xor(
    bits_array_t *dest, const bits_array_t *src1, const bits_array_t *src2);
void bit_array_not(bits_array_t *dest, const bits_array_t *src);

//
// Comparisons
//

// Note: (bit_array_cmp(a,b) == 0) <=> (bit_array_cmp_big_endian(a,b) == 0)

// comparison functions return:
//   1 iff bitarr1 > bitarr2
//   0 iff bitarr1 == bitarr2
//  -1 iff bitarr1 < bitarr2

// Compare two bit arrays by value stored, with index 0 being the Least
// Significant Bit (LSB). Arrays do not have to be the same length.
// Example: ..0101 (5) > ...0011 (3) [index 0 is LSB at right hand side]
int bit_array_cmp(const bits_array_t *bitarr1, const bits_array_t *bitarr2);

// Compare two bit arrays by value stored, with index 0 being the Most
// Significant Bit (MSB). Arrays do not have to be the same length.
// Example: 10.. > 01.. [index 0 is MSB at left hand side]
int bit_array_cmp_big_endian(
    const bits_array_t *bitarr1, const bits_array_t *bitarr2);

// compare bitarr with (bitarr2 << pos)
int bit_array_cmp_words(
    const bits_array_t *bitarr, bit_index_t pos, const bits_array_t *bitarr2);

//
// Shift, interleave, reverse
//

// Shift array left/right.  If fill is zero, filled with 0, otherwise 1
void bit_array_shift_right(
    bits_array_t *bitarr, bit_index_t shift_dist, char fill);
void bit_array_shift_left(
    bits_array_t *bitarr, bit_index_t shift_dist, char fill);

// shift left without losing any bits. Resizes bitarr.
void bit_array_shift_left_extend(
    bits_array_t *bitarr, bit_index_t shift_dist, char fill);

// Cyclic shift
void bit_array_cycle_right(bits_array_t *bitarr, bit_index_t dist);
void bit_array_cycle_left(bits_array_t *bitarr, bit_index_t dist);

// Interleave
// dst cannot point to the same bit array as src1 or src2
// src1, src2 may point to the same bit array
// abcd 1234 -> a1b2c3d4
// 0011 0000 -> 00001010
// 1111 0000 -> 10101010
// 0101 1010 -> 01100110
// Extends dst if it is too short, but does not shrink it if it is too long
// if dst is longer than length(src1)+length(src2), the end bits are not altered
void bit_array_interleave(
    bits_array_t *dst, const bits_array_t *src1, const bits_array_t *src2);

// Reverse the whole array or part of it
void bit_array_reverse(bits_array_t *bitarr);
void bit_array_reverse_region(
    bits_array_t *bitarr, bit_index_t start, bit_index_t len);

//
// Numeric
//

// Returns 1 on sucess, 0 if value in array is too big
char bit_array_as_num(const bits_array_t *bitarr, uint64_t *result);

// 1 iff bitarr > value
// 0 iff bitarr == value
// -1 iff bitarr < value
int bit_array_cmp_uint64(const bits_array_t *bitarr, uint64_t value);

//
// Arithmetic
//

// bitarr will be extended if needed
void bit_array_add_uint64(bits_array_t *bitarr, uint64_t value);

// Add `add` to `bitarr` at `pos` -- same as:
//   bitarr + (add << pos)
// where pos can be bigger than the length of the array (bitarr will be resized)
void bit_array_add_word(bits_array_t *bitarr, bit_index_t pos, uint64_t add);

// Add `add` to `bitarr` at `pos`
void bit_array_add_words(
    bits_array_t *bitarr, bit_index_t pos, const bits_array_t *add);

// If value is greater than bitarr, bitarr is not changed and 0 is returned
// Returns 1 on success, 0 if value > bitarr
char bit_array_sub_uint64(bits_array_t *bitarr, uint64_t value);

// minus `minus` from `bitarr` at `pos` -- same as:
//   bitarr + (minus << pos)
// Returns 1 on success, 0 if value > bitarr
char bit_array_sub_word(bits_array_t *bitarr, bit_index_t pos, word_t minus);

// minus `minus` from `bitarr` at `pos`
// Returns 1 on success, 0 if value > bitarr
char bit_array_sub_words(
    bits_array_t *bitarr, bit_index_t pos, bits_array_t *minus);

// Multiply by some value
void bit_array_mul_uint64(bits_array_t *bitarr, uint64_t multiplier);

// bitarr = round_down(bitarr / divisor)
// rem = bitarr % divisor
void bit_array_div_uint64(
    bits_array_t *bitarr, uint64_t divisor, uint64_t *rem);

//
// Arithmetic between arrays
//

// dst = src1 + src2
// src1, src2 and dst can all be the same bits_array_t
// If dst is shorter than either of src1, src2, it is enlarged
void bit_array_add(
    bits_array_t *dst, const bits_array_t *src1, const bits_array_t *src2);

// dst = src1 - src2
// src1, src2 and dst can all be the same bits_array_t
// If dst is shorter than src1, it will be extended to be as long as src1
// src1 must be greater than or equal to src2 (src1 >= src2)
void bit_array_subtract(
    bits_array_t *dst, const bits_array_t *src1, const bits_array_t *src2);

// dst = src1 * src2
// Pointers cannot all point to the same bits_array_t
void bit_array_multiply(
    bits_array_t *dst, bits_array_t *src1, bits_array_t *src2);

// Results in:
//   quotient = dividend / divisor
//   dividend = dividend % divisor
// (dividend is used to return the remainder)
void bit_array_divide(
    bits_array_t *dividend, bits_array_t *quotient, bits_array_t *divisor);

//
// Read/Write bits_array_t to a file
//
// File format is [8 bytes: for number of elements in array][data]
// Number of bytes of data is: (int)((num_of_bits + 7) / 8)
//

// Saves bit array to a file
// returns the number of bytes written
bit_index_t bit_array_save(const bits_array_t *bitarr, FILE *f);

// Reads bit array from a file. bitarr is resized and filled.
// Returns 1 on success, 0 on failure
char bit_array_load(bits_array_t *bitarr, FILE *f);

//
// Hash function
//

// Pass seed as 0 on first call, pass previous hash value if rehashing due
// to a collision
// Using bob jenkins hash lookup3
uint64_t bit_array_hash(const bits_array_t *bitarr, uint64_t seed);

//
// Randomness
//

// Set bits randomly with probability prob : 0 <= prob <= 1
void bit_array_random(bits_array_t *bitarr, float prob);

// Shuffle the bits in an array randomly
void bit_array_shuffle(bits_array_t *bitarr);

// Get the next permutation of an array with a fixed size and given number of
// bits set.  Also known as next lexicographic permutation.
// Given a bit array find the next lexicographic orginisation of the bits
// Number of possible combinations given by (size choose bits_set) i.e. nCk
// 00011 -> 00101 -> 00110 -> 01001 -> 01010 ->
// 01100 -> 10001 -> 10010 -> 10100 -> 11000 -> 00011 (back to start)
void bit_array_next_permutation(bits_array_t *bitarr);

//
// Generally useful functions
//

// Generalised 'binary to string' function
// Adds bits to the string in order of lsb to msb
// e.g. 0b11010 (26 in decimal) would come out as "01011"
char *bit_array_word2str(const void *ptr, size_t num_of_bits, char *str);

// Same as above but in reverse
char *bit_array_word2str_rev(const void *ptr, size_t num_of_bits, char *str);

#ifdef __cplusplus
}
#endif

#endif
