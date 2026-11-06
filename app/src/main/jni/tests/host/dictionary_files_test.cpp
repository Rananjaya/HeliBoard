// SPDX-License-Identifier: GPL-3.0-only
// Host-only tests (built by tests/host/run_host_tests.sh, not part of the Android build).
// They open real dictionary files the way BinaryDictionary.open() does, read every word and look it
// up, so AddressSanitizer / UndefinedBehaviorSanitizer can check the whole read path.
//
// RCB_DICTS_DIR:  folder with the bundled dictionaries (app/src/main/assets/dicts)
// RCB_CORPUS_DIR: malformed dictionaries that wrote past the v2 word buffer before Stage 1a
// RCB_READ_CORPUS_DIR: malformed dictionaries that read outside the file before Stage 1b

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
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
    bool corrupted = false;
};

// Reads up to maxWords words with a MAX_WORD_LENGTH buffer followed by guard slots, and looks each one up.
// The dictionary starts at offset in the file.
ReadResult readDictionary(const std::string &path, const long maxWords, const int offset = 0) {
    ReadResult result;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return result;
    auto policy = DictionaryStructureWithBufferPolicyFactory::newPolicyForExistingDictFile(
            path.c_str(), offset, static_cast<int>(st.st_size) - offset, false /* isUpdatable */);
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
    result.corrupted = dictionary.getDictionaryStructurePolicy()->isCorrupted();
    return result;
}

// Same, with the dictionary placed so that it ends at a page boundary: a read past its end then
// touches an unmapped page and is reported by AddressSanitizer (inside the last page it would not).
ReadResult readDictionaryPageAligned(const std::string &path, const long maxWords) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> data((std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
    const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t padding = (pageSize - data.size() % pageSize) % pageSize;
    const char *const tmpDir = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    std::string copy = std::string(tmpDir) + "/rcb_host_dict_XXXXXX";
    const int fd = mkstemp(&copy[0]);
    if (fd < 0) return ReadResult();
    const std::vector<char> zeros(padding, 0);
    const bool written = write(fd, zeros.data(), padding) == static_cast<ssize_t>(padding)
            && write(fd, data.data(), data.size()) == static_cast<ssize_t>(data.size());
    close(fd);
    const ReadResult result = written
            ? readDictionary(copy, maxWords, static_cast<int>(padding)) : ReadResult();
    unlink(copy.c_str());
    return result;
}

std::vector<std::string> dictionaryFiles(const std::string &dir) {
    std::vector<std::string> files;
    DIR *const handle = opendir(dir.c_str());
    if (!handle) return files;
    while (const dirent *const entry = readdir(handle)) {
        const std::string name = entry->d_name;
        if (name.size() >= 5 && name.compare(name.size() - 5, 5, ".dict") == 0) {
            files.push_back(name);
        }
    }
    closedir(handle);
    return files;
}

void expectBundledDictionary(const char *fileName, const long expectedWords) {
    const std::string dir = envDir("RCB_DICTS_DIR");
    ASSERT_FALSE(dir.empty()) << "RCB_DICTS_DIR is not set";
    const ReadResult result = readDictionary(dir + "/" + fileName, 10000000);
    ASSERT_TRUE(result.opened) << fileName;
    EXPECT_EQ(expectedWords, result.words) << fileName;
    EXPECT_LE(result.longestWord, MAX_WORD_LENGTH) << fileName;
    EXPECT_TRUE(result.guardIntact) << fileName;
    EXPECT_FALSE(result.corrupted) << fileName;
    // also when the dictionary ends at a page boundary
    const ReadResult aligned = readDictionaryPageAligned(dir + "/" + fileName, 10000000);
    EXPECT_EQ(expectedWords, aligned.words) << fileName;
    EXPECT_FALSE(aligned.corrupted) << fileName;
}

// Word counts of the bundled dictionaries; a changed count means the file or the reader changed.
TEST(BundledDictionaryTest, TestMainSinhala) { expectBundledDictionary("main_si.dict", 83029); }
TEST(BundledDictionaryTest, TestMainEnglishUs) { expectBundledDictionary("main_en-US.dict", 160715); }
TEST(BundledDictionaryTest, TestEmojiSinhala) { expectBundledDictionary("emoji_si.dict", 7458); }
TEST(BundledDictionaryTest, TestEmojiEnglish) { expectBundledDictionary("emoji_en.dict", 9065); }

// Each file wrote past the 48-slot word buffer before Stage 1a. Now they must be read (or
// rejected) without writing outside the buffer.
TEST(MalformedDictionaryCorpusTest, TestNoWritesPastWordBuffer) {
    const std::string dir = envDir("RCB_CORPUS_DIR");
    ASSERT_FALSE(dir.empty()) << "RCB_CORPUS_DIR is not set";
    const std::vector<std::string> files = dictionaryFiles(dir);
    EXPECT_FALSE(files.empty()) << dir;
    for (const std::string &name : files) {
        SCOPED_TRACE(name);
        for (const ReadResult &result : { readDictionary(dir + "/" + name, 20000),
                readDictionaryPageAligned(dir + "/" + name, 20000) }) {
            EXPECT_LE(result.longestWord, MAX_WORD_LENGTH);
            EXPECT_TRUE(result.guardIntact);
        }
    }
}

// Each file read outside the dictionary before Stage 1b (AddressSanitizer stops the test if one
// still does). Now they must be rejected when opening, or report themselves as corrupted.
TEST(MalformedDictionaryCorpusTest, TestNoReadsOutsideTheFile) {
    const std::string dir = envDir("RCB_READ_CORPUS_DIR");
    ASSERT_FALSE(dir.empty()) << "RCB_READ_CORPUS_DIR is not set";
    const std::vector<std::string> files = dictionaryFiles(dir);
    EXPECT_FALSE(files.empty()) << dir;
    for (const std::string &name : files) {
        SCOPED_TRACE(name);
        for (const ReadResult &result : { readDictionary(dir + "/" + name, 20000),
                readDictionaryPageAligned(dir + "/" + name, 20000) }) {
            EXPECT_TRUE(!result.opened || result.corrupted);
            EXPECT_LE(result.longestWord, MAX_WORD_LENGTH);
            EXPECT_TRUE(result.guardIntact);
        }
    }
}

} // namespace
} // namespace latinime
