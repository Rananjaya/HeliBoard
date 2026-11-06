// SPDX-License-Identifier: GPL-3.0-only
// Regression tests for the v2 (format 202) word reader. A malformed dictionary could describe a
// word path longer than MAX_WORD_LENGTH across several PtNode levels, and the reader wrote past
// the caller's MAX_WORD_LENGTH stack buffer (found with AddressSanitizer on crafted files).

#include "dictionary/structure/v2/patricia_trie_policy.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "defines.h"
#include "dictionary/structure/dictionary_structure_with_buffer_policy_factory.h"

namespace latinime {
namespace {

// Node flags, see PatriciaTrieReadingUtils
const uint8_t HAS_MULTIPLE_CHARS = 0x20;
const uint8_t IS_TERMINAL = 0x10;
const uint8_t CHILDREN_TWO_BYTES = 0x80;
const uint8_t CHAR_TERMINATOR = 0x1F;

// Builds a format 202 dictionary whose only word is a chain of PtNodes, one per level.
// levelLengths[i] is the number of code points in the node at depth i.
// The word id of that word (its terminal PtNode position) is written to outWordId.
// Code points from firstCodePoint upward; 0x20-0xFF use one byte, others the 3-byte form.
std::vector<uint8_t> buildChainDictionary(const std::vector<int> &levelLengths, int *outWordId,
        std::vector<int> *outWord, const int firstCodePoint = 'a') {
    const std::string attributes = std::string("dictionary\x1f" "main:test\x1f")
            + "locale\x1f" "en\x1f" + "version\x1f" "1\x1f";
    const int headerSize = 12 + static_cast<int>(attributes.size());
    std::vector<uint8_t> dict = { 0x9B, 0xC1, 0x3A, 0xFE, 0x00, 0xCA, 0x00, 0x00,
            static_cast<uint8_t>(headerSize >> 24), static_cast<uint8_t>(headerSize >> 16),
            static_cast<uint8_t>(headerSize >> 8), static_cast<uint8_t>(headerSize) };
    dict.insert(dict.end(), attributes.begin(), attributes.end());
    std::vector<uint8_t> body;
    int nextChar = firstCodePoint;
    for (size_t level = 0; level < levelLengths.size(); ++level) {
        const bool isLast = level + 1 == levelLengths.size();
        const int length = levelLengths[level];
        body.push_back(1); // PtNode array with one PtNode
        if (isLast) *outWordId = static_cast<int>(body.size());
        uint8_t flags = isLast ? IS_TERMINAL : CHILDREN_TWO_BYTES;
        if (length > 1) flags |= HAS_MULTIPLE_CHARS;
        body.push_back(flags);
        for (int i = 0; i < length; ++i) {
            outWord->push_back(nextChar);
            if (nextChar >= 0x20 && nextChar <= 0xFF) {
                body.push_back(static_cast<uint8_t>(nextChar));
            } else {
                body.push_back(static_cast<uint8_t>(nextChar >> 16));
                body.push_back(static_cast<uint8_t>(nextChar >> 8));
                body.push_back(static_cast<uint8_t>(nextChar));
            }
            if (nextChar == 'z') {
                nextChar = 'a';
            } else if (nextChar == 0x0DC6) {
                nextChar = 0x0D9A; // stay within Sinhala consonants
            } else {
                ++nextChar;
            }
        }
        if (length > 1) body.push_back(CHAR_TERMINATOR);
        if (isLast) {
            body.push_back(100); // probability
        } else {
            // children position is relative to this field; the child array follows directly
            body.push_back(0x00);
            body.push_back(0x02);
        }
    }
    dict.insert(dict.end(), body.begin(), body.end());
    return dict;
}

class ChainDictionary {
 public:
    explicit ChainDictionary(const std::vector<int> &levelLengths, const int firstCodePoint = 'a')
            : mPath(), mWordId(NOT_A_WORD_ID), mWord(), mPolicy() {
        open(buildChainDictionary(levelLengths, &mWordId, &mWord, firstCodePoint));
    }

    // Opens raw dictionary bytes as given.
    explicit ChainDictionary(const std::vector<uint8_t> &data)
            : mPath(), mWordId(NOT_A_WORD_ID), mWord(), mPolicy() {
        open(data);
    }
    ~ChainDictionary() { if (!mPath.empty()) unlink(mPath.c_str()); }

