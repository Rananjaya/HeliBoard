// SPDX-License-Identifier: GPL-3.0-only
// Regression tests for the v2 (format 202) dictionary reader, found with AddressSanitizer on
// crafted files. A malformed dictionary could describe a word path longer than MAX_WORD_LENGTH
// across several PtNode levels, and the word reader wrote past the caller's MAX_WORD_LENGTH stack
// buffer. It could also make the reader read outside the file: the header, the code point table,
// PtNodes, children positions and the shortcut and bigram lists.

#include "dictionary/structure/v2/patricia_trie_policy.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "defines.h"
#include "dictionary/property/word_property.h"
#include "dictionary/structure/dictionary_structure_with_buffer_policy_factory.h"
#include "suggest/core/dicnode/dic_node.h"
#include "suggest/core/dicnode/dic_node_utils.h"
#include "suggest/core/dicnode/dic_node_vector.h"
#include "utils/int_array_view.h"

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

    // The dictionary is written at the end of the file, and the file ends at a page boundary.
    // The memory map then ends exactly where the dictionary ends, so a read past its end touches
    // an unmapped page and is reported by the sanitizers (inside the last page it would not be).
    void open(const std::vector<uint8_t> &data) {
        const char *const tmpDir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
        mPath = std::string(tmpDir) + "/latinime_v2_policy_testXXXXXX";
        const int fd = mkstemp(&mPath[0]);
        EXPECT_NE(-1, fd);
        const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        const size_t padding = (pageSize - data.size() % pageSize) % pageSize;
        const std::vector<uint8_t> zeros(padding, 0);
        EXPECT_EQ(static_cast<ssize_t>(padding), write(fd, zeros.data(), padding));
        EXPECT_EQ(static_cast<ssize_t>(data.size()), write(fd, data.data(), data.size()));
        close(fd);
        mPolicy = DictionaryStructureWithBufferPolicyFactory::newPolicyForExistingDictFile(
                mPath.c_str(), static_cast<int>(padding) /* bufOffset */,
                static_cast<int>(data.size()), false /* isUpdatable */);
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

// ---- Out-of-bounds read protection (malformed dictionaries) ----

const uint8_t HAS_SHORTCUT_TARGETS = 0x08;
const uint8_t HAS_BIGRAMS = 0x04;
const uint8_t CHILDREN_ONE_BYTE = 0x40;
const uint8_t CHILDREN_THREE_BYTES = 0xC0;

// A format 202 dictionary from the given header attributes and body bytes.
std::vector<uint8_t> makeDictionary(const std::string &attributes,
        const std::vector<uint8_t> &body) {
    const int headerSize = 12 + static_cast<int>(attributes.size());
    std::vector<uint8_t> dict = { 0x9B, 0xC1, 0x3A, 0xFE, 0x00, 0xCA, 0x00, 0x00,
            static_cast<uint8_t>(headerSize >> 24), static_cast<uint8_t>(headerSize >> 16),
            static_cast<uint8_t>(headerSize >> 8), static_cast<uint8_t>(headerSize) };
    dict.insert(dict.end(), attributes.begin(), attributes.end());
    dict.insert(dict.end(), body.begin(), body.end());
    return dict;
}

const std::string DEFAULT_ATTRIBUTES = std::string("locale\x1f" "en\x1f");

// Calls every reading API on the dictionary. Under ASan/UBSan this fails on any read outside the
// file. Returns the number of words found by iteration.
int readEverything(DictionaryStructureWithBufferPolicy *const policy) {
    int wordCount = 0;
    int token = 0;
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    do {
        int count = 0;
        token = policy->getNextWordAndNextToken(token, buffer.data(), &count);
        if (count > 0) {
            ++wordCount;
            const CodePointArrayView word(buffer.data(), count);
            const int wordId = policy->getWordId(word, false /* forceLowerCaseSearch */);
            policy->getWordId(word, true /* forceLowerCaseSearch */);
            policy->getProbabilityOfWord(WordIdArrayView(), wordId);
            policy->getWordProperty(word);
        }
        for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
            EXPECT_EQ(GUARD, buffer[i]);
        }
    } while (token != 0);
    // The suggestion path: walk the trie with DicNodes, as the suggestion search does.
    std::vector<DicNode> pending(1);
    DicNodeUtils::initAsRoot(policy, WordIdArrayView(), &pending[0]);
    for (int visited = 0; !pending.empty() && visited < 10000; ++visited) {
        const DicNode node = pending.back();
        pending.pop_back();
        DicNodeVector children;
        DicNodeUtils::getAllChildDicNodes(&node, policy, &children);
        const int childCount = children.getSizeAndLock();
        for (int i = 0; i < childCount; ++i) {
            EXPECT_NE(NOT_A_CODE_POINT, children[i]->getNodeCodePoint());
            pending.push_back(*children[i]);
        }
    }
    return wordCount;
}

