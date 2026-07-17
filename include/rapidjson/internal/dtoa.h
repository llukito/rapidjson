// Tencent is pleased to support the open source community by making RapidJSON available.
// 
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed 
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
// CONDITIONS OF ANY KIND, either express or implied. See the License for the 
// specific language governing permissions and limitations under the License.

// This is a C++ header-only implementation of Grisu2 algorithm from the publication:
// Loitsch, Florian. "Printing floating-point numbers quickly and accurately with
// integers." ACM Sigplan Notices 45.6 (2010): 233-243.

#ifndef RAPIDJSON_DTOA_
#define RAPIDJSON_DTOA_

#include "itoa.h" // GetDigitsLut()
#include "diyfp.h"
#include "ieee754.h"

RAPIDJSON_NAMESPACE_BEGIN
namespace internal {

#ifdef __GNUC__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(effc++)
RAPIDJSON_DIAG_OFF(array-bounds) // some gcc versions generate wrong warnings https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
#endif

inline void GrisuRound(char* buffer, int len, uint64_t delta, uint64_t rest, uint64_t ten_kappa, uint64_t wp_w) {
    while (rest < wp_w && delta - rest >= ten_kappa &&
           (rest + ten_kappa < wp_w ||  /// closer
            wp_w - rest > rest + ten_kappa - wp_w)) {
        buffer[len - 1]--;
        rest += ten_kappa;
    }
}

inline int CountDecimalDigit32(uint32_t n) {
    // Simple pure C++ implementation was faster than __builtin_clz version in this situation.
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    // Will not reach 10 digits in DigitGen()
    //if (n < 1000000000) return 9;
    //return 10;
    return 9;
}

inline void DigitGen(const DiyFp& W, const DiyFp& Mp, uint64_t delta, char* buffer, int* len, int* K) {
    static const uint64_t kPow10[] = { 1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL, 100000000ULL,
                                       1000000000ULL, 10000000000ULL, 100000000000ULL, 1000000000000ULL,
                                       10000000000000ULL, 100000000000000ULL, 1000000000000000ULL,
                                       10000000000000000ULL, 100000000000000000ULL, 1000000000000000000ULL,
                                       10000000000000000000ULL };
    const DiyFp one(uint64_t(1) << -Mp.e, Mp.e);
    const DiyFp wp_w = Mp - W;
    uint32_t p1 = static_cast<uint32_t>(Mp.f >> -one.e);
    uint64_t p2 = Mp.f & (one.f - 1);
    int kappa = CountDecimalDigit32(p1); // kappa in [0, 9]
    *len = 0;

    while (kappa > 0) {
        uint32_t d = 0;
        switch (kappa) {
            case  9: d = p1 /  100000000; p1 %=  100000000; break;
            case  8: d = p1 /   10000000; p1 %=   10000000; break;
            case  7: d = p1 /    1000000; p1 %=    1000000; break;
            case  6: d = p1 /     100000; p1 %=     100000; break;
            case  5: d = p1 /      10000; p1 %=      10000; break;
            case  4: d = p1 /       1000; p1 %=       1000; break;
            case  3: d = p1 /        100; p1 %=        100; break;
            case  2: d = p1 /         10; p1 %=         10; break;
            case  1: d = p1;              p1 =           0; break;
            default:;
        }
        if (d || *len)
            buffer[(*len)++] = static_cast<char>('0' + static_cast<char>(d));
        kappa--;
        uint64_t tmp = (static_cast<uint64_t>(p1) << -one.e) + p2;
        if (tmp <= delta) {
            *K += kappa;
            GrisuRound(buffer, *len, delta, tmp, kPow10[kappa] << -one.e, wp_w.f);
            return;
        }
    }

    // kappa = 0
    for (;;) {
        p2 *= 10;
        delta *= 10;
        char d = static_cast<char>(p2 >> -one.e);
        if (d || *len)
            buffer[(*len)++] = static_cast<char>('0' + d);
        p2 &= one.f - 1;
        kappa--;
        if (p2 < delta) {
            *K += kappa;
            int index = -kappa;
            GrisuRound(buffer, *len, delta, p2, one.f, wp_w.f * (index < 20 ? kPow10[index] : 0));
            return;
        }
    }
}

inline void Grisu2(double value, char* buffer, int* length, int* K) {
    const DiyFp v(value);
    DiyFp w_m, w_p;
    v.NormalizedBoundaries(&w_m, &w_p);

    const DiyFp c_mk = GetCachedPower(w_p.e, K);
    const DiyFp W = v.Normalize() * c_mk;
    DiyFp Wp = w_p * c_mk;
    DiyFp Wm = w_m * c_mk;
    Wm.f++;
    Wp.f--;
    DigitGen(W, Wp, Wp.f - Wm.f, buffer, length, K);
}

inline char* WriteExponent(int K, char* buffer) {
    if (K < 0) {
        *buffer++ = '-';
        K = -K;
    }

    if (K >= 100) {
        *buffer++ = static_cast<char>('0' + static_cast<char>(K / 100));
        K %= 100;
        const char* d = GetDigitsLut() + K * 2;
        *buffer++ = d[0];
        *buffer++ = d[1];
    }
    else if (K >= 10) {
        const char* d = GetDigitsLut() + K * 2;
        *buffer++ = d[0];
        *buffer++ = d[1];
    }
    else
        *buffer++ = static_cast<char>('0' + static_cast<char>(K));

    return buffer;
}

inline char* Prettify(char* buffer, int length, int k, int maxDecimalPlaces) {
    const int kk = length + k;  // 10^(kk-1) <= v < 10^kk

    if (0 <= k && kk <= 21) {
        // 1234e7 -> 12340000000
        for (int i = length; i < kk; i++)
            buffer[i] = '0';
        buffer[kk] = '.';
        buffer[kk + 1] = '0';
        return &buffer[kk + 2];
    }
    else if (0 < kk && kk <= 21) {
        // 1234e-2 -> 12.34
        std::memmove(&buffer[kk + 1], &buffer[kk], static_cast<size_t>(length - kk));
        buffer[kk] = '.';
        if (0 > k + maxDecimalPlaces) {
            // When maxDecimalPlaces = 2, 1.2345 -> 1.23, 1.102 -> 1.1
            // Remove extra trailing zeros (at least one) after truncation.
            for (int i = kk + maxDecimalPlaces; i > kk + 1; i--)
                if (buffer[i] != '0')
                    return &buffer[i + 1];
            return &buffer[kk + 2]; // Reserve one zero
        }
        else
            return &buffer[length + 1];
    }
    else if (-6 < kk && kk <= 0) {
        // 1234e-6 -> 0.001234
        const int offset = 2 - kk;
        std::memmove(&buffer[offset], &buffer[0], static_cast<size_t>(length));
        buffer[0] = '0';
        buffer[1] = '.';
        for (int i = 2; i < offset; i++)
            buffer[i] = '0';
        if (length - kk > maxDecimalPlaces) {
            // When maxDecimalPlaces = 2, 0.123 -> 0.12, 0.102 -> 0.1
            // Remove extra trailing zeros (at least one) after truncation.
            for (int i = maxDecimalPlaces + 1; i > 2; i--)
                if (buffer[i] != '0')
                    return &buffer[i + 1];
            return &buffer[3]; // Reserve one zero
        }
        else
            return &buffer[length + offset];
    }
    else if (kk < -maxDecimalPlaces) {
        // Truncate to zero
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        return &buffer[3];
    }
    else if (length == 1) {
        // 1e30
        buffer[1] = 'e';
        return WriteExponent(kk - 1, &buffer[2]);
    }
    else {
        // 1234e30 -> 1.234e33
        std::memmove(&buffer[2], &buffer[1], static_cast<size_t>(length - 1));
        buffer[1] = '.';
        buffer[length + 1] = 'e';
        return WriteExponent(kk - 1, &buffer[0 + length + 2]);
    }
}

inline char* dtoa(double value, char* buffer, int maxDecimalPlaces = 324) {
    RAPIDJSON_ASSERT(maxDecimalPlaces >= 1);
    Double d(value);
    if (d.IsZero()) {
        if (d.Sign())
            *buffer++ = '-';     // -0.0, Issue #289
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        return &buffer[3];
    }
    else {
        if (value < 0) {
            *buffer++ = '-';
            value = -value;
        }
        int length, K;
        Grisu2(value, buffer, &length, &K);
        return Prettify(buffer, length, K, maxDecimalPlaces);
    }
}

// ---------------------------------------------------------------------------
// Shortest round-trip double-to-string without Grisu tables or FP compares.
// Digit generation is pure integer / bitwise arithmetic (Dragon4-style).
// ---------------------------------------------------------------------------

// Minimal multiprecision unsigned integer for ShortestDigitGen.
// Capacity holds values up to roughly 2^1100 / 5^1100 intermediates.
struct DtoaBigint {
    static const int kCapacity = 40; // 40 * 32 = 1280 bits
    uint32_t digits[kCapacity];
    int count; // number of limbs in use; 0 means zero

