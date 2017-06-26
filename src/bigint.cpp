/*
 * Copyright (c) 2017 Andrew Kelley
 *
 * This file is part of zig, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */


#include <stdint.h>
#include "list.hpp"

struct Buf;

struct BigInt {
    // Least significant digit first
    ZigList<uint64_t> digits;
    bool is_negative;
};

enum Cmp {
    CmpLT,
    CmpGT,
    CmpEQ,
};

static void bigint_normalize(BigInt *dest) {
    size_t last_nonzero_digit = SIZE_MAX;
    for (size_t i = 0; i < dest->digits.length; i += 1) {
        uint64_t digit = dest->digits.at(i);
        if (digit != 0) {
            last_nonzero_digit = i;
        }
    }
    if (last_nonzero_digit == SIZE_MAX) {
        dest->is_negative = false;
        dest->digits.resize(0);
    } else {
        dest->digits.resize(last_nonzero_digit + 1);
    }
}

static uint8_t digit_to_char(uint8_t digit, bool uppercase) {
    if (digit >= 0 && digit <= 9) {
        return digit + '0';
    } else if (digit >= 10 && digit <= 35) {
        return digit + (uppercase ? 'A' : 'a');
    } else {
        zig_unreachable();
    }
}

static void to_twos_complement(BigInt *dest, BigInt *src, size_t bit_count) {
    if (src->is_negative) {
        BigInt negated = {0};
        bigint_negate(&negated, src);

        BigInt inverted = {0};
        bigint_not(&inverted, &negated, bit_count);

        BigInt one = {0};
        bigint_init_unsigned(&one, 1);

        bigint_add(dest, &inverted, one);
    } else {
        bigint_init_bigint(dest, src);
    }
}

static void from_twos_complement(BigInt *dest, BigInt *src, size_t bit_count) {
    assert(!src->is_negative);

    if (bit_count == 0) {
        bigint_init_unsigned(dest, 0);
        return;
    }

    if (bit_at_index(src, bit_count - 1)) {
        BigInt negative_one = {0};
        bigint_init_signed(&negative_one, -1);

        BigInt minus_one = {0};
        bigint_add(minus_one, &src, negative_one);

        BigInt inverted = {0};
        bigint_not(inverted, &minus_one, bit_count);

        bigint_negate(dest, &inverted);
    } else {
        bigint_init_bigint(dest, src);
    }
}

static bool bit_at_index(BigInt *bi, size_t index) {
    size_t digit_index = bi->digits.length - (index / 64) - 1;
    size_t digit_bit_index = index % 64;
    uint64_t digit = bi->digits.at(digit_index);
    return ((digit >> digit_bit_index) & 0x1) == 0x1;
}

void bigint_init_unsigned(BigInt *dest, uint64_t x) {
    dest->digits.resize(1);
    dest->digits.at(0) = x;
    dest->is_negative = false;
    bigint_normalize(dest);
}

void bigint_init_signed(BigInt *dest, int64_t x) {
    if (x >= 0) {
        return bigint_init_unsigned(dest, x);
    }
    dest->is_negative = true;
    dest->digits.resize(1);
    dest->digits.at(0) = ((uint64_t)(-(x + 1))) + 1;
    bigint_normalize(dest);
}

void bigint_init_bigint(BigInt *dest, BigInt *src) {
    dest->digits.resize(src->digits.length);
    for (size_t i = 0; i < src->digits.length; i += 1) {
        dest->digits.at(i) = src->digits.at(i);
    }
    dest->is_negative = src->is_negative;
}

