/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dictionary/utils/byte_array_utils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "defines.h"
#include "utils/byte_array_view.h"

namespace latinime {
namespace {

TEST(ByteArrayUtilsTest, TestReadCodePointTable) {
    const int codePointTable[] = { 0x6f, 0x6b };
    const uint8_t buffer[] = { 0x20u, 0x21u, 0x00u, 0x01u, 0x00u };
    int pos = 0;
    // Expect the first entry of codePointTable
    EXPECT_EQ(0x6f, ByteArrayUtils::readCodePointAndAdvancePosition(buffer, codePointTable, &pos));
    // Expect the second entry of codePointTable
    EXPECT_EQ(0x6b, ByteArrayUtils::readCodePointAndAdvancePosition(buffer, codePointTable, &pos));
    // Expect the original code point from buffer[2] to buffer[4], 0x100
    // It isn't picked from the codePointTable, since it exceeds the range of the codePointTable.
    EXPECT_EQ(0x100, ByteArrayUtils::readCodePointAndAdvancePosition(buffer, codePointTable, &pos));
}

TEST(ByteArrayUtilsTest, TestReadInt) {
    const uint8_t buffer[] = { 0x1u, 0x8Au, 0x0u, 0xAAu };

    EXPECT_EQ(0x01u, ByteArrayUtils::readUint8(buffer, 0));
    EXPECT_EQ(0x8Au, ByteArrayUtils::readUint8(buffer, 1));
    EXPECT_EQ(0x0u, ByteArrayUtils::readUint8(buffer, 2));
    EXPECT_EQ(0xAAu, ByteArrayUtils::readUint8(buffer, 3));

    EXPECT_EQ(0x018Au, ByteArrayUtils::readUint16(buffer, 0));
    EXPECT_EQ(0x8A00u, ByteArrayUtils::readUint16(buffer, 1));
    EXPECT_EQ(0xAAu, ByteArrayUtils::readUint16(buffer, 2));

    EXPECT_EQ(0x18A00AAu, ByteArrayUtils::readUint32(buffer, 0));

    int pos = 0;
    EXPECT_EQ(0x18A00, ByteArrayUtils::readSint24AndAdvancePosition(buffer, &pos));
    pos = 1;
    EXPECT_EQ(-0xA00AA, ByteArrayUtils::readSint24AndAdvancePosition(buffer, &pos));
}

TEST(ByteArrayUtilsTest, TestWriteAndReadInt) {
    uint8_t buffer[4];

    int pos = 0;
    const uint8_t data_1B = 0xC8;
    ByteArrayUtils::writeUintAndAdvancePosition(buffer, data_1B, 1, &pos);
    EXPECT_EQ(data_1B, ByteArrayUtils::readUint(buffer, 1, 0));

    pos = 0;
    const uint32_t data_4B = 0xABCD1234;
    ByteArrayUtils::writeUintAndAdvancePosition(buffer, data_4B, 4, &pos);
    EXPECT_EQ(data_4B, ByteArrayUtils::readUint(buffer, 4, 0));
}

TEST(ByteArrayUtilsTest, TestReadCodePoint) {
    const uint8_t buffer[] = { 0x10, 0xFF, 0x00u, 0x20u, 0x41u, 0x1Fu, 0x60 };

    EXPECT_EQ(0x10FF00, ByteArrayUtils::readCodePoint(buffer, 0));
    EXPECT_EQ(0x20, ByteArrayUtils::readCodePoint(buffer, 3));
    EXPECT_EQ(0x41, ByteArrayUtils::readCodePoint(buffer, 4));
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePoint(buffer, 5));

    int pos = 0;
    int codePointArray[3];
    EXPECT_EQ(3, ByteArrayUtils::readStringAndAdvancePosition(buffer, MAX_WORD_LENGTH, nullptr,
            codePointArray, &pos));
    EXPECT_EQ(0x10FF00, codePointArray[0]);
    EXPECT_EQ(0x20, codePointArray[1]);
    EXPECT_EQ(0x41, codePointArray[2]);
    EXPECT_EQ(0x60, ByteArrayUtils::readCodePoint(buffer, pos));
}

TEST(ByteArrayUtilsTest, TestWriteAndReadCodePoint) {
    uint8_t buffer[10];

    const int codePointArray[] = { 0x10FF00, 0x20, 0x41 };
    int pos = 0;
    ByteArrayUtils::writeCodePointsAndAdvancePosition(buffer, codePointArray, 3,
            true /* writesTerminator */, &pos);
    EXPECT_EQ(0x10FF00, ByteArrayUtils::readCodePoint(buffer, 0));
    EXPECT_EQ(0x20, ByteArrayUtils::readCodePoint(buffer, 3));
    EXPECT_EQ(0x41, ByteArrayUtils::readCodePoint(buffer, 4));
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePoint(buffer, 5));
}

