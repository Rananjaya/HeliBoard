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

#include "dictionary/structure/pt_common/patricia_trie_reading_utils.h"

#include "defines.h"
#include "dictionary/interface/dictionary_bigrams_structure_policy.h"
#include "dictionary/interface/dictionary_shortcuts_structure_policy.h"
#include "dictionary/utils/byte_array_utils.h"

namespace latinime {

typedef PatriciaTrieReadingUtils PtReadingUtils;

const PtReadingUtils::NodeFlags PtReadingUtils::MASK_CHILDREN_POSITION_TYPE = 0xC0;
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_CHILDREN_POSITION_TYPE_NOPOSITION = 0x00;
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_CHILDREN_POSITION_TYPE_ONEBYTE = 0x40;
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_CHILDREN_POSITION_TYPE_TWOBYTES = 0x80;
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_CHILDREN_POSITION_TYPE_THREEBYTES = 0xC0;

// Flag for single/multiple char group
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_HAS_MULTIPLE_CHARS = 0x20;
// Flag for terminal PtNodes
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_IS_TERMINAL = 0x10;
// Flag for shortcut targets presence
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_HAS_SHORTCUT_TARGETS = 0x08;
// Flag for bigram presence
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_HAS_BIGRAMS = 0x04;
// Flag for non-words (typically, shortcut only entries)
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_IS_NOT_A_WORD = 0x02;
// Flag for possibly offensive words
const PtReadingUtils::NodeFlags PtReadingUtils::FLAG_IS_POSSIBLY_OFFENSIVE = 0x01;

namespace {

// Largest fixed part of a valid PtNode: flags, MAX_WORD_LENGTH code points and the terminator (3
// bytes each at most), probability and a 3-byte children position.
const int MAX_PT_NODE_FIXED_PART_SIZE = 1 + (MAX_WORD_LENGTH + 1) * 3 + 1 + 3;

// Shared by the raw pointer (v4) and ReadOnlyByteArrayView (v2) forms; the ByteArrayUtils overload
// for the buffer type decides whether reads are bounds checked.
template<typename Buffer>
AK_FORCE_INLINE int getPtNodeArraySize(const Buffer buffer, int *const pos) {
    const uint8_t firstByte = ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
    if (firstByte < 0x80) {
        return firstByte;
    } else {
        return ((firstByte & 0x7F) << 8) ^ ByteArrayUtils::readUint8AndAdvancePosition(
                buffer, pos);
    }
}

template<typename Buffer>
AK_FORCE_INLINE int getChars(const Buffer buffer, const PtReadingUtils::NodeFlags flags, const int maxLength,
        const int *const codePointTable, int *const outBuffer, int *const pos) {
    int length = 0;
    if (PtReadingUtils::hasMultipleChars(flags)) {
        length = ByteArrayUtils::readStringAndAdvancePosition(buffer, maxLength, codePointTable,
                outBuffer, pos);
    } else {
        const int codePoint = ByteArrayUtils::readCodePointAndAdvancePosition(buffer,
                codePointTable, pos);
        if (codePoint == NOT_A_CODE_POINT) {
            // CAVEAT: codePoint == NOT_A_CODE_POINT means the code point is
            // CHARACTER_ARRAY_TERMINATOR. The code point must not be CHARACTER_ARRAY_TERMINATOR
            // when the PtNode has a single code point.
            length = 0;
            AKLOGE("codePoint is NOT_A_CODE_POINT. pos: %d", *pos - 1);
            ASSERT(false);
        } else if (maxLength > 0) {
            outBuffer[0] = codePoint;
            length = 1;
        }
    }
    return length;
}

} // namespace

template<typename Buffer>
/* static */ AK_FORCE_INLINE int PtReadingUtils::readChildrenPosition(const Buffer buffer,
        const NodeFlags flags, int *const pos) {
    const int base = *pos;
    int offset = 0;
    switch (MASK_CHILDREN_POSITION_TYPE & flags) {
        case FLAG_CHILDREN_POSITION_TYPE_ONEBYTE:
            offset = ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
            break;
        case FLAG_CHILDREN_POSITION_TYPE_TWOBYTES:
            offset = ByteArrayUtils::readUint16AndAdvancePosition(buffer, pos);
            break;
        case FLAG_CHILDREN_POSITION_TYPE_THREEBYTES:
            offset = ByteArrayUtils::readUint24AndAdvancePosition(buffer, pos);
            break;
        default:
            // If we come here, it means we asked for the children of a word with
            // no children.
            return NOT_A_DICT_POS;
    }
    return base + offset;
}

/* static */ int PtReadingUtils::getPtNodeArraySizeAndAdvancePosition(
        const uint8_t *const buffer, int *const pos) {
    return getPtNodeArraySize(buffer, pos);
}

/* static */ int PtReadingUtils::getPtNodeArraySizeAndAdvancePosition(
        const ReadOnlyByteArrayView buffer, int *const pos) {
    return getPtNodeArraySize(buffer, pos);
}

/* static */ PtReadingUtils::NodeFlags PtReadingUtils::getFlagsAndAdvancePosition(
        const uint8_t *const buffer, int *const pos) {
    return ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
}

/* static */ int PtReadingUtils::getCharsAndAdvancePosition(const uint8_t *const buffer,
        const NodeFlags flags, const int maxLength, const int *const codePointTable,
        int *const outBuffer, int *const pos) {
    return getChars(buffer, flags, maxLength, codePointTable, outBuffer, pos);
}

/* static */ int PtReadingUtils::getCharsAndAdvancePosition(const ReadOnlyByteArrayView buffer,
        const NodeFlags flags, const int maxLength, const int *const codePointTable,
        int *const outBuffer, int *const pos) {
    if (!hasMultipleChars(flags)) {
        return getChars(buffer, flags, maxLength, codePointTable, outBuffer, pos);
    }
    bool isTooLong = false;
    const int length = ByteArrayUtils::readStringAndAdvancePosition(buffer, maxLength,
            codePointTable, outBuffer, pos, &isTooLong);
    if (isTooLong) {
        // A PtNode with more code points than maxLength is malformed; do not cut it.
        ByteArrayUtils::movePastEnd(buffer, pos);
    }
    return length;
}

/* static */ int PtReadingUtils::readChildrenPositionAndAdvancePosition(
        const uint8_t *const buffer, const NodeFlags flags, int *const pos) {
    return readChildrenPosition(buffer, flags, pos);
}

/* static */ void PtReadingUtils::readPtNodeInfo(const ReadOnlyByteArrayView dictBuf,
        const int ptNodePos,
        const DictionaryShortcutsStructurePolicy *const shortcutPolicy,
        const DictionaryBigramsStructurePolicy *const bigramPolicy, const int *const codePointTable,
        NodeFlags *const outFlags, int *const outCodePointCount, int *const outCodePoint,
        int *const outProbability, int *const outChildrenPos, int *const outShortcutPos,
        int *const outBigramPos, int *const outSiblingPos) {
    int readingPos = ptNodePos;
    NodeFlags flags;
    if (ByteArrayUtils::isInBounds(dictBuf, ptNodePos, MAX_PT_NODE_FIXED_PART_SIZE)) {
        // Fast path (all but the last PtNodes of a file): the flags, characters, probability and
        // children position of the largest valid PtNode are inside the buffer, so they are read
        // without a check each. Multiple characters still go through the bounded reader, which
        // reports a PtNode longer than MAX_WORD_LENGTH.
        const uint8_t *const buffer = dictBuf.data();
        flags = getFlagsAndAdvancePosition(buffer, &readingPos);
        if (hasMultipleChars(flags)) {
            bool isTooLong = false;
            *outCodePointCount = ByteArrayUtils::readStringAndAdvancePosition(buffer,
                    MAX_WORD_LENGTH, codePointTable, outCodePoint, &readingPos, &isTooLong);
            if (isTooLong) {
                // A PtNode with more code points than MAX_WORD_LENGTH is malformed.
                ByteArrayUtils::movePastEnd(dictBuf, &readingPos);
            } else {
                ByteArrayUtils::checkCodePointTableEntry(dictBuf, codePointTable,
                        NOT_A_CODE_POINT, &readingPos);
            }
            if (ByteArrayUtils::isPastEnd(dictBuf, readingPos)) {
                // Malformed characters: nothing more is read (the position is not valid).
                *outFlags = flags;
                *outProbability = NOT_A_PROBABILITY;
                *outChildrenPos = NOT_A_DICT_POS;
                *outShortcutPos = NOT_A_DICT_POS;
                *outBigramPos = NOT_A_DICT_POS;
                *outSiblingPos = readingPos;
                return;
            }
        } else {
            *outCodePointCount = getCharsAndAdvancePosition(buffer, flags, MAX_WORD_LENGTH,
                    codePointTable, outCodePoint, &readingPos);
        }
        *outProbability = isTerminal(flags) ?
                ByteArrayUtils::readUint8AndAdvancePosition(buffer, &readingPos)
                : NOT_A_PROBABILITY;
        *outChildrenPos = hasChildrenInFlags(flags) ?
                readChildrenPositionAndAdvancePosition(buffer, flags, &readingPos)
                : NOT_A_DICT_POS;
    } else {
        flags = getFlagsAndAdvancePosition(dictBuf, &readingPos);
        *outCodePointCount = getCharsAndAdvancePosition(
                dictBuf, flags, MAX_WORD_LENGTH, codePointTable, outCodePoint, &readingPos);
        *outProbability = isTerminal(flags) ?
                readProbabilityAndAdvancePosition(dictBuf, &readingPos) : NOT_A_PROBABILITY;
        *outChildrenPos = hasChildrenInFlags(flags) ?
                readChildrenPositionAndAdvancePosition(dictBuf, flags, &readingPos)
                : NOT_A_DICT_POS;
    }
    *outFlags = flags;
    *outShortcutPos = NOT_A_DICT_POS;
    if (hasShortcutTargets(flags)) {
        *outShortcutPos = readingPos;
        shortcutPolicy->skipAllShortcuts(&readingPos);
    }
    *outBigramPos = NOT_A_DICT_POS;
    if (hasBigrams(flags)) {
        *outBigramPos = readingPos;
        if (!bigramPolicy->skipAllBigrams(&readingPos)) {
            // The bigram list runs past the end of the buffer.
            ByteArrayUtils::movePastEnd(dictBuf, &readingPos);
        }
    }
    *outSiblingPos = readingPos;
}

} // namespace latinime