    DictionaryStructureWithBufferPolicy *policy() { return mPolicy.get(); }
    int wordId() const { return mWordId; }
    const std::vector<int> &word() const { return mWord; }

 private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(ChainDictionary);

    void open(const std::vector<uint8_t> &data) {
        const char *const tmpDir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
        mPath = std::string(tmpDir) + "/latinime_v2_policy_testXXXXXX";
        const int fd = mkstemp(&mPath[0]);
        EXPECT_NE(-1, fd);
        EXPECT_EQ(static_cast<ssize_t>(data.size()), write(fd, data.data(), data.size()));
        close(fd);
        mPolicy = DictionaryStructureWithBufferPolicyFactory::newPolicyForExistingDictFile(
                mPath.c_str(), 0 /* bufOffset */, static_cast<int>(data.size()),
                false /* isUpdatable */);
    }

    std::string mPath;
    int mWordId;
    std::vector<int> mWord;
    DictionaryStructureWithBufferPolicy::StructurePolicyPtr mPolicy;
};

const int GUARD = 0x5A5A5A5A;
const int GUARD_SIZE = 64;

// Reads the word into a MAX_WORD_LENGTH area followed by guard values, and checks the guard.
int readWord(ChainDictionary *const dict, std::vector<int> *const outCodePoints) {
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    const int count = dict->policy()->getCodePointsAndReturnCodePointCount(dict->wordId(),
            MAX_WORD_LENGTH, buffer.data());
    for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
        EXPECT_EQ(GUARD, buffer[i]) << "write past MAX_WORD_LENGTH at index " << i;
    }
    outCodePoints->assign(buffer.begin(), buffer.begin() + std::max(count, 0));
    return count;
}

// Same through word iteration, which is the path that crashed on crafted files.
int iterateWord(ChainDictionary *const dict) {
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    int count = 0;
    dict->policy()->getNextWordAndNextToken(0 /* token */, buffer.data(), &count);
    for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
        EXPECT_EQ(GUARD, buffer[i]) << "write past MAX_WORD_LENGTH at index " << i;
    }
    return count;
}

void expectWordIsRead(const std::vector<int> &levelLengths, const int expectedLength,
        const int firstCodePoint = 'a') {
    ChainDictionary dict(levelLengths, firstCodePoint);
    ASSERT_NE(nullptr, dict.policy());
    std::vector<int> codePoints;
    EXPECT_EQ(expectedLength, readWord(&dict, &codePoints));
    EXPECT_EQ(dict.word(), codePoints);
    EXPECT_EQ(expectedLength, iterateWord(&dict));
    EXPECT_FALSE(dict.policy()->isCorrupted());
}

void expectWordIsRejected(const std::vector<int> &levelLengths, const int firstCodePoint = 'a') {
    ChainDictionary dict(levelLengths, firstCodePoint);
    ASSERT_NE(nullptr, dict.policy());
    std::vector<int> codePoints;
    EXPECT_EQ(0, readWord(&dict, &codePoints));
    EXPECT_EQ(0, iterateWord(&dict));
    EXPECT_TRUE(dict.policy()->isCorrupted());
}

TEST(PatriciaTriePolicyTest, TestShortWords) {
    expectWordIsRead({ 1 }, 1);
    expectWordIsRead({ 5 }, 5);
    expectWordIsRead({ 3, 4, 1 }, 8);
}

TEST(PatriciaTriePolicyTest, TestMaxLengthBoundary) {
    // single PtNode
    expectWordIsRead({ MAX_WORD_LENGTH - 1 }, MAX_WORD_LENGTH - 1);
    expectWordIsRead({ MAX_WORD_LENGTH }, MAX_WORD_LENGTH);
    // across PtNode levels
    expectWordIsRead({ 24, 23 }, MAX_WORD_LENGTH - 1);
    expectWordIsRead({ 24, 24 }, MAX_WORD_LENGTH);
    expectWordIsRead({ 16, 16, 16 }, MAX_WORD_LENGTH);
}

TEST(PatriciaTriePolicyTest, TestTooLongSinglePtNodeIsRejected) {
    // Previously this was silently truncated to MAX_WORD_LENGTH.
    expectWordIsRejected({ MAX_WORD_LENGTH + 1 });
    expectWordIsRejected({ 200 });
}