bool bigint_fits_in_bits(BigInt *bn, size_t bit_count, bool is_signed) {
    if (bit_count == 0) {
        BigInt zero = {0};
        bigint_init_unsigned(&zero, 0);
        return bigint_cmp(bn, zero) == CmpEQ;
    }

    if (!is_signed) {
        size_t full_bits = bn->digits.length * 64;
        size_t leading_zero_count = bigint_clz(bn, full_bits);
        return bit_count >= full_bits - leading_zero_count;
    }

    BigInt one = {0};
    bigint_init_unsigned(&one, 1);

    BigInt shl_amt = {0};
    bigint_init_unsigned(&shl_amt, bit_count - 1);

    BigInt max_value_plus_one = {0};
    bigint_shl(&max_value_plus_one, &one, &shl_amt);

    BigInt max_value = {0};
    bigint_sub(&max_value, &max_value_plus_one, &one);

    BigInt min_value = {0};
    bigint_negate(&min_value, &max_value_plus_one);

    Cmp min_cmp = bigint_cmp(bn, &min_value);
    Cmp max_cmp = bigint_cmp(bn, &max_value);

    return (min_cmp == CmpGT || min_cmp == CmpEQ) && (max_cmp == CmpLT || max_cmp == CmpEQ);
}

void bigint_write_twos_complement(BigInt *big_int, uint8_t *buf, bool is_big_endian, size_t bit_count) {
    if (bit_count == 0)
        return;

    BigInt twos_comp = {0};
    to_twos_complement(&twos_comp, big_int, bit_count);

    size_t bits_in_last_digit = bit_count % 64;
    size_t bytes_in_last_digit = (bits_in_last_digit + 7) / 8;
    size_t unwritten_byte_count = 8 - bytes_in_last_digit;

    if (is_big_endian) {
        size_t last_digit_index = (bit_count - 1) / 64;
        size_t digit_index = last_digit_index;
        size_t buf_index = 0;
        for (;;) {
            uint64_t x = (digit_index < twos_comp.digits.length) ? twos_comp.digits.items[digit_index] : 0;

            for (size_t byte_index = 7;;) {
                uint8_t byte = x & 0xff;
                if (digit_index == last_digit_index) {
                    buf[buf_index + byte_index - unwritten_byte_count] = byte;
                    if (byte_index == unwritten_byte_count) break;
                } else {
                    buf[buf_index + byte_index] = byte;
                }

                if (byte_index == 0) break;
                byte_index -= 1;
                x >>= 8;
            }

            if (digit_index == 0) break;
            digit_index -= 1;
            if (digit_index == last_digit_index) {
                buf_index += bytes_in_last_digit
            } else {
                buf_index += 8;
            }
        }
    } else {
        size_t digit_count = (bit_count + 63) / 64;
        size_t buf_index = 0;
        for (size_t digit_index = 0; digit_index < digit_count; digit_index += 1) {
            uint64_t x = (digit_index < twos_comp.digits.length) ? twos_comp.digits.items[digit_index] : 0;

            for (size_t byte_index = 0; byte_index < 8; byte_index += 1) {
                uint8_t byte = x & 0xff;
                buf[buf_index] = byte;
                buf_index += 1;
                if (buf_index >= unwritten_byte_count) {
                    break;
                }
                x >>= 8;
            }
        }
    }
}


void bigint_read_twos_complement(BigInt *dest, const uint8_t *buf, size_t bit_count, bool is_big_endian,
        bool is_signed)
{
    if (bit_count == 0) {
        bigint_init_unsigned(dest, 0);
        return;
    }

    size_t digit_count = (bit_count + 63) / 64;
    dest->digits.resize(digit_count);

    size_t bits_in_last_digit = bit_count % 64;
    size_t bytes_in_last_digit = (bits_in_last_digit + 7) / 8;
    size_t unread_byte_count = 8 - bytes_in_last_digit;

    if (is_big_endian) {
        aoeu
    } else {
        size_t buf_index = 0;
        for (size_t digit_index = 0; digit_index < digit_count; digit_index += 1) {
            uint64_t digit = 0;
            size_t end_byte_index = (digit_index == digit_count - 1) ? bytes_in_last_digit : 8;
            for (size_t byte_index = 0; byte_index < end_byte_index; byte_index += 1) {
                uint8_t byte = buf[buf_index];
                buf_index += 1;

                digit <<= 8;
                digit |= byte;
            }
            dest->digits.items[digit_index] = digit;
        }
    }

    if (is_signed) {
        bigint_normalize(dest);
        BigInt tmp = {0};
        bigint_init_bigint(&tmp, dest);
        from_twos_complement(dest, &tmp, bit_count);
    } else {
        dest->is_negative = false;
        bigint_normalize(dest);
    }
}

