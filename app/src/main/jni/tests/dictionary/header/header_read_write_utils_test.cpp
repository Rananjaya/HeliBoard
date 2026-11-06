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

#include "dictionary/header/header_read_write_utils.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "defines.h"
#include "dictionary/interface/dictionary_header_structure_policy.h"
#include "utils/byte_array_view.h"

namespace latinime {
namespace {

TEST(HeaderReadWriteUtilsTest, TestInsertCharactersIntoVector) {
    DictionaryHeaderStructurePolicy::AttributeMap::key_type vector;

    HeaderReadWriteUtils::insertCharactersIntoVector("", &vector);
    EXPECT_TRUE(vector.empty());

    static const char *str = "abc-xyz!?";
    HeaderReadWriteUtils::insertCharactersIntoVector(str, &vector);
    EXPECT_EQ(strlen(str) , vector.size());
    for (size_t i = 0; i < vector.size(); ++i) {
        EXPECT_EQ(str[i], vector[i]);
    }
}

TEST(HeaderReadWriteUtilsTest, TestAttributeMapForInt) {
    DictionaryHeaderStructurePolicy::AttributeMap attributeMap;

    // Returns default value if not exists.
    EXPECT_EQ(-1, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "", -1));
    EXPECT_EQ(100, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "abc", 100));

    HeaderReadWriteUtils::setIntAttribute(&attributeMap, "abc", 10);
    EXPECT_EQ(10, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "abc", 100));
    HeaderReadWriteUtils::setIntAttribute(&attributeMap, "abc", 20);
    EXPECT_EQ(20, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "abc", 100));
    HeaderReadWriteUtils::setIntAttribute(&attributeMap, "abcd", 30);
    EXPECT_EQ(30, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "abcd", 100));
    EXPECT_EQ(20, HeaderReadWriteUtils::readIntAttributeValue(&attributeMap, "abc", 100));
}

TEST(HeaderReadWriteUtilsTest, TestAttributeMapCodeForPoints) {
    DictionaryHeaderStructurePolicy::AttributeMap attributeMap;

    // Returns empty vector if not exists.
    EXPECT_TRUE(HeaderReadWriteUtils::readCodePointVectorAttributeValue(&attributeMap, "").empty());
    EXPECT_TRUE(HeaderReadWriteUtils::readCodePointVectorAttributeValue(
            &attributeMap, "abc").empty());

    HeaderReadWriteUtils::setCodePointVectorAttribute(&attributeMap, "abc", {});
    EXPECT_TRUE(HeaderReadWriteUtils::readCodePointVectorAttributeValue(
            &attributeMap, "abc").empty());

    const std::vector<int> codePoints = { 0x0, 0x20, 0x1F, 0x100000 };
    HeaderReadWriteUtils::setCodePointVectorAttribute(&attributeMap, "abc", codePoints);
    EXPECT_EQ(codePoints, HeaderReadWriteUtils::readCodePointVectorAttributeValue(
            &attributeMap, "abc"));
}

// Header: magic number, version 202, flags, header size, then key/value strings.
std::vector<uint8_t> makeHeader(const std::string &attributes, const int headerSizeDelta = 0) {
    const uint32_t headerSize = static_cast<uint32_t>(12 + attributes.size() + headerSizeDelta);
    std::vector<uint8_t> header = { 0x9B, 0xC1, 0x3A, 0xFE, 0x00, 0xCA, 0x00, 0x00,
            static_cast<uint8_t>(headerSize >> 24), static_cast<uint8_t>(headerSize >> 16),
            static_cast<uint8_t>(headerSize >> 8), static_cast<uint8_t>(headerSize) };
    header.insert(header.end(), attributes.begin(), attributes.end());
    return header;
}

bool isValid(const std::vector<uint8_t> &data) {
    return HeaderReadWriteUtils::isValidHeader(ReadOnlyByteArrayView(data.data(), data.size()));
}

TEST(HeaderReadWriteUtilsTest, TestValidHeaders) {
    EXPECT_TRUE(isValid(makeHeader("locale\x1f" "en\x1f" "version\x1f" "1\x1f")));
    // Missing attributes: allowed, defaults are used.
    EXPECT_TRUE(isValid(makeHeader("")));
    // Body after the header.
    std::vector<uint8_t> withBody = makeHeader("locale\x1f" "si\x1f");
    withBody.insert(withBody.end(), { 1, 0x10, 'a', 100 });
    EXPECT_TRUE(isValid(withBody));
}