    void AssignU64(uint64_t v) {
        if (v == 0) {
            count = 0;
            return;
        }
        digits[0] = static_cast<uint32_t>(v);
        digits[1] = static_cast<uint32_t>(v >> 32);
        count = digits[1] ? 2 : 1;
    }

    void AssignPow2(int exp) {
        RAPIDJSON_ASSERT(exp >= 0);
        const int limb = exp / 32;
        const int bit = exp % 32;
        RAPIDJSON_ASSERT(limb < kCapacity);
        std::memset(digits, 0, static_cast<size_t>(limb + 1) * sizeof(uint32_t));
        digits[limb] = uint32_t(1) << bit;
        count = limb + 1;
    }

    bool IsZero() const { return count == 0; }

    int Compare(const DtoaBigint& rhs) const {
        if (count != rhs.count)
            return count < rhs.count ? -1 : 1;
        for (int i = count - 1; i >= 0; i--) {
            if (digits[i] != rhs.digits[i])
                return digits[i] < rhs.digits[i] ? -1 : 1;
        }
        return 0;
    }

    // *this += rhs; assumes no strong size constraints beyond capacity.
    void Add(const DtoaBigint& rhs) {
        uint64_t carry = 0;
        const int n = count > rhs.count ? count : rhs.count;
        for (int i = 0; i < n; i++) {
            carry += (i < count ? digits[i] : 0);
            carry += (i < rhs.count ? rhs.digits[i] : 0);
            digits[i] = static_cast<uint32_t>(carry);
            carry >>= 32;
        }
        count = n;
        if (carry) {
            RAPIDJSON_ASSERT(count < kCapacity);
            digits[count++] = static_cast<uint32_t>(carry);
        }
        // Trim is unnecessary if carry/high limbs stay nonzero.
    }