void bigint_add(BigInt *dest, BigInt *op1, BigInt *op2) {
    if (op1->is_negative == op2->is_negative) {
        dest->is_negative = op1->is_negative;
        dest->digits.resize(0);

        uint64_t overflow = 0;
        for (size_t i = 0;; i += 1) {
            bool found_digit = false;
            uint64_t x = overflow;
            overflow = 0;

            if (i < op1->digits.length) {
                found_digit = true;
                uint64_t digit = op1->digits.at(i);
                if (__builtin_uaddll_overflow(x, digit, &x)) {
                    overflow += 1;
                }
            }

            if (i < op2->digits.length) {
                found_digit = true;
                uint64_t digit = op2->digits.at(i);
                if (__builtin_uaddll_overflow(x, digit, &x)) {
                    overflow += 1;
                }
            }

            dest->digits.append(x);
            if (!found_digit)
                break;
        }
        if (overflow > 0) {
            dest->digits.append(overflow);
        }
        bigint_normalize(dest);
        return;
    }
    BigInt *op_pos;
    BigInt *op_neg;
    if (op1->is_negative) {
        op_neg = op1;
        op_pos = op2;
    } else {
        op_pos = op1;
        op_neg = op2;
    }

    BigInt op_neg_abs = {0};
    bigint_negate(&op_neg_abs, op_neg);
    BigInt *bigger_op;
    BigInt *smaller_op;
    switch (bigint_cmp(op_pos, &op_neg_abs)) {
        case CmpEQ:
            bigint_init_unsigned(dest, 0);
            return;
        case CmpLT:
            bigger_op = &op_neg_abs;
            smaller_op = op_pos;
            dest->is_negative = true;
            break;
        case CmpGT:
            bigger_op = op_pos;
            smaller_op = &op_neg_abs;
            dest->is_negative = false;
            break;
    }
    dest->digits.resize(0);
    uint64_t overflow = 0;
    for (size_t i = 0; ; i += 1) {
        bool found_digit = false;
        uint64_t x = bigger_op->digits.items[i];
        uint64_t prev_overflow = overflow;
        overflow = 0;

        if (i < smaller_op->digits.length) {
            found_digit = true;
            uint64_t digit = smaller_op->digits.items[i];
            if (__builtin_usubll_overflow(x, digit, &x)) {
                overflow += 1;
            }
        }
        if (__builtin_usubll_overflow(x, prev_overflow, &x)) {
            found_digit = true;
            overflow += 1;
        }
        dest->digits.append(x);
        if (!found_digit)
            break;
    }
    assert(overflow == 0);
    bigint_normalize(dest);
}

static void bigint_wrap(BigInt *dest, BigInt *op, size_t bit_count) {
    BigInt one = {0};
    bigint_init_unsigned(&one, 1);

    BigInt bit_count_bi = {0};
    bigint_init_unsigned(&bit_count_bi, bit_count);

    BigInt shifted = {0};
    bigint_shl(&shifted, &one, &bit_count_bi);

    bigint_rem(dest, op, &shifted);
}

void bigint_add_wrap(BigInt *dest, BigInt *op1, BigInt *op2, size_t bit_count) {
    BigInt unwrapped = {0};
    bigint_add(&unwrapped, op1, op2);
    bigint_wrap(dest, &unwrapped, bit_count);
}