TEST(PatriciaTriePolicyTest, TestTooLongAcrossLevelsIsRejected) {
    // Previously these wrote past the caller's MAX_WORD_LENGTH buffer.
    expectWordIsRejected({ 24, 25 });
    expectWordIsRejected({ 40, 40 });
    expectWordIsRejected({ 1, MAX_WORD_LENGTH });
}

TEST(PatriciaTriePolicyTest, TestDeeplyNestedChainIsRejected) {
    expectWordIsRejected(std::vector<int>(10, 20)); // 200 code points over 10 levels
    expectWordIsRejected(std::vector<int>(MAX_WORD_LENGTH - 1, 2)); // 94 over 47 levels
    expectWordIsRejected(std::vector<int>(MAX_WORD_LENGTH, 2)); // 96 over 48 levels
}

TEST(PatriciaTriePolicyTest, TestManySingleCodePointLevels) {
    expectWordIsRead(std::vector<int>(MAX_WORD_LENGTH, 1), MAX_WORD_LENGTH);
    // 49 levels: never written past the buffer (the level loop also stops at MAX_WORD_LENGTH)
    ChainDictionary dict(std::vector<int>(MAX_WORD_LENGTH + 1, 1));
    ASSERT_NE(nullptr, dict.policy());
    std::vector<int> codePoints;
    EXPECT_EQ(0, readWord(&dict, &codePoints));
    EXPECT_EQ(0, iterateWord(&dict));
}

TEST(PatriciaTriePolicyTest, TestSinhalaCodePointsAtBoundary) {
    // Sinhala code points use the 3-byte form, so the limit must count code points, not bytes.
    const int SINHALA_KA = 0x0D9A;
    expectWordIsRead({ 24, 23 }, MAX_WORD_LENGTH - 1, SINHALA_KA);
    expectWordIsRead({ 24, 24 }, MAX_WORD_LENGTH, SINHALA_KA);
    expectWordIsRejected({ 24, 25 }, SINHALA_KA);
    expectWordIsRejected({ 30, 30, 30 }, SINHALA_KA);
}

TEST(PatriciaTriePolicyTest, TestEmptyAndHeaderOnlyDictionaries) {
    // An empty file is rejected when opening.
    ChainDictionary empty(std::vector<uint8_t>{});
    EXPECT_EQ(nullptr, empty.policy());

    // A valid header with no PtNodes: reading any word id writes nothing.
    int unusedWordId = NOT_A_WORD_ID;
    std::vector<int> unusedWord;
    std::vector<uint8_t> data = buildChainDictionary({ 1 }, &unusedWordId, &unusedWord);
    const int headerSize = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    data.resize(headerSize);
    ChainDictionary headerOnly(data);
    ASSERT_NE(nullptr, headerOnly.policy());
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    EXPECT_EQ(0, headerOnly.policy()->getCodePointsAndReturnCodePointCount(0 /* wordId */,
            MAX_WORD_LENGTH, buffer.data()));
    for (int i = 0; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
        EXPECT_EQ(GUARD, buffer[i]) << "unexpected write at index " << i;
    }
}

TEST(PatriciaTriePolicyTest, TestMissingCharacterTerminatorIsRejected) {
    // A multi-char PtNode whose terminator is missing: the following PtNode bytes are read as
    // more characters. The bounds check must stop the copy and reject the word.
    int wordId = NOT_A_WORD_ID;
    std::vector<int> word;
    std::vector<uint8_t> data = buildChainDictionary({ 30, 30 }, &wordId, &word);
    const int headerSize = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    // first PtNode: [array size][flags][30 chars][terminator] -> replace the terminator
    const size_t terminatorPos = headerSize + 2 + 30;
    ASSERT_NE(static_cast<size_t>(0), data.size());
    EXPECT_EQ(CHAR_TERMINATOR, data[terminatorPos]);
    data[terminatorPos] = 'x';
    // pad with characters so the runaway read stays inside the file
    data.insert(data.end(), 200, 'y');
    ChainDictionary dict(data);
    ASSERT_NE(nullptr, dict.policy());
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    dict.policy()->getCodePointsAndReturnCodePointCount(wordId, MAX_WORD_LENGTH, buffer.data());
    for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
        EXPECT_EQ(GUARD, buffer[i]) << "write past MAX_WORD_LENGTH at index " << i;
    }
}

} // namespace
} // namespace latinime