// Word ids that may point anywhere (e.g. a broken bigram target, or a caller bug). This may mark
// even a valid dictionary as corrupted (the word reader does that for any invalid position), but
// must never read outside the file.
void probeArbitraryWordIds(DictionaryStructureWithBufferPolicy *const policy) {
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    for (const int wordId : { 0, 1, 2, 3, 5, 8, 100, 1 << 20, S_INT_MAX, -2 }) {
        policy->getCodePointsAndReturnCodePointCount(wordId, MAX_WORD_LENGTH, buffer.data());
        policy->getProbabilityOfWord(WordIdArrayView(), wordId);
        const int prevWordIds[] = { wordId };
        policy->getProbabilityOfWord(WordIdArrayView(prevWordIds, 1), 1);
        policy->getShortcutIterator(wordId);
        for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SIZE; ++i) {
            EXPECT_EQ(GUARD, buffer[i]);
        }
    }
}

void expectRejectedWhenOpening(const std::vector<uint8_t> &data) {
    ChainDictionary dict(data);
    EXPECT_EQ(nullptr, dict.policy());
}

// Opens the data, reads everything and expects the dictionary to report itself as corrupted.
void expectReadSafelyAndCorrupted(const std::vector<uint8_t> &data) {
    ChainDictionary dict(data);
    ASSERT_NE(nullptr, dict.policy());
    readEverything(dict.policy());
    EXPECT_TRUE(dict.policy()->isCorrupted());
    probeArbitraryWordIds(dict.policy());
}

TEST(PatriciaTriePolicyTest, TestValidDictionaryIsNotCorrupted) {
    // one word "ab" (multi-char, terminal)
    ChainDictionary dict(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 'a', 'b', CHAR_TERMINATOR, 100 }));
    ASSERT_NE(nullptr, dict.policy());
    EXPECT_EQ(1, readEverything(dict.policy()));
    EXPECT_FALSE(dict.policy()->isCorrupted());
    probeArbitraryWordIds(dict.policy());
}

TEST(PatriciaTriePolicyTest, TestHeaderSizeLargerThanFileIsRejected) {
    std::vector<uint8_t> data = makeDictionary(DEFAULT_ATTRIBUTES, { 1, IS_TERMINAL, 'a', 100 });
    data[11] = static_cast<uint8_t>(data.size() + 1); // header size one byte past the file
    expectRejectedWhenOpening(data);
    data[8] = 0x7F; // 2 GB
    expectRejectedWhenOpening(data);
    data[8] = 0xFF; // would be negative as an int
    expectRejectedWhenOpening(data);
}

TEST(PatriciaTriePolicyTest, TestTruncatedHeaderIsRejected) {
    const std::vector<uint8_t> data = makeDictionary(DEFAULT_ATTRIBUTES, {});
    for (size_t size = 1; size < data.size(); ++size) {
        expectRejectedWhenOpening(std::vector<uint8_t>(data.begin(), data.begin() + size));
    }
    // last attribute value not terminated inside the header
    expectRejectedWhenOpening(makeDictionary("locale\x1f" "en", { 1, IS_TERMINAL, 'a', 100 }));
}