    // *this -= rhs; requires *this >= rhs.
    void Subtract(const DtoaBigint& rhs) {
        RAPIDJSON_ASSERT(Compare(rhs) >= 0);
        int64_t borrow = 0;
        for (int i = 0; i < count; i++) {
            int64_t diff = static_cast<int64_t>(digits[i]) - borrow
                         - (i < rhs.count ? rhs.digits[i] : 0);
            if (diff < 0) {
                diff += (int64_t(1) << 32);
                borrow = 1;
            }
            else
                borrow = 0;
            digits[i] = static_cast<uint32_t>(diff);
        }
        while (count > 0 && digits[count - 1] == 0)
            count--;
    }

    void MultiplyByU32(uint32_t m) {
        if (m == 0 || count == 0) {
            count = 0;
            return;
        }
        if (m == 1)
            return;
        uint64_t carry = 0;
        for (int i = 0; i < count; i++) {
            carry += static_cast<uint64_t>(digits[i]) * m;
            digits[i] = static_cast<uint32_t>(carry);
            carry >>= 32;
        }
        if (carry) {
            RAPIDJSON_ASSERT(count < kCapacity);
            digits[count++] = static_cast<uint32_t>(carry);
        }
    }

    void ShiftLeft(int shift) {
        if (count == 0 || shift == 0)
            return;
        const int limbShift = shift / 32;
        const int bitShift = shift % 32;
        RAPIDJSON_ASSERT(count + limbShift + (bitShift ? 1 : 0) <= kCapacity);

        if (bitShift == 0) {
            for (int i = count - 1; i >= 0; i--)
                digits[i + limbShift] = digits[i];
            std::memset(digits, 0, static_cast<size_t>(limbShift) * sizeof(uint32_t));
            count += limbShift;
            return;
        }

        // Clear destination high limbs so |= of shifted-in bits is correct.
        std::memset(digits + count, 0, static_cast<size_t>(limbShift + 1) * sizeof(uint32_t));
        for (int i = count - 1; i >= 0; i--) {
            const uint64_t v = static_cast<uint64_t>(digits[i]) << bitShift;
            digits[i + limbShift + 1] |= static_cast<uint32_t>(v >> 32);
            digits[i + limbShift] = static_cast<uint32_t>(v);
        }
        std::memset(digits, 0, static_cast<size_t>(limbShift) * sizeof(uint32_t));
        count += limbShift;
        if (digits[count] != 0)
            count++;
    }

    // Multiply by 5^exp (exp built from small powers only — not Grisu caches).
    void MultiplyPow5(int exp) {
        static const uint32_t kPow5[] = {
            5u, 25u, 125u, 625u, 3125u, 15625u, 78125u, 390625u,
            1953125u, 9765625u, 48828125u, 244140625u // 5^1 .. 5^12
        };
        while (exp >= 13) {
            // 5^13 = 1220703125
            MultiplyByU32(1220703125u);
            exp -= 13;
        }
        if (exp > 0)
            MultiplyByU32(kPow5[exp - 1]);
    }