// Bounded reading (ReadOnlyByteArrayView): reads never leave the buffer, and a read that would
// leaves the position past the end so the caller can reject the data.
TEST(ByteArrayUtilsTest, TestBoundedReadInt) {
    const uint8_t buffer[] = { 0x10, 0x20, 0x30 };
    const ReadOnlyByteArrayView view(buffer, sizeof(buffer));
    const int pastEnd = static_cast<int>(sizeof(buffer)) + 1;

    int pos = 0;
    EXPECT_EQ(0x102030u, ByteArrayUtils::readUint24AndAdvancePosition(view, &pos));
    EXPECT_EQ(3, pos);
    EXPECT_FALSE(ByteArrayUtils::isPastEnd(view, pos)); // exactly at the end is fine

    pos = 1;
    EXPECT_EQ(0x2030u, ByteArrayUtils::readUint16AndAdvancePosition(view, &pos));
    EXPECT_EQ(3, pos);

    // Reads that are partly or completely outside the buffer read nothing.
    pos = 1;
    EXPECT_EQ(0u, ByteArrayUtils::readUint24AndAdvancePosition(view, &pos));
    EXPECT_EQ(pastEnd, pos);
    pos = 2;
    EXPECT_EQ(0u, ByteArrayUtils::readUint16AndAdvancePosition(view, &pos));
    EXPECT_EQ(pastEnd, pos);
    pos = 3;
    EXPECT_EQ(0u, ByteArrayUtils::readUint8AndAdvancePosition(view, &pos));
    EXPECT_EQ(pastEnd, pos);
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, pos));
    // Once past the end, every further read fails the same way.
    EXPECT_EQ(0u, ByteArrayUtils::readUint8AndAdvancePosition(view, &pos));
    EXPECT_EQ(pastEnd, pos);

    // Negative positions.
    pos = -1;
    EXPECT_EQ(0u, ByteArrayUtils::readUint8AndAdvancePosition(view, &pos));
    EXPECT_EQ(pastEnd, pos);
    pos = NOT_A_DICT_POS;
    EXPECT_EQ(0u, ByteArrayUtils::readUint24AndAdvancePosition(view, &pos));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, pos));

    // Empty buffer.
    const ReadOnlyByteArrayView empty;
    pos = 0;
    EXPECT_EQ(0u, ByteArrayUtils::readUint8AndAdvancePosition(empty, &pos));
    EXPECT_EQ(1, pos);
}

TEST(ByteArrayUtilsTest, TestBoundsCheckIntegerLimits) {
    const uint8_t buffer[] = { 1, 2, 3, 4 };
    const ReadOnlyByteArrayView view(buffer, sizeof(buffer));
    EXPECT_TRUE(ByteArrayUtils::isInBounds(view, 0, 4));
    EXPECT_TRUE(ByteArrayUtils::isInBounds(view, 1, 3));
    EXPECT_TRUE(ByteArrayUtils::isInBounds(view, 4, 0));
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, 2, 3));
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, 0, 5));
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, -1, 1));
    // pos + length would overflow an int; the check must not.
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, S_INT_MAX, 1));
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, S_INT_MAX - 1, 3));
    EXPECT_FALSE(ByteArrayUtils::isInBounds(view, S_INT_MIN, 1));
    int pos = S_INT_MAX;
    EXPECT_EQ(0u, ByteArrayUtils::readUint24AndAdvancePosition(view, &pos));
    EXPECT_EQ(5, pos);
    EXPECT_FALSE(ByteArrayUtils::isPastEnd(view, 4));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, 5));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, -1));
}