TEST(PatriciaTriePolicyTest, TestMissingHeaderAttributes) {
    ChainDictionary dict(makeDictionary("", { 1, IS_TERMINAL, 'a', 100 }));
    ASSERT_NE(nullptr, dict.policy());
    EXPECT_EQ(1, readEverything(dict.policy()));
    EXPECT_FALSE(dict.policy()->isCorrupted());
    EXPECT_TRUE(dict.policy()->getHeaderStructurePolicy()->getLocale()->empty());
}

TEST(PatriciaTriePolicyTest, TestShortCodePointTable) {
    // Table with two entries: 0x20 -> U+0D9A (Sinhala KA), 0x21 -> 'z'. Header values are stored
    // without the table, so U+0D9A is written in the 3-byte form.
    const std::string attributes = DEFAULT_ATTRIBUTES + "codePointTable\x1f"
            + std::string("\x00\x0D\x9A", 3) + "z\x1f";
    {
        // "KA z" through the table
        ChainDictionary dict(makeDictionary(attributes,
                { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x21, CHAR_TERMINATOR, 100 }));
        ASSERT_NE(nullptr, dict.policy());
        std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
        int count = 0;
        dict.policy()->getNextWordAndNextToken(0, buffer.data(), &count);
        ASSERT_EQ(2, count);
        EXPECT_EQ(0x0D9A, buffer[0]);
        EXPECT_EQ('z', buffer[1]);
        EXPECT_EQ(1, readEverything(dict.policy()));
        EXPECT_FALSE(dict.policy()->isCorrupted());
    }
    // 0x22 and 0xFF are not in the table: invalid code point indexes
    expectReadSafelyAndCorrupted(makeDictionary(attributes,
            { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x22, CHAR_TERMINATOR, 100 }));
    expectReadSafelyAndCorrupted(makeDictionary(attributes, { 1, IS_TERMINAL, 0xFF, 100 }));
    // empty table: no one-byte character is valid
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES + "codePointTable\x1f\x1f",
            { 1, IS_TERMINAL, 0x20, 100 }));
}

TEST(PatriciaTriePolicyTest, TestZeroLengthPtNodeIsRejected) {
    // A single-char PtNode whose only character is the terminator: no code point at all. Its
    // (uninitialized) code point buffer must not be used.
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL, CHAR_TERMINATOR, 100 }));
    // multi-char PtNode with no characters
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, CHAR_TERMINATOR, 100 }));
    // zero-length PtNode next to a valid one
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 2, IS_TERMINAL, 'a', 100, IS_TERMINAL, CHAR_TERMINATOR, 100 }));
    // zero-length child
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, CHILDREN_ONE_BYTE, 'a', 1, 1, IS_TERMINAL, CHAR_TERMINATOR, 100 }));
}

TEST(PatriciaTriePolicyTest, TestTruncatedBodyIsReadSafely) {
    // Every prefix of a dictionary with children, a 3-byte code point, shortcuts and bigrams.
    const std::vector<uint8_t> body = {
        2, // root array: 2 PtNodes
        // "a": terminal, with a shortcut "xy" and a bigram to the second root PtNode, children
        IS_TERMINAL | HAS_SHORTCUT_TARGETS | HAS_BIGRAMS | CHILDREN_ONE_BYTE, 'a', 100, 14,
        0x00, 0x06, 0x0F, 'x', 'y', CHAR_TERMINATOR, // shortcut list (size includes itself)
        0x10 | 0x05, 1, // bigram: 1-byte offset from this field to the next PtNode
        // U+0D9A, terminal
        IS_TERMINAL, 0x00, 0x0D, 0x9A, 90,
        // children of "a": "b"
        1, IS_TERMINAL, 'b', 80,
    };
    const std::vector<uint8_t> full = makeDictionary(DEFAULT_ATTRIBUTES, body);
    {
        ChainDictionary dict(full);
        ASSERT_NE(nullptr, dict.policy());
        EXPECT_EQ(3, readEverything(dict.policy()));
        EXPECT_FALSE(dict.policy()->isCorrupted());
        probeArbitraryWordIds(dict.policy());
    }
    const size_t headerSize = full.size() - body.size();
    for (size_t size = headerSize; size < full.size(); ++size) {
        SCOPED_TRACE(size);
        ChainDictionary dict(std::vector<uint8_t>(full.begin(), full.begin() + size));
        ASSERT_NE(nullptr, dict.policy());
        readEverything(dict.policy());
        if (size > headerSize) EXPECT_TRUE(dict.policy()->isCorrupted());
        probeArbitraryWordIds(dict.policy());
    }
}

