/*
 * Copyright (C) 2013, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LATINIME_BYTE_ARRAY_UTILS_H
#define LATINIME_BYTE_ARRAY_UTILS_H

#include <cstdint>

#include "defines.h"
#include "utils/byte_array_view.h"

namespace latinime {

/**
 * Utility methods for reading byte arrays.
 */
class ByteArrayUtils {
 public:
    /**
     * Integer writing
     *
     * Each method write a corresponding size integer in a big endian manner.
     */
    static AK_FORCE_INLINE void writeUintAndAdvancePosition(uint8_t *const buffer,
            const uint32_t data, const int size, int *const pos) {
        // size must be in 1 to 4.
        ASSERT(size >= 1 && size <= 4);
        switch (size) {
            case 1:
                ByteArrayUtils::writeUint8AndAdvancePosition(buffer, data, pos);
                return;
            case 2:
                ByteArrayUtils::writeUint16AndAdvancePosition(buffer, data, pos);
                return;
            case 3:
                ByteArrayUtils::writeUint24AndAdvancePosition(buffer, data, pos);
                return;
            case 4:
                ByteArrayUtils::writeUint32AndAdvancePosition(buffer, data, pos);
                return;
            default:
                break;
        }
    }

    /**
     * Integer reading
     *
     * Each method read a corresponding size integer in a big endian manner.
     */
    static AK_FORCE_INLINE uint32_t readUint32(const uint8_t *const buffer, const int pos) {
        return (buffer[pos] << 24) ^ (buffer[pos + 1] << 16)
                ^ (buffer[pos + 2] << 8) ^ buffer[pos + 3];
    }

    static AK_FORCE_INLINE uint32_t readUint24(const uint8_t *const buffer, const int pos) {
        return (buffer[pos] << 16) ^ (buffer[pos + 1] << 8) ^ buffer[pos + 2];
    }

    static AK_FORCE_INLINE uint16_t readUint16(const uint8_t *const buffer, const int pos) {
        return (buffer[pos] << 8) ^ buffer[pos + 1];
    }

    static AK_FORCE_INLINE uint8_t readUint8(const uint8_t *const buffer, const int pos) {
        return buffer[pos];
    }

    static AK_FORCE_INLINE uint32_t readUint32AndAdvancePosition(
            const uint8_t *const buffer, int *const pos) {
        const uint32_t value = readUint32(buffer, *pos);
        *pos += 4;
        return value;
    }

    static AK_FORCE_INLINE int readSint24AndAdvancePosition(
            const uint8_t *const buffer, int *const pos) {
        const uint8_t value = readUint8(buffer, *pos);
        if (value < 0x80) {
            return readUint24AndAdvancePosition(buffer, pos);
        } else {
            (*pos)++;
            return -(((value & 0x7F) << 16) ^ readUint16AndAdvancePosition(buffer, pos));
        }
    }

    static AK_FORCE_INLINE uint32_t readUint24AndAdvancePosition(
            const uint8_t *const buffer, int *const pos) {
        const uint32_t value = readUint24(buffer, *pos);
        *pos += 3;
        return value;
    }

    static AK_FORCE_INLINE uint16_t readUint16AndAdvancePosition(
            const uint8_t *const buffer, int *const pos) {
        const uint16_t value = readUint16(buffer, *pos);
        *pos += 2;
        return value;
    }

    static AK_FORCE_INLINE uint8_t readUint8AndAdvancePosition(
            const uint8_t *const buffer, int *const pos) {
        return buffer[(*pos)++];
    }

    static AK_FORCE_INLINE uint32_t readUint(const uint8_t *const buffer,
            const int size, const int pos) {
        // size must be in 1 to 4.
        ASSERT(size >= 1 && size <= 4);
        switch (size) {
            case 1:
                return ByteArrayUtils::readUint8(buffer, pos);
            case 2:
                return ByteArrayUtils::readUint16(buffer, pos);
            case 3:
                return ByteArrayUtils::readUint24(buffer, pos);
            case 4:
                return ByteArrayUtils::readUint32(buffer, pos);
            default:
                return 0;
        }
    }