void bigint_sub(BigInt *dest, BigInt *op1, BigInt *op2) {
    BigInt op2_negated = {0};
    bigint_negate(&op2_negated, op2);
    return bigint_add(dest, op1, &op2_negated);
}

void bigint_sub_wrap(BigInt *dest, BigInt *op1, BigInt *op2, size_t bit_count) {
    BigInt op2_negated = {0};
    bigint_negate(&op2_negated, op2);
    return bigint_add_wrap(dest, op1, &op2_negated, bit_count);
}

static void mul_overflow(uint64_t op1, uint64_t op2, uint64_t *result, uint64_t *carry) {
    aoeue
}

void bigint_mul(BigInt *dest, BigInt *op1, BigInt *op2) {
    dest->is_negative = (op1->is_negative != op2->is_negative);
    dest->digits.resize(0);

    uint64_t carry = 0;
    for (size_t i = 0;; i += 1) {
        bool found_digit = false;
        uint64_t x;
        uint64_t prev_carry = carry;
        carry = 0;

        if (i < op1->digits.length && i < op2->digits.length) {
            found_digit = true;
            uint64_t digit1 = op1->digits.at(i);
            uint64_t digit2 = op2->digits.at(i);
            mul_overflow(digit1, digit2, &x, &carry);
        } else {
            x = 0;
        }

        if (prev_carry > 0) {
            found_digit = true;
            if (__builtin_uaddll_overflow(x, prev_carry, &x)) {
                carry += 1;
            }
        }

        dest->digits.append(x);
        if (!found_digit)
            break;
    }
    if (carry > 0) {
        dest->digits.append(carry);
    }
    bigint_normalize(dest);
}

void bigint_mul_wrap(BigInt *dest, BigInt *op1, BigInt *op2, size_t bit_count) {
    BigInt unwrapped = {0};
    bigint_mul(&unwrapped, op1, op2);
    bigint_wrap(dest, &unwrapped, bit_count);
}