TEST(PatriciaTriePolicyTest, TestTruncatedCodePointAtEndOfFile) {
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES, { 1, IS_TERMINAL, 0x00, 0x0D }));
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 'a', 0x00 }));
}

TEST(PatriciaTriePolicyTest, TestTruncatedShortcutData) {
    // shortcut list size field points past the end of the file
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0x00, 0x40, 0x0F, 'x' }));
    // shortcut list size smaller than its own field (would move backwards)
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0x00, 0x00, 0x0F, 'x',
              CHAR_TERMINATOR }));
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0x00, 0x01, 0x0F, 'x',
              CHAR_TERMINATOR }));
    // shortcut list size field cut after one byte
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0x00 }));
    // shortcut target without terminator at the end of the file
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100, 0x00, 0x05, 0x0F, 'x', 'y' }));
}

TEST(PatriciaTriePolicyTest, TestLongShortcutTargetIsStillCut) {
    // The emoji dictionaries have shortcut targets (descriptions) longer than MAX_WORD_LENGTH.
    // They are cut to MAX_WORD_LENGTH as before, not treated as malformed.
    std::vector<uint8_t> body = { 1, IS_TERMINAL | HAS_SHORTCUT_TARGETS, 'a', 100 };
    const int targetLength = MAX_WORD_LENGTH + 12;
    const int listSize = 2 + 1 + targetLength + 1;
    body.push_back(static_cast<uint8_t>(listSize >> 8));
    body.push_back(static_cast<uint8_t>(listSize));
    body.push_back(0x0E);
    for (int i = 0; i < targetLength; ++i) body.push_back(static_cast<uint8_t>('a' + i % 26));
    body.push_back(CHAR_TERMINATOR);
    ChainDictionary dict(makeDictionary(DEFAULT_ATTRIBUTES, body));
    ASSERT_NE(nullptr, dict.policy());
    const int word[] = { 'a' };
    const WordProperty property = dict.policy()->getWordProperty(CodePointArrayView(word, 1));
    const auto &shortcuts = property.getUnigramProperty().getShortcuts();
    ASSERT_EQ(1u, shortcuts.size());
    EXPECT_EQ(static_cast<size_t>(MAX_WORD_LENGTH), shortcuts[0].getTargetCodePoints()->size());
    EXPECT_FALSE(dict.policy()->isCorrupted());
}

TEST(PatriciaTriePolicyTest, TestTruncatedBigramData) {
    // last bigram entry says "has next", but the file ends
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_BIGRAMS, 'a', 100, 0x80 | 0x10 | 0x05, 0 }));
    // bigram address cut
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | HAS_BIGRAMS, 'a', 100, 0x30 | 0x05, 0x00 }));
    // bigram target far outside the file, and before its start
    {
        ChainDictionary dict(makeDictionary(DEFAULT_ATTRIBUTES,
                { 1, IS_TERMINAL | HAS_BIGRAMS, 'a', 100, 0x30 | 0x05, 0xFF, 0xFF, 0xFF }));
        ASSERT_NE(nullptr, dict.policy());
        readEverything(dict.policy());
        probeArbitraryWordIds(dict.policy());
    }
    {
        ChainDictionary dict(makeDictionary(DEFAULT_ATTRIBUTES,
                { 1, IS_TERMINAL | HAS_BIGRAMS, 'a', 100, 0x40 | 0x30 | 0x05, 0xFF, 0xFF, 0xFF }));
        ASSERT_NE(nullptr, dict.policy());
        readEverything(dict.policy());
        probeArbitraryWordIds(dict.policy());
    }
}

