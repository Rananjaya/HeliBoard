// SPDX-License-Identifier: GPL-3.0-only
// Host-only tests (built by tests/host/run_host_tests.sh, not part of the Android build).
// They open real dictionary files the way BinaryDictionary.open() does, read every word and look it
// up, so AddressSanitizer / UndefinedBehaviorSanitizer can check the whole read path.
//
// RCB_DICTS_DIR:  folder with the bundled dictionaries (app/src/main/assets/dicts)
// RCB_CORPUS_DIR: folder with malformed dictionaries that overflowed the v2 word buffer before the fix

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "defines.h"
#include "dictionary/property/word_property.h"
#include "dictionary/structure/dictionary_structure_with_buffer_policy_factory.h"
#include "suggest/core/dictionary/dictionary.h"

namespace latinime {
namespace {

// Dictionary only uses JNIEnv for logging (LogUtils::logToJava); FindClass returning null makes it a no-op.
jclass fakeFindClass(JNIEnv *, const char *) { return nullptr; }
void fakeExceptionClear(JNIEnv *) {}
JNIEnv *fakeEnv() {
    static JNINativeInterface_ functions;
    static JNIEnv env;
    memset(&functions, 0, sizeof(functions));
    functions.FindClass = fakeFindClass;
    functions.ExceptionClear = fakeExceptionClear;
    env.functions = &functions;
    return &env;
}

std::string envDir(const char *name) {
    const char *value = getenv(name);
    return value ? value : "";
}

const int GUARD = 0x5A5A5A5A;
const int GUARD_SLOTS = 64;

struct ReadResult {
    bool opened = false;
    long words = 0;
    int longestWord = 0;
    bool guardIntact = true;
};

// Reads up to maxWords words with a MAX_WORD_LENGTH buffer followed by guard slots, and looks each one up.
ReadResult readDictionary(const std::string &path, const long maxWords) {
    ReadResult result;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return result;
    auto policy = DictionaryStructureWithBufferPolicyFactory::newPolicyForExistingDictFile(
            path.c_str(), 0 /* offset */, static_cast<int>(st.st_size), false /* isUpdatable */);
    if (!policy) return result;
    result.opened = true;
    Dictionary dictionary(fakeEnv(), std::move(policy));
    std::vector<int> buffer(MAX_WORD_LENGTH + GUARD_SLOTS, GUARD);
    int token = 0;
    do {
        int count = 0;
        token = dictionary.getNextWordAndNextToken(token, buffer.data(), &count);
        for (int i = MAX_WORD_LENGTH; i < MAX_WORD_LENGTH + GUARD_SLOTS; ++i) {
            if (buffer[i] != GUARD) result.guardIntact = false;
        }
        if (count > result.longestWord) result.longestWord = count;
        if (count <= 0 || count > MAX_WORD_LENGTH) continue;
        ++result.words;
        const CodePointArrayView word(buffer.data(), count);
        dictionary.getProbability(word);
        dictionary.getMaxProbabilityOfExactMatches(word);
        const WordProperty property = dictionary.getWordProperty(word);
        (void) property;
        if (result.words >= maxWords) break;
    } while (token != 0);
    return result;
}

void expectBundledDictionary(const char *fileName, const long expectedWords) {
    const std::string dir = envDir("RCB_DICTS_DIR");
    ASSERT_FALSE(dir.empty()) << "RCB_DICTS_DIR is not set";
    const ReadResult result = readDictionary(dir + "/" + fileName, 10000000);
    ASSERT_TRUE(result.opened) << fileName;
    EXPECT_EQ(expectedWords, result.words) << fileName;
    EXPECT_LE(result.longestWord, MAX_WORD_LENGTH) << fileName;
    EXPECT_TRUE(result.guardIntact) << fileName;
}

// Word counts of the bundled dictionaries; a changed count means the file or the reader changed.
TEST(BundledDictionaryTest, TestMainSinhala) { expectBundledDictionary("main_si.dict", 83029); }
TEST(BundledDictionaryTest, TestMainEnglishUs) { expectBundledDictionary("main_en-US.dict", 160715); }
TEST(BundledDictionaryTest, TestEmojiSinhala) { expectBundledDictionary("emoji_si.dict", 7458); }
TEST(BundledDictionaryTest, TestEmojiEnglish) { expectBundledDictionary("emoji_en.dict", 9065); }

// Each file wrote past the 48-slot word buffer before the fix. Now they must be read (or rejected)
// without writing outside the buffer. Other, still unfixed, out-of-bounds reads are not in this corpus.
TEST(MalformedDictionaryCorpusTest, TestNoWritesPastWordBuffer) {
    const std::string dir = envDir("RCB_CORPUS_DIR");
    ASSERT_FALSE(dir.empty()) << "RCB_CORPUS_DIR is not set";
    DIR *const handle = opendir(dir.c_str());
    ASSERT_NE(nullptr, handle) << dir;
    int files = 0;
    while (const dirent *const entry = readdir(handle)) {
        const std::string name = entry->d_name;
        if (name.size() < 5 || name.compare(name.size() - 5, 5, ".dict") != 0) continue;
        SCOPED_TRACE(name);
        ++files;
        const ReadResult result = readDictionary(dir + "/" + name, 20000);
        EXPECT_LE(result.longestWord, MAX_WORD_LENGTH);
        EXPECT_TRUE(result.guardIntact);
    }
    closedir(handle);
    EXPECT_GT(files, 0);
}

} // namespace
} // namespace latinime