    void MultiplyPow10(int exp) {
        // 10^exp = 2^exp * 5^exp
        MultiplyPow5(exp);
        ShiftLeft(exp);
    }

    // Single decimal digit quotient: *this = *this % denom, return *this / denom.
    // Requires 0 <= *this / denom <= 9 (as maintained by the digit loop).
    uint32_t DivModTakeDigit(const DtoaBigint& denom) {
        if (Compare(denom) < 0)
            return 0;

        // Binary search digit in [1, 9].
        uint32_t low = 1, high = 9, result = 1;
        DtoaBigint product;
        while (low <= high) {
            const uint32_t mid = (low + high) >> 1;
            product = denom;
            product.MultiplyByU32(mid);
            const int cmp = Compare(product);
            if (cmp >= 0) {
                result = mid;
                low = mid + 1;
            }
            else
                high = mid - 1;
        }
        product = denom;
        product.MultiplyByU32(result);
        Subtract(product);
        return result;
    }
};

// Estimate floor(log10(2^e2 * f)) roughly; used only to pick a starting scale.
// Pure integer: log10(2) ≈ 78913 / 2^18.
inline int EstimateLog10Pow2(int e2) {
    // Returns floor(e2 * log10(2)) for e2 in IEEE double range.
    return static_cast<int>((static_cast<int64_t>(e2) * 78913LL) >> 18);
}

// Generate the shortest decimal digits of significand * 2^exponent that
// round-trip to the same double. No floating-point ops in this routine.
// On return, buffer[0..*length) holds digits and the value equals
// (digit_integer) * 10^(*K).
inline void ShortestDigitGen(uint64_t significand, int exponent, bool lowerBoundaryIsCloser,
                             char* buffer, int* length, int* K) {
    // half-ulp bounds as integers: value = significand * 2^exponent
    // Work with 2x scale so midpoints are integral.
    //   numerator / denominator == value
    //   deltaMinus / denominator == value - lower
    //   deltaPlus  / denominator == upper - value
    DtoaBigint numerator, denominator, deltaMinus, deltaPlus;

    numerator.AssignU64(significand);
    if (exponent >= 0) {
        numerator.ShiftLeft(exponent);
        denominator.AssignU64(1);
    }
    else {
        denominator.AssignPow2(-exponent);
    }

    // Scale by 2 so boundary midpoints are exact.
    numerator.ShiftLeft(1);
    denominator.ShiftLeft(1);

    // Half ulp in this 2x scale is 2^exponent  (or 2^(exponent-1) if closer lower).
    if (exponent >= 0) {
        deltaPlus.AssignPow2(exponent);
    }
    else {
        deltaPlus.AssignU64(1);
        // denominator already has 2^(-exponent + 1); half-ulp = 2^(exponent-1) * 2
        // = 2^exponent in absolute units. Relative to denom (= 2^(-e+1)):
        // half-ulp / (1/2 for the *2 scale)... assign 1 for the common case.
        // value = s * 2^e, scaled num = s*2, denom = 2^(-e+1)
        // half ulp = 2^(e-1); as num-units: half_ulp * 2 / 2^e * denom_factor
        // = 2^e / 2^e = 1. So delta = 1.
    }
    deltaMinus = deltaPlus;

    if (lowerBoundaryIsCloser) {
        // Lower bound is one quarter ulp away: double everything, then halve deltaMinus.
        numerator.ShiftLeft(1);
        denominator.ShiftLeft(1);
        deltaPlus.ShiftLeft(1);
        // deltaMinus stays (half of deltaPlus)
    }

    // even significand => ties round to this value (IEEE ties-to-even)
    const bool even = (significand & 1u) == 0;

    // Decimal exponent estimate: value ~= 10^kRest
    // significand has up to 53 bits; use bit length for log2 estimate.
    int bitLen = 0;
    {
        uint64_t t = significand;
        while (t) {
            bitLen++;
            t >>= 1;
        }
    }
    // log10(value) ≈ (exponent + bitLen - 1) * log10(2)
    int kRest = EstimateLog10Pow2(exponent + bitLen - 1);
    // We want 1 <= value / 10^kRest < 10; adjust via integer compares below.

    // Scale so that numerator/denominator is in [1, 10).
    // Multiply numerator by 10^(-kRest): if kRest >= 0, divide by 10^kRest
    // (i.e. grow denominator); if kRest < 0, grow numerator by 10^(-kRest).
    if (kRest >= 0) {
        denominator.MultiplyPow10(kRest);
    }
    else {
        numerator.MultiplyPow10(-kRest);
        deltaMinus.MultiplyPow10(-kRest);
        deltaPlus.MultiplyPow10(-kRest);
    }

    // Fix over/under estimate of kRest so that 1 <= num/den < 10.
    // num/den >= 1  <=> num >= den
    // num/den < 10 <=> num < 10*den
    {
        DtoaBigint tenDen = denominator;
        tenDen.MultiplyByU32(10);
        if (numerator.Compare(denominator) < 0) {
            numerator.MultiplyByU32(10);
            deltaMinus.MultiplyByU32(10);
            deltaPlus.MultiplyByU32(10);
            kRest--;
        }
        else if (numerator.Compare(tenDen) >= 0) {
            denominator.MultiplyByU32(10);
            kRest++;
        }
    }

    // Digit generation: produce d0.d1d2... with no FP comparisons.
    // low  = (num - deltaMinus) / den   (exclusive or inclusive via `even`)
    // high = (num + deltaPlus)  / den
    *length = 0;
    for (;;) {
        // digit = floor(num / den), num = num % den  (digit in 0..9)
        const uint32_t digit = numerator.DivModTakeDigit(denominator);
        buffer[(*length)++] = static_cast<char>('0' + static_cast<char>(digit));

        // low_ok: remainder <= deltaMinus (or < if not even / open bound)
        // high_ok: remainder + deltaPlus >= den (or > if open)
        DtoaBigint restPlus = numerator;
        restPlus.Add(deltaPlus);

        const int cmpLow = numerator.Compare(deltaMinus);
        const int cmpHigh = restPlus.Compare(denominator);

        const bool low = even ? (cmpLow <= 0) : (cmpLow < 0);
        const bool high = even ? (cmpHigh >= 0) : (cmpHigh > 0);

        if (low || high) {
            // Round: if only high, round up; if only low, keep; if both, tie to closer.
            if (high && !low) {
                // round up
                int i = *length - 1;
                for (;;) {
                    if (buffer[i] < '9') {
                        buffer[i]++;
                        break;
                    }
                    buffer[i] = '0';
                    if (i == 0) {
                        buffer[0] = '1';
                        kRest++;
                        break;
                    }
                    i--;
                }
            }
            else if (low && high) {
                // Tie: round to even digit, or by comparing 2*rest with den.
                // Compare distance: 2*num ? den  (closer to high if 2*num > den)
                DtoaBigint twice = numerator;
                twice.ShiftLeft(1);
                const int cmpTie = twice.Compare(denominator);
                if (cmpTie > 0 || (cmpTie == 0 && ((buffer[*length - 1] - '0') & 1))) {
                    int i = *length - 1;
                    for (;;) {
                        if (buffer[i] < '9') {
                            buffer[i]++;
                            break;
                        }
                        buffer[i] = '0';
                        if (i == 0) {
                            buffer[0] = '1';
                            kRest++;
                            break;
                        }
                        i--;
                    }
                }
            }
            // else only low: leave digits as-is
            break;
        }

        // Continue: num = num * 10, deltas * 10
        numerator.MultiplyByU32(10);
        deltaMinus.MultiplyByU32(10);
        deltaPlus.MultiplyByU32(10);
    }

    // buffer = d0 d1 ... d_{n-1} represents digit_int * 10^(kRest - (n-1))
    // dtoa/Prettify expects: value = digit_int * 10^K  with K = kRest - (length - 1)
    *K = kRest - (*length - 1);

    // Strip trailing zeros from the digit buffer; adjust K.
    while (*length > 1 && buffer[*length - 1] == '0') {
        (*length)--;
        (*K)++;
    }
}

// Shortest round-trip dtoa: no Grisu cached powers, integer digit generation only.
inline char* shortdtoa(double value, char* buffer) {
    Double d(value);
    if (d.IsZero()) {
        if (d.Sign())
            *buffer++ = '-';
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        return &buffer[3];
    }

    if (d.Sign())
        *buffer++ = '-';

    // Integer significand and binary exponent: value = significand * 2^exponent
    const uint64_t significand = d.IntegerSignificand();
    const int exponent = d.IntegerExponent();
    // Lower boundary is closer iff the number is a power of two (normal).
    const bool lowerBoundaryIsCloser = d.IsNormal() && d.Significand() == 0;

    int length, K;
    ShortestDigitGen(significand, exponent, lowerBoundaryIsCloser, buffer, &length, &K);
    return Prettify(buffer, length, K, 324);
}

#ifdef __GNUC__
RAPIDJSON_DIAG_POP
#endif

} // namespace internal
RAPIDJSON_NAMESPACE_END

#endif // RAPIDJSON_DTOA_
