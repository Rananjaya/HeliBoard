// SPDX-License-Identifier: GPL-3.0-only
// Tests for reading v2 PtNodes from a buffer of known size. Each buffer is a heap allocation of
// exactly the data's size, so AddressSanitizer reports any read past its end, even by one byte
// (a memory-mapped dictionary file only faults at a page boundary).

#include "dictionary/structure/pt_common/patricia_trie_reading_utils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "defines.h"
#include "dictionary/structure/v2/bigram/bigram_list_policy.h"
#include "dictionary/structure/v2/shortcut/shortcut_list_policy.h"
#include "dictionary/utils/byte_array_utils.h"
#include "utils/byte_array_view.h"

namespace latinime {
namespace {

typedef PatriciaTrieReadingUtils PtReadingUtils;

const uint8_t CHILDREN_ONE_BYTE = 0x40;
const uint8_t CHILDREN_THREE_BYTES = 0xC0;
const uint8_t HAS_MULTIPLE_CHARS = 0x20;
const uint8_t IS_TERMINAL = 0x10;
const uint8_t HAS_SHORTCUT_TARGETS = 0x08;
const uint8_t HAS_BIGRAMS = 0x04;
const uint8_t TERMINATOR = 0x1F;
// More than the fast path needs after a PtNode (its largest fixed part).
const size_t FILLER_SIZE = 300;

struct PtNodeInfo {
    PtReadingUtils::NodeFlags flags = 0;
    int codePointCount = 0;
    int codePoints[MAX_WORD_LENGTH];
    int probability = NOT_A_PROBABILITY;
    int childrenPos = NOT_A_DICT_POS;
    int shortcutPos = NOT_A_DICT_POS;
    int bigramPos = NOT_A_DICT_POS;
    int siblingPos = NOT_A_DICT_POS;
};

// Reads the PtNode at position 0 from an exact-size heap copy of data.
PtNodeInfo readPtNode(const std::vector<uint8_t> &data, const int *const codePointTable = nullptr) {
    std::unique_ptr<uint8_t[]> copy(new uint8_t[data.size() > 0 ? data.size() : 1]);
    std::copy(data.begin(), data.end(), copy.get());
    const ReadOnlyByteArrayView buffer(copy.get(), data.size());
    const BigramListPolicy bigramPolicy(buffer);
    const ShortcutListPolicy shortcutPolicy(buffer);
    PtNodeInfo info;
    PtReadingUtils::readPtNodeInfo(buffer, 0, &shortcutPolicy, &bigramPolicy, codePointTable,
            &info.flags, &info.codePointCount, info.codePoints, &info.probability,
            &info.childrenPos, &info.shortcutPos, &info.bigramPos, &info.siblingPos);
    return info;
}

bool isMalformed(const PtNodeInfo &info, const std::vector<uint8_t> &data) {
    return ByteArrayUtils::isPastEnd(ReadOnlyByteArrayView(data.data(), data.size()),
            info.siblingPos);
}

std::vector<uint8_t> withFiller(std::vector<uint8_t> data) {
    data.insert(data.end(), FILLER_SIZE, 0);
    return data;
}

std::vector<uint8_t> multiCharNode(const int length, const uint8_t extraFlags = 0) {
    std::vector<uint8_t> node = { static_cast<uint8_t>(IS_TERMINAL | HAS_MULTIPLE_CHARS
            | extraFlags) };
    for (int i = 0; i < length; ++i) node.push_back(static_cast<uint8_t>('a' + i % 26));
    node.push_back(TERMINATOR);
    node.push_back(100);
    return node;
}

TEST(PatriciaTrieReadingUtilsTest, TestValidPtNodes) {
    // single char, terminal, one-byte children offset: on both the checked and the fast path
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL | CHILDREN_ONE_BYTE, 'a', 90, 5 },
            withFiller({ IS_TERMINAL | CHILDREN_ONE_BYTE, 'a', 90, 5 }) }) {
        const PtNodeInfo info = readPtNode(data);
        EXPECT_FALSE(isMalformed(info, data));
        EXPECT_EQ(1, info.codePointCount);
        EXPECT_EQ('a', info.codePoints[0]);
        EXPECT_EQ(90, info.probability);
        EXPECT_EQ(3 + 5, info.childrenPos);
        EXPECT_EQ(4, info.siblingPos);
    }
    // 3-byte code point (Sinhala KA)
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL, 0x00, 0x0D, 0x9A, 80 },
            withFiller({ IS_TERMINAL, 0x00, 0x0D, 0x9A, 80 }) }) {
        const PtNodeInfo info = readPtNode(data);
        EXPECT_FALSE(isMalformed(info, data));
        EXPECT_EQ(0x0D9A, info.codePoints[0]);
        EXPECT_EQ(5, info.siblingPos);
    }
    // exactly MAX_WORD_LENGTH characters
    for (const auto &data : { multiCharNode(MAX_WORD_LENGTH),
            withFiller(multiCharNode(MAX_WORD_LENGTH)) }) {
        const PtNodeInfo info = readPtNode(data);
        EXPECT_FALSE(isMalformed(info, data));
        EXPECT_EQ(MAX_WORD_LENGTH, info.codePointCount);
        EXPECT_EQ(100, info.probability);
    }
    // shortcut list and bigram list
    const std::vector<uint8_t> withLists = { IS_TERMINAL | HAS_SHORTCUT_TARGETS | HAS_BIGRAMS, 'a',
            100, 0x00, 0x05, 0x0F, 'x', TERMINATOR, 0x80 | 0x10 | 0x05, 3, 0x20 | 0x05, 0, 7 };
    for (const auto &data : { withLists, withFiller(withLists) }) {
        const PtNodeInfo info = readPtNode(data);
        EXPECT_FALSE(isMalformed(info, data));
        EXPECT_EQ(3, info.shortcutPos);
        EXPECT_EQ(8, info.bigramPos);
        EXPECT_EQ(13, info.siblingPos);
    }
}