    /**
     * Code Point Reading
     *
     * 1 byte = bbbbbbbb match
     * case 000xxxxx: xxxxx << 16 + next byte << 8 + next byte
     * else: if 00011111 (= 0x1F) : this is the terminator. This is a relevant choice because
     *       unicode code points range from 0 to 0x10FFFF, so any 3-byte value starting with
     *       00011111 would be outside unicode.
     * else: iso-latin-1 code
     * This allows for the whole unicode range to be encoded, including chars outside of
     * the BMP. Also everything in the iso-latin-1 charset is only 1 byte, except control
     * characters which should never happen anyway (and still work, but take 3 bytes).
     */
    static AK_FORCE_INLINE int readCodePoint(const uint8_t *const buffer, const int pos) {
        int p = pos;
        return readCodePointAndAdvancePosition(buffer, nullptr /* codePointTable */, &p);
    }

    static AK_FORCE_INLINE int readCodePointAndAdvancePosition(
            const uint8_t *const buffer, const int *const codePointTable, int *const pos) {
        /*
         * codePointTable is an array to convert the most frequent characters in this dictionary to
         * 1 byte code points. It is only made of the original code points of the most frequent
         * characters used in this dictionary. 0x20 - 0xFF is used for the 1 byte characters.
         * The original code points are restored by picking the code points at the indices of the
         * codePointTable. The indices are calculated by subtracting 0x20 from the firstByte.
         */
        const uint8_t firstByte = readUint8(buffer, *pos);
        if (firstByte < MINIMUM_ONE_BYTE_CHARACTER_VALUE) {
            if (firstByte == CHARACTER_ARRAY_TERMINATOR) {
                *pos += 1;
                return NOT_A_CODE_POINT;
            } else {
                return readUint24AndAdvancePosition(buffer, pos);
            }
        } else {
            *pos += 1;
            if (codePointTable) {
                return codePointTable[firstByte - MINIMUM_ONE_BYTE_CHARACTER_VALUE];
            }
            return firstByte;
        }
    }

