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

#ifndef RAPIDJSON_INTERNAL_STRFUNC_H_
#define RAPIDJSON_INTERNAL_STRFUNC_H_

#include "../stream.h"
#include <cwchar>

RAPIDJSON_NAMESPACE_BEGIN
namespace internal {

//! Custom strlen() which works on different character types.
/*! \tparam Ch Character type (e.g. char, wchar_t, short)
    \param s Null-terminated input string.
    \return Number of characters in the string. 
    \note This has the same semantics as strlen(), the return value is not number of Unicode codepoints.
*/
template <typename Ch>
inline SizeType StrLen(const Ch* s) {
    RAPIDJSON_ASSERT(s != 0);
    const Ch* p = s;
    while (*p) ++p;
    return SizeType(p - s);
}

template <>
inline SizeType StrLen(const char* s) {
    return SizeType(std::strlen(s));
}

template <>
inline SizeType StrLen(const wchar_t* s) {
    return SizeType(std::wcslen(s));
}

//! Custom strcmpn() which works on different character types.
/*! \tparam Ch Character type (e.g. char, wchar_t, short)
    \param s1 Null-terminated input string.
    \param s2 Null-terminated input string.
    \return 0 if equal
*/
template<typename Ch>
inline int StrCmp(const Ch* s1, const Ch* s2) {
    RAPIDJSON_ASSERT(s1 != 0);
    RAPIDJSON_ASSERT(s2 != 0);
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return static_cast<unsigned>(*s1) < static_cast<unsigned>(*s2) ? -1 : static_cast<unsigned>(*s1) > static_cast<unsigned>(*s2);
}

//! Byte-wise lexicographic comparison using only bitwise ops and equality checks.
/*! \tparam Ch Character type (e.g. char, wchar_t)
    \param s1 First string (may contain embedded nulls).
    \param len1 Length of s1.
    \param s2 Second string (may contain embedded nulls).
    \param len2 Length of s2.
    \return Negative if s1<s2, 0 if equal, positive if s1>s2.
    \note Character ordering uses XOR + MSB isolation instead of relational operators.
*/
template<typename Ch>
inline int BitwiseStrCmp(const Ch* s1, SizeType len1, const Ch* s2, SizeType len2) {
    SizeType minLen = (len1 < len2) ? len1 : len2;
    for (SizeType i = 0; i < minLen; ++i) {
        if (s1[i] != s2[i]) {
            // Determine ordering using only bitwise ops and == / !=
            unsigned char a = static_cast<unsigned char>(s1[i]);
            unsigned char b = static_cast<unsigned char>(s2[i]);
            unsigned char diff = a ^ b; // non-zero bits are the differing positions
            // Isolate the most-significant set bit of diff
            diff |= (diff >> 1);
            diff |= (diff >> 2);
            diff |= (diff >> 4);
            // Now diff has all bits set from the MSB of original diff downward.
            // msb = diff - (diff >> 1) isolates the highest set bit.
            unsigned char msb = diff ^ (diff >> 1);
            // The character with a 1 at that position is larger.
            return (a & msb) != 0 ? 1 : -1;
        }
    }
    // All compared characters equal — shorter string is "less"
    if (len1 == len2) return 0;
    return (len1 < len2) ? -1 : 1;
}

//! Returns number of code points in a encoded string.
template<typename Encoding>
bool CountStringCodePoint(const typename Encoding::Ch* s, SizeType length, SizeType* outCount) {
    RAPIDJSON_ASSERT(s != 0);
    RAPIDJSON_ASSERT(outCount != 0);
    GenericStringStream<Encoding> is(s);
    const typename Encoding::Ch* end = s + length;
    SizeType count = 0;
    while (is.src_ < end) {
        unsigned codepoint;
        if (!Encoding::Decode(is, &codepoint))
            return false;
        count++;
    }
    *outCount = count;
    return true;
}

} // namespace internal
RAPIDJSON_NAMESPACE_END

#endif // RAPIDJSON_INTERNAL_STRFUNC_H_