TEST(PatriciaTrieReadingUtilsTest, TestEveryTruncationIsReadSafely) {
    const std::vector<std::vector<uint8_t>> nodes = {
        multiCharNode(MAX_WORD_LENGTH, CHILDREN_THREE_BYTES),
        { IS_TERMINAL | CHILDREN_THREE_BYTES, 0x00, 0x0D, 0x9A, 80, 0x00, 0x01, 0x00 },
        { IS_TERMINAL | HAS_SHORTCUT_TARGETS | HAS_BIGRAMS, 'a', 100, 0x00, 0x05, 0x0F, 'x',
                TERMINATOR, 0x80 | 0x10 | 0x05, 3, 0x30 | 0x05, 0, 0, 7 },
    };
    for (const auto &node : nodes) {
        for (size_t size = 0; size < node.size(); ++size) {
            SCOPED_TRACE(size);
            const std::vector<uint8_t> data(node.begin(), node.begin() + size);
            const PtNodeInfo info = readPtNode(data);
            EXPECT_TRUE(isMalformed(info, data) || info.codePointCount <= 0);
        }
    }
}

TEST(PatriciaTrieReadingUtilsTest, TestTooLongPtNodeIsMalformed) {
    // More than MAX_WORD_LENGTH characters, at the end of the buffer and with data after it (the
    // fast path). Nothing after the characters may be read.
    for (const auto &data : { multiCharNode(MAX_WORD_LENGTH + 1, CHILDREN_THREE_BYTES),
            withFiller(multiCharNode(MAX_WORD_LENGTH + 1, CHILDREN_THREE_BYTES)),
            multiCharNode(200), withFiller(multiCharNode(200)) }) {
        const PtNodeInfo info = readPtNode(data);
        EXPECT_TRUE(isMalformed(info, data));
    }
    // Characters without a terminator up to the end of the buffer.
    std::vector<uint8_t> unterminated = { IS_TERMINAL | HAS_MULTIPLE_CHARS };
    unterminated.insert(unterminated.end(), 20, 'a');
    EXPECT_TRUE(isMalformed(readPtNode(unterminated), unterminated));
}

TEST(PatriciaTrieReadingUtilsTest, TestUndefinedCodePointTableEntry) {
    std::vector<int> table(0xFF - 0x20 + 1, NOT_A_CODE_POINT);
    table[0] = 'x';
    table[1] = 'y';
    // defined
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x21,
            TERMINATOR, 100 }, withFiller({ IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x21,
            TERMINATOR, 100 }) }) {
        const PtNodeInfo info = readPtNode(data, table.data());
        EXPECT_FALSE(isMalformed(info, data));
        EXPECT_EQ(2, info.codePointCount);
        EXPECT_EQ('y', info.codePoints[1]);
    }
    // 0x22 is not defined: multi-char PtNode is malformed, on both paths
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x22,
            TERMINATOR, 100 }, withFiller({ IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x22,
            TERMINATOR, 100 }) }) {
        EXPECT_TRUE(isMalformed(readPtNode(data, table.data()), data));
    }
    // single-char PtNode with an undefined byte: no code point
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL, 0xFF, 100 },
            withFiller({ IS_TERMINAL, 0xFF, 100 }) }) {
        const PtNodeInfo info = readPtNode(data, table.data());
        EXPECT_TRUE(isMalformed(info, data) || info.codePointCount <= 0);
    }
}

TEST(PatriciaTrieReadingUtilsTest, TestZeroLengthPtNode) {
    for (const auto &data : { std::vector<uint8_t>{ IS_TERMINAL, TERMINATOR, 100 },
            withFiller({ IS_TERMINAL, TERMINATOR, 100 }),
            std::vector<uint8_t>{ IS_TERMINAL | HAS_MULTIPLE_CHARS, TERMINATOR, 100 },
            withFiller({ IS_TERMINAL | HAS_MULTIPLE_CHARS, TERMINATOR, 100 }) }) {
        EXPECT_EQ(0, readPtNode(data).codePointCount);
    }
}

TEST(PatriciaTrieReadingUtilsTest, TestMalformedListsAreReadSafely) {
    // shortcut list size smaller than its own field, or past the end
    for (const uint8_t size : { 0, 1, 0xFF }) {
        for (const auto &data : {
                std::vector<uint8_t>{ IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0, size },
                std::vector<uint8_t>{ IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0, size, 0x0F,
                        'x', TERMINATOR } }) {
            const PtNodeInfo info = readPtNode(data);
            if (size < 2 || size > data.size()) EXPECT_TRUE(isMalformed(info, data));
        }
    }
    // bigram list that says "has next" up to the end of the buffer
    const std::vector<uint8_t> bigrams = { IS_TERMINAL | HAS_BIGRAMS, 'a', 100,
            0x80 | 0x10 | 0x05, 3, 0x80 | 0x30 | 0x05, 0, 0, 7 };
    EXPECT_TRUE(isMalformed(readPtNode(bigrams), bigrams));
}

} // namespace
} // namespace latinime