TEST(PatriciaTriePolicyTest, TestInvalidPositionsAndCounts) {
    // children offset cut by the end of the file
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, CHILDREN_THREE_BYTES, 'a', 0x00 }));
    // largest 3-byte children offset: far outside the file
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | CHILDREN_THREE_BYTES, 'a', 100, 0xFF, 0xFF, 0xFF }));
    // children offset pointing exactly at the end of the file
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 1, IS_TERMINAL | CHILDREN_ONE_BYTE, 'a', 100, 1 }));
    // array says 0x7FFF PtNodes, the file has one
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES,
            { 0xFF, 0xFF, IS_TERMINAL, 'a', 100 }));
    // two-byte array size cut
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES, { 0x80 }));
}

TEST(PatriciaTriePolicyTest, TestLongerThanMaxWordLengthPtNodeIsRejected) {
    // A single PtNode with more than MAX_WORD_LENGTH characters is malformed in every read path,
    // not cut to MAX_WORD_LENGTH.
    std::vector<uint8_t> body = { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS };
    for (int i = 0; i < MAX_WORD_LENGTH + 1; ++i) body.push_back('a');
    body.push_back(CHAR_TERMINATOR);
    body.push_back(100);
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES, body));
}

TEST(PatriciaTriePolicyTest, TestMalformedPtNodeFollowedByMoreData) {
    // As above, but with enough data after the PtNode for the reader's fast path (no check per
    // byte): malformed characters must still stop the reading of the PtNode. Before the fix of
    // the fast path, the probability was then read past the end of the buffer.
    const std::vector<uint8_t> filler(400, 0);
    std::vector<uint8_t> tooLong = { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS | CHILDREN_ONE_BYTE };
    for (int i = 0; i < MAX_WORD_LENGTH + 1; ++i) tooLong.push_back('a');
    tooLong.insert(tooLong.end(), { CHAR_TERMINATOR, 100, 1 });
    tooLong.insert(tooLong.end(), filler.begin(), filler.end());
    expectReadSafelyAndCorrupted(makeDictionary(DEFAULT_ATTRIBUTES, tooLong));

    // Multi-char PtNode with a byte the code point table does not define.
    const std::string attributes = DEFAULT_ATTRIBUTES + "codePointTable\x1f" "xy\x1f";
    std::vector<uint8_t> undefined = { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x22,
            CHAR_TERMINATOR, 100 };
    undefined.insert(undefined.end(), filler.begin(), filler.end());
    expectReadSafelyAndCorrupted(makeDictionary(attributes, undefined));

    // The same PtNodes are read correctly when they are valid.
    std::vector<uint8_t> valid = { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS, 0x20, 0x21,
            CHAR_TERMINATOR, 100 };
    valid.insert(valid.end(), filler.begin(), filler.end());
    ChainDictionary dict(makeDictionary(attributes, valid));
    ASSERT_NE(nullptr, dict.policy());
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SIZE, GUARD);
    int count = 0;
    dict.policy()->getNextWordAndNextToken(0, buffer.data(), &count);
    ASSERT_EQ(2, count);
    EXPECT_EQ('x', buffer[0]);
    EXPECT_EQ('y', buffer[1]);
    EXPECT_FALSE(dict.policy()->isCorrupted());
}

TEST(PatriciaTriePolicyTest, TestMaxLengthPtNodeFollowedByMoreData) {
    // A valid PtNode of exactly MAX_WORD_LENGTH characters, read on the fast path.
    std::vector<uint8_t> body = { 1, IS_TERMINAL | HAS_MULTIPLE_CHARS };
    for (int i = 0; i < MAX_WORD_LENGTH; ++i) body.push_back(static_cast<uint8_t>('a' + i % 26));
    body.insert(body.end(), { CHAR_TERMINATOR, 100 });
    body.insert(body.end(), 400, 0);
    ChainDictionary dict(makeDictionary(DEFAULT_ATTRIBUTES, body));
    ASSERT_NE(nullptr, dict.policy());
    EXPECT_EQ(1, readEverything(dict.policy()));
    EXPECT_FALSE(dict.policy()->isCorrupted());
}

} // namespace
} // namespace latinime