void bigint_div_trunc(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_div_floor(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_rem(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_mod(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_or(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_and(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_xor(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_shl(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_shl_wrap(BigInt *dest, BigInt *op1, BigInt *op2, size_t bit_count) {
    BigInt unwrapped = {0};
    bigint_shl(&unwrapped, op1, op2);
    bigint_wrap(dest, &unwrapped, bit_count);
}

void bigint_shr(BigInt *dest, BigInt *op1, BigInt *op2) {
    aoeu
}

void bigint_negate(BigInt *dest, BigInt *op) {
    bigint_init_bigint(dest, op);
    dest->is_negative = !dest->is_negative;
    bigint_normalize(dest);
}

void bigint_to_bigfloat(BigFloat *dest, BigInt *op) {
    dest->value = 0.0;
    if (op->digits.length == 0)
        return;

    double base = (double)UINT64_MAX;

    for (size_t i = op->digits.length - 1;;) {
        uint64_t digit = op->digits.at(i);
        dest->value *= base;
        dest->value += (double)digit;

        if (i == 0) return;
        i -= 1;
    }
}

void bigint_not(BigInt *dest, BigInt *op, size_t bit_count) {
    if (bit_count == 0) {
        bigint_init_unsigned(dest, 0);
        return;
    }
    if (op->is_negative) {
        BigInt twos_comp = {0};
        to_twos_complement(&twos_comp, op, bit_count);

        BigInt inverted = {0};
        bigint_not(&inverted, &twos_comp, bit_count);

        from_twos_complement(dest, &inverted, bit_count);
    } else {
        dest->is_negative = false;
        dest->digits.resize(op->digits.length);
        for (size_t i = 0; i < op->digits.length; i += 1) {
            dest->digits.items[i] = ~op->digits.items[i];
        }
        size_t digit_index = dest->digits.length - (bit_count / 64) - 1;
        size_t digit_bit_index = bit_count % 64;
        if (digit_index < dest->digits.length) {
            uint64_t mask = (1 << digit_bit_index) - 1;
            dest->digits.items[digit_index] &= mask;
        }
        bigint_normalize(dest);
    }
}

void bigint_truncate(BigInt *dest, BigInt *op, size_t bit_count) {
    if (bit_count == 0) {
        bigint_init_unsigned(dest, 0);
        return;
    }
    if (op->is_negative) {
        BigInt twos_comp = {0};
        to_twos_complement(&twos_comp, op, bit_count);

        BigInt truncated = {0};
        bigint_truncate(&truncated, &twos_comp, bit_count);

        from_twos_complement(dest, &truncated, bit_count);
    } else {

    }
}

Cmp bigint_cmp(BigInt *op1, BigInt *op2) {
    if (op1->is_negative && !op2->is_negative) {
        return CmpLT;
    } else if (!op1->is_negative && op2->is_negative) {
        return CmpGT;
    } else if (op1->digits.length > op2->digits.length) {
        return op1->is_negative ? CmpLT : CmpGT;
    } else if (op2->digits.length > op1->digits.length) {
        return op1->is_negative ? CmpGT : CmpLT;
    } else if (op1->digits.length == 0) {
        return CmpEQ;
    }
    for (size_t i = op1->digits.length - 1; ;) {
        uint64_t op1_digit = op1->digits.at(i);
        uint64_t op2_digit = op2->digits.at(i);

        if (op1_digit > op2_digit) {
            return op1->is_negative ? CmpLT : CmpGT;
        }

        if (i == 0) {
            return CmpEQ;
        }
        i -= 1;
    }
}

Buf *bigint_to_buf(BigInt *bn, uint64_t base) {
    size_t index = bn->digits.length;

    Buf *result = buf_alloc();
    if (bn->is_negative) {
        buf_append_char(result, '-');
    }
    size_t first_digit_index = buf_len(result);

    BigInt digit_bi = {0};
    BigInt a1 = {0};
    BigInt a2 = {0};

    BigInt *a = &a1;
    BigInt *other_a = &a2;
    bigint_init_bigint(a, bn);

    BigInt base_bi = {0};
    bigint_init_unsigned(&base_bi, 10);

    BigInt zero = {0};
    bigint_init_unsigned(&zero, 0);

    for (;;) {
        bigint_rem(&digit_bi, a, &base_bi);
        uint8_t digit = bigint_to_unsigned(&digit_bi, 8);
        buf_append_char(result, digit_to_char(digit));
        bigint_div_trunc(&other_a, &a, &base_bi);
        {
            BigInt *tmp = a;
            a = other_a;
            other_a = tmp;
        }
        if (bigint_cmp(a, &zero) == CmpEQ) {
            break;
        }
    }

    // reverse
    for (size_t i = first_digit_index; i < buf_len(result); i += 1) {
        size_t other_i = buf_len(result) + first_digit_index - i - 1;
        uint8_t tmp = buf_ptr(result)[i];
        buf_ptr(result)[i] = buf_ptr(result)[other_i];
        buf_ptr(result)[other_i] = tmp;
    }
    return result;
}

size_t bigint_ctz(BigInt *bi, size_t bit_count) {
    if (bi->digits.length == 0 || bit_count == 0)
        return 0;

    BigInt twos_comp = {0};
    to_twos_complement(&twos_comp, bi, bit_count);

    size_t count = 0;
    for (size_t i = 0; i < bit_count; i += 1) {
        if (bit_at_index(&twos_comp, i))
            return count;
        count += 1;
    }
    return count;
}

size_t bigint_clz(BigInt *bi, size_t bit_count) {
    if (bi->is_negative || bi->digits.length == 0 || bit_count == 0)
        return 0;

    size_t count = 0;
    for (size_t i = bit_count - 1;;) {
        if (bit_at_index(bi, i))
            return count;
        count += 1;

        if (i == 0) break;
        i -= 1;
    }
    return count;
}