TEST(HeaderReadWriteUtilsTest, TestTruncatedHeaders) {
    const std::vector<uint8_t> header = makeHeader("locale\x1f" "en\x1f");
    for (size_t size = 0; size < header.size(); ++size) {
        // header size field says more than the file has
        EXPECT_FALSE(isValid(std::vector<uint8_t>(header.begin(), header.begin() + size)))
                << size;
    }
}

TEST(HeaderReadWriteUtilsTest, TestInvalidHeaderSizes) {
    // larger than the file
    EXPECT_FALSE(isValid(makeHeader("locale\x1f" "en\x1f", 1)));
    EXPECT_FALSE(isValid(makeHeader("", 4096)));
    // smaller than the fixed part
    EXPECT_FALSE(isValid(makeHeader("", -1)));
    EXPECT_FALSE(isValid(makeHeader("", -12)));
    // 0x80000000 and 0xFFFFFFFF must not become negative ints
    std::vector<uint8_t> huge = makeHeader("locale\x1f" "en\x1f");
    huge[8] = 0x80; huge[9] = 0; huge[10] = 0; huge[11] = 0;
    EXPECT_FALSE(isValid(huge));
    huge[8] = 0xFF; huge[9] = 0xFF; huge[10] = 0xFF; huge[11] = 0xFF;
    EXPECT_FALSE(isValid(huge));
}

TEST(HeaderReadWriteUtilsTest, TestMalformedAttributes) {
    // value not terminated before the end of the header
    EXPECT_FALSE(isValid(makeHeader("locale\x1f" "en")));
    // key without value
    EXPECT_FALSE(isValid(makeHeader("locale\x1f")));
    // header ends inside a 3-byte code point
    EXPECT_FALSE(isValid(makeHeader(std::string("k\x1f") + std::string("\x00\x0D", 2))));
    // value longer than the parser allows (2048 code points)
    EXPECT_FALSE(isValid(makeHeader("k\x1f" + std::string(2049, 'v') + "\x1f")));
    EXPECT_TRUE(isValid(makeHeader("k\x1f" + std::string(2048, 'v') + "\x1f")));
    // key longer than 256 code points
    EXPECT_FALSE(isValid(makeHeader(std::string(257, 'k') + "\x1f" "v\x1f")));
}

TEST(HeaderReadWriteUtilsTest, TestFetchAttributesStaysInHeader) {
    // The last value is cut by the header size: it is not read from the bytes after the header.
    std::vector<uint8_t> data = makeHeader("a\x1f" "1\x1f" "b\x1f" "2", 0);
    data.push_back(0x1F);
    DictionaryHeaderStructurePolicy::AttributeMap attributes;
    HeaderReadWriteUtils::fetchAllHeaderAttributes(data.data(), &attributes);
    EXPECT_EQ(1u, attributes.size());
}

TEST(HeaderReadWriteUtilsTest, TestCodePointTable) {
    DictionaryHeaderStructurePolicy::AttributeMap attributes;
    // no table
    EXPECT_TRUE(HeaderReadWriteUtils::readCodePointTable(&attributes).empty());

    // short table: entries after the defined ones are NOT_A_CODE_POINT
    HeaderReadWriteUtils::setCodePointVectorAttribute(&attributes, "codePointTable",
            { 0x0D9A, 'z' });
    std::vector<int> table = HeaderReadWriteUtils::readCodePointTable(&attributes);
    ASSERT_EQ(static_cast<size_t>(HeaderReadWriteUtils::CODE_POINT_TABLE_SIZE), table.size());
    EXPECT_EQ(224u, table.size());
    EXPECT_EQ(0x0D9A, table[0]);
    EXPECT_EQ('z', table[1]);
    for (size_t i = 2; i < table.size(); ++i) EXPECT_EQ(NOT_A_CODE_POINT, table[i]);

    // empty table value
    HeaderReadWriteUtils::setCodePointVectorAttribute(&attributes, "codePointTable", {});
    table = HeaderReadWriteUtils::readCodePointTable(&attributes);
    ASSERT_EQ(224u, table.size());
    EXPECT_EQ(NOT_A_CODE_POINT, table[0]);

    // longer than needed: the extra entries cannot be addressed and are dropped
    HeaderReadWriteUtils::setCodePointVectorAttribute(&attributes, "codePointTable",
            std::vector<int>(300, 'q'));
    table = HeaderReadWriteUtils::readCodePointTable(&attributes);
    ASSERT_EQ(224u, table.size());
    EXPECT_EQ('q', table[223]);
}

}  // namespace
}  // namespace latinime