    /**
     * String (array of code points) Reading
     *
     * Reads code points until the terminator is found.
     */
    // Returns the length of the string.
    // outIsTooLong (optional): whether the string was longer than maxLength and was cut.
    static int readStringAndAdvancePosition(const uint8_t *const buffer,
            const int maxLength, const int *const codePointTable, int *const outBuffer,
            int *const pos, bool *const outIsTooLong = nullptr) {
        int length = 0;
        int codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
        while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
            outBuffer[length++] = codePoint;
            codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
        }
        if (outIsTooLong) {
            *outIsTooLong = NOT_A_CODE_POINT != codePoint;
        }
        return length;
    }

    // Advances the position and returns the length of the string.
    static int advancePositionToBehindString(
            const uint8_t *const buffer, const int maxLength, int *const pos) {
        int length = 0;
        int codePoint = readCodePointAndAdvancePosition(buffer, nullptr /* codePointTable */, pos);
        while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
            codePoint = readCodePointAndAdvancePosition(buffer, nullptr /* codePointTable */, pos);
            length++;
        }
        return length;
    }

    /**
     * Bounded reading, for buffers whose size is known (read-only v2 dictionaries).
     *
     * Valid data is read exactly as by the methods above. A read that would leave the buffer reads
     * nothing: it returns 0 (NOT_A_CODE_POINT for code points) and moves the position past the end
     * of the buffer (pos > size). Following reads then fail the same way, and the caller detects
     * the malformed data with isPastEnd(). The buffer size must stay well below INT_MAX
     * (see PatriciaTriePolicy::MAX_DICTIONARY_SIZE).
     */
    static AK_FORCE_INLINE bool isInBounds(const ReadOnlyByteArrayView buffer, const int pos,
            const int length) {
        // Subtraction instead of pos + length, which could overflow.
        return pos >= 0 && static_cast<size_t>(length) <= buffer.size()
                && static_cast<size_t>(pos) <= buffer.size() - static_cast<size_t>(length);
    }

    static AK_FORCE_INLINE bool isPastEnd(const ReadOnlyByteArrayView buffer, const int pos) {
        return pos < 0 || static_cast<size_t>(pos) > buffer.size();
    }

    static AK_FORCE_INLINE void movePastEnd(const ReadOnlyByteArrayView buffer, int *const pos) {
        *pos = static_cast<int>(buffer.size()) + 1;
    }

    static AK_FORCE_INLINE uint32_t readUint24AndAdvancePosition(
            const ReadOnlyByteArrayView buffer, int *const pos) {
        if (!isInBounds(buffer, *pos, 3)) {
            movePastEnd(buffer, pos);
            return 0;
        }
        return readUint24AndAdvancePosition(buffer.data(), pos);
    }

    static AK_FORCE_INLINE uint16_t readUint16AndAdvancePosition(
            const ReadOnlyByteArrayView buffer, int *const pos) {
        if (!isInBounds(buffer, *pos, 2)) {
            movePastEnd(buffer, pos);
            return 0;
        }
        return readUint16AndAdvancePosition(buffer.data(), pos);
    }

    static AK_FORCE_INLINE uint8_t readUint8AndAdvancePosition(
            const ReadOnlyByteArrayView buffer, int *const pos) {
        if (!isInBounds(buffer, *pos, 1)) {
            movePastEnd(buffer, pos);
            return 0;
        }
        return readUint8AndAdvancePosition(buffer.data(), pos);
    }

    // codePointTable must be null or have an entry for every one-byte value 0x20-0xFF (see
    // HeaderReadWriteUtils::readCodePointTable). A NOT_A_CODE_POINT entry is a byte the dictionary
    // does not define, and is treated as malformed data.
    static AK_FORCE_INLINE int readCodePointAndAdvancePosition(
            const ReadOnlyByteArrayView buffer, const int *const codePointTable, int *const pos) {
        if (!isInBounds(buffer, *pos, 1)) {
            movePastEnd(buffer, pos);
            return NOT_A_CODE_POINT;
        }
        const uint8_t firstByte = buffer.data()[*pos];
        if (firstByte < MINIMUM_ONE_BYTE_CHARACTER_VALUE) {
            if (firstByte == CHARACTER_ARRAY_TERMINATOR) {
                *pos += 1;
                return NOT_A_CODE_POINT;
            }
            if (!isInBounds(buffer, *pos, 3)) {
                movePastEnd(buffer, pos);
                return NOT_A_CODE_POINT;
            }
            return readUint24AndAdvancePosition(buffer.data(), pos);
        }
        *pos += 1;
        if (codePointTable) {
            const int codePoint = codePointTable[firstByte - MINIMUM_ONE_BYTE_CHARACTER_VALUE];
            if (codePoint == NOT_A_CODE_POINT) {
                movePastEnd(buffer, pos);
            }
            return codePoint;
        }
        return firstByte;
    }

    // Returns the length of the string, like the method above: at most maxLength code points are
    // stored and a longer string is cut (its next code point is consumed). outIsTooLong (optional)
    // tells whether that happened, for callers that treat it as malformed data.
    // maxLength must be small (MAX_WORD_LENGTH or a header attribute limit).
    static int readStringAndAdvancePosition(const ReadOnlyByteArrayView buffer,
            const int maxLength, const int *const codePointTable, int *const outBuffer,
            int *const pos, bool *const outIsTooLong = nullptr) {
        int length = 0;
        int codePoint;
        if (isInBounds(buffer, *pos, getMaxStringSize(maxLength))) {
            // Fast path: the longest string read here is inside the buffer, so the code points
            // are read without a check each (this is most strings, all but the last few hundred
            // bytes of a file).
            codePoint = readCodePointAndAdvancePosition(buffer.data(), codePointTable, pos);
            while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
                outBuffer[length++] = codePoint;
                codePoint = readCodePointAndAdvancePosition(buffer.data(), codePointTable, pos);
            }
            checkCodePointTableEntry(buffer, codePointTable, codePoint, pos);
        } else {
            codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
            while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
                outBuffer[length++] = codePoint;
                codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
            }
        }
        if (outIsTooLong) {
            *outIsTooLong = NOT_A_CODE_POINT != codePoint;
        }
        return length;
    }

    // Advances the position and returns the length of the string, see above.
    static int advancePositionToBehindString(const ReadOnlyByteArrayView buffer,
            const int maxLength, const int *const codePointTable, int *const pos,
            bool *const outIsTooLong = nullptr) {
        int length = 0;
        int codePoint;
        if (isInBounds(buffer, *pos, getMaxStringSize(maxLength))) {
            // Fast path, see readStringAndAdvancePosition.
            codePoint = readCodePointAndAdvancePosition(buffer.data(), codePointTable, pos);
            while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
                codePoint = readCodePointAndAdvancePosition(buffer.data(), codePointTable, pos);
                length++;
            }
            checkCodePointTableEntry(buffer, codePointTable, codePoint, pos);
        } else {
            codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
            while (NOT_A_CODE_POINT != codePoint && length < maxLength) {
                codePoint = readCodePointAndAdvancePosition(buffer, codePointTable, pos);
                length++;
            }
        }
        if (outIsTooLong) {
            *outIsTooLong = NOT_A_CODE_POINT != codePoint;
        }
        return length;
    }

    // After an unchecked string read that ended on NOT_A_CODE_POINT: if that came from the code
    // point table (a byte the table does not define) instead of the terminator, the data is
    // malformed and the position is moved past the end.
    static AK_FORCE_INLINE void checkCodePointTableEntry(const ReadOnlyByteArrayView buffer,
            const int *const codePointTable, const int lastCodePoint, int *const pos) {
        if (codePointTable && lastCodePoint == NOT_A_CODE_POINT
                && buffer.data()[*pos - 1] != CHARACTER_ARRAY_TERMINATOR) {
            movePastEnd(buffer, pos);
        }
    }

    /**
     * String (array of code points) Writing
     */
    static void writeCodePointsAndAdvancePosition(uint8_t *const buffer,
            const int *const codePoints, const int codePointCount, const bool writesTerminator,
            int *const pos) {
        for (int i = 0; i < codePointCount; ++i) {
            const int codePoint = codePoints[i];
            if (codePoint == NOT_A_CODE_POINT || codePoint == CHARACTER_ARRAY_TERMINATOR) {
                break;
            } else if (codePoint < MINIMUM_ONE_BYTE_CHARACTER_VALUE
                    || codePoint > MAXIMUM_ONE_BYTE_CHARACTER_VALUE) {
                // three bytes character.
                writeUint24AndAdvancePosition(buffer, codePoint, pos);
            } else {
                // one byte character.
                writeUint8AndAdvancePosition(buffer, codePoint, pos);
            }
        }
        if (writesTerminator) {
            writeUint8AndAdvancePosition(buffer, CHARACTER_ARRAY_TERMINATOR, pos);
        }
    }

    static int calculateRequiredByteCountToStoreCodePoints(const int *const codePoints,
            const int codePointCount, const bool writesTerminator) {
        int byteCount = 0;
        for (int i = 0; i < codePointCount; ++i) {
            const int codePoint = codePoints[i];
            if (codePoint == NOT_A_CODE_POINT || codePoint == CHARACTER_ARRAY_TERMINATOR) {
                break;
            } else if (codePoint < MINIMUM_ONE_BYTE_CHARACTER_VALUE
                    || codePoint > MAXIMUM_ONE_BYTE_CHARACTER_VALUE) {
                // three bytes character.
                byteCount += 3;
            } else {
                // one byte character.
                byteCount += 1;
            }
        }
        if (writesTerminator) {
            // The terminator is one byte.
            byteCount += 1;
        }
        return byteCount;
    }

 private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(ByteArrayUtils);

    static const uint8_t MINIMUM_ONE_BYTE_CHARACTER_VALUE;
    static const uint8_t MAXIMUM_ONE_BYTE_CHARACTER_VALUE;
    static const uint8_t CHARACTER_ARRAY_TERMINATOR;

    // Bytes read for a string of at most maxLength code points: maxLength code points and the
    // terminator (or the code point that makes it too long), each at most 3 bytes.
    static AK_FORCE_INLINE int getMaxStringSize(const int maxLength) {
        return (maxLength + 1) * 3;
    }

    static AK_FORCE_INLINE void writeUint32AndAdvancePosition(uint8_t *const buffer,
            const uint32_t data, int *const pos) {
        buffer[(*pos)++] = (data >> 24) & 0xFF;
        buffer[(*pos)++] = (data >> 16) & 0xFF;
        buffer[(*pos)++] = (data >> 8) & 0xFF;
        buffer[(*pos)++] = data & 0xFF;
    }

    static AK_FORCE_INLINE void writeUint24AndAdvancePosition(uint8_t *const buffer,
            const uint32_t data, int *const pos) {
        buffer[(*pos)++] = (data >> 16) & 0xFF;
        buffer[(*pos)++] = (data >> 8) & 0xFF;
        buffer[(*pos)++] = data & 0xFF;
    }

    static AK_FORCE_INLINE void writeUint16AndAdvancePosition(uint8_t *const buffer,
            const uint16_t data, int *const pos) {
        buffer[(*pos)++] = (data >> 8) & 0xFF;
        buffer[(*pos)++] = data & 0xFF;
    }

    static AK_FORCE_INLINE void writeUint8AndAdvancePosition(uint8_t *const buffer,
            const uint8_t data, int *const pos) {
        buffer[(*pos)++] = data & 0xFF;
    }
};
} // namespace latinime
#endif /* LATINIME_BYTE_ARRAY_UTILS_H */