TEST(ByteArrayUtilsTest, TestBoundedReadCodePoint) {
    // 'a', 3-byte U+0D9A (Sinhala KA), terminator, then a 3-byte code point cut after 2 bytes.
    const uint8_t buffer[] = { 'a', 0x00, 0x0D, 0x9A, 0x1F, 0x00, 0x0D };
    const ReadOnlyByteArrayView view(buffer, sizeof(buffer));
    int pos = 0;
    EXPECT_EQ('a', ByteArrayUtils::readCodePointAndAdvancePosition(view, nullptr, &pos));
    EXPECT_EQ(0x0D9A, ByteArrayUtils::readCodePointAndAdvancePosition(view, nullptr, &pos));
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePointAndAdvancePosition(view, nullptr,
            &pos));
    EXPECT_EQ(5, pos);
    EXPECT_FALSE(ByteArrayUtils::isPastEnd(view, pos));
    // truncated 3-byte code point
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePointAndAdvancePosition(view, nullptr,
            &pos));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, pos));
}

TEST(ByteArrayUtilsTest, TestBoundedReadCodePointTable) {
    // A table as built by HeaderReadWriteUtils::readCodePointTable: two defined entries, the rest
    // NOT_A_CODE_POINT.
    std::vector<int> table(0xFF - 0x20 + 1, NOT_A_CODE_POINT);
    table[0] = 0x0D9A;
    table[1] = 'z';
    const uint8_t buffer[] = { 0x20, 0x21, 0x22, 0xFF };
    const ReadOnlyByteArrayView view(buffer, sizeof(buffer));
    int pos = 0;
    EXPECT_EQ(0x0D9A, ByteArrayUtils::readCodePointAndAdvancePosition(view, table.data(), &pos));
    EXPECT_EQ('z', ByteArrayUtils::readCodePointAndAdvancePosition(view, table.data(), &pos));
    EXPECT_FALSE(ByteArrayUtils::isPastEnd(view, pos));
    // 0x22 is not defined by the table: malformed
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePointAndAdvancePosition(view,
            table.data(), &pos));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, pos));
    // The highest index (0xFF) is inside the table.
    pos = 3;
    EXPECT_EQ(NOT_A_CODE_POINT, ByteArrayUtils::readCodePointAndAdvancePosition(view,
            table.data(), &pos));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(view, pos));
}

TEST(ByteArrayUtilsTest, TestBoundedReadString) {
    const uint8_t terminated[] = { 'a', 'b', 'c', 0x1F, 'x' };
    const ReadOnlyByteArrayView view(terminated, sizeof(terminated));
    int out[8];
    int pos = 0;
    bool isTooLong = true;
    EXPECT_EQ(3, ByteArrayUtils::readStringAndAdvancePosition(view, 8, nullptr, out, &pos,
            &isTooLong));
    EXPECT_EQ(4, pos);
    EXPECT_FALSE(isTooLong);
    EXPECT_EQ('c', out[2]);

    // Longer than maxLength: cut (as before) and reported.
    pos = 0;
    EXPECT_EQ(2, ByteArrayUtils::readStringAndAdvancePosition(view, 2, nullptr, out, &pos,
            &isTooLong));
    EXPECT_TRUE(isTooLong);
    EXPECT_FALSE(ByteArrayUtils::isPastEnd(view, pos));
    // Exactly maxLength code points is not too long.
    pos = 0;
    EXPECT_EQ(3, ByteArrayUtils::advancePositionToBehindString(view, 3, nullptr, &pos,
            &isTooLong));
    EXPECT_FALSE(isTooLong);
    EXPECT_EQ(4, pos);

    // No terminator before the end of the buffer: stops at the end, never reads beyond it.
    const uint8_t unterminated[] = { 'a', 'b', 'c' };
    const ReadOnlyByteArrayView cut(unterminated, sizeof(unterminated));
    pos = 0;
    EXPECT_EQ(3, ByteArrayUtils::readStringAndAdvancePosition(cut, 8, nullptr, out, &pos,
            &isTooLong));
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(cut, pos));
    pos = 0;
    ByteArrayUtils::advancePositionToBehindString(cut, 8, nullptr, &pos, nullptr);
    EXPECT_TRUE(ByteArrayUtils::isPastEnd(cut, pos));
}

}  // namespace
}  // namespace latinime
