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

#ifndef LATINIME_PATRICIA_TRIE_READING_UTILS_H
#define LATINIME_PATRICIA_TRIE_READING_UTILS_H

#include <cstdint>

#include "defines.h"
#include "dictionary/utils/byte_array_utils.h"
#include "utils/byte_array_view.h"

namespace latinime {

class DictionaryShortcutsStructurePolicy;
class DictionaryBigramsStructurePolicy;

class PatriciaTrieReadingUtils {
 public:
    typedef uint8_t NodeFlags;

    // Each reading method has two forms. The ReadOnlyByteArrayView form (read-only v2 dictionaries)
    // never reads outside the buffer, see "Bounded reading" in ByteArrayUtils. The raw pointer form
    // is used by the v4 dictionaries, which check their buffers themselves.
    static int getPtNodeArraySizeAndAdvancePosition(const uint8_t *const buffer, int *const pos);
    static int getPtNodeArraySizeAndAdvancePosition(const ReadOnlyByteArrayView buffer,
            int *const pos);

    static NodeFlags getFlagsAndAdvancePosition(const uint8_t *const buffer, int *const pos);
    static AK_FORCE_INLINE NodeFlags getFlagsAndAdvancePosition(
            const ReadOnlyByteArrayView buffer, int *const pos) {
        return ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
    }

    static AK_FORCE_INLINE int getCodePointAndAdvancePosition(const ReadOnlyByteArrayView buffer,
            const int *const codePointTable, int *const pos) {
        return ByteArrayUtils::readCodePointAndAdvancePosition(buffer, codePointTable, pos);
    }

    // Returns the number of read characters.
    static int getCharsAndAdvancePosition(const uint8_t *const buffer, const NodeFlags flags,
            const int maxLength, const int *const codePointTable, int *const outBuffer,
            int *const pos);
    static int getCharsAndAdvancePosition(const ReadOnlyByteArrayView buffer,
            const NodeFlags flags, const int maxLength, const int *const codePointTable,
            int *const outBuffer, int *const pos);

    // Returns the number of skipped characters.
    static AK_FORCE_INLINE int skipCharacters(const ReadOnlyByteArrayView buffer,
            const NodeFlags flags, const int maxLength, const int *const codePointTable,
            int *const pos) {
        if (hasMultipleChars(flags)) {
            bool isTooLong = false;
            const int length = ByteArrayUtils::advancePositionToBehindString(buffer, maxLength,
                    codePointTable, pos, &isTooLong);
            if (isTooLong) {
                // A PtNode with more code points than maxLength is malformed.
                ByteArrayUtils::movePastEnd(buffer, pos);
            }
            return length;
        }
        if (maxLength > 0) {
            getCodePointAndAdvancePosition(buffer, codePointTable, pos);
            return 1;
        }
        return 0;
    }

    static AK_FORCE_INLINE int readProbabilityAndAdvancePosition(
            const ReadOnlyByteArrayView buffer, int *const pos) {
        return ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
    }

    static int readChildrenPositionAndAdvancePosition(const uint8_t *const buffer,
            const NodeFlags flags, int *const pos);
    static AK_FORCE_INLINE int readChildrenPositionAndAdvancePosition(
            const ReadOnlyByteArrayView buffer, const NodeFlags flags, int *const pos) {
        const int base = *pos;
        int offset = 0;
        const NodeFlags positionType = MASK_CHILDREN_POSITION_TYPE & flags;
        if (positionType == FLAG_CHILDREN_POSITION_TYPE_ONEBYTE) {
            offset = ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
        } else if (positionType == FLAG_CHILDREN_POSITION_TYPE_TWOBYTES) {
            offset = ByteArrayUtils::readUint16AndAdvancePosition(buffer, pos);
        } else if (positionType == FLAG_CHILDREN_POSITION_TYPE_THREEBYTES) {
            offset = ByteArrayUtils::readUint24AndAdvancePosition(buffer, pos);
        } else {
            // No children.
            return NOT_A_DICT_POS;
        }
        // A truncated offset must not produce a position that looks valid.
        return ByteArrayUtils::isPastEnd(buffer, *pos) ? NOT_A_DICT_POS : base + offset;
    }

    /**
     * Node Flags
     */
    static AK_FORCE_INLINE bool isPossiblyOffensive(const NodeFlags flags) {
        return (flags & FLAG_IS_POSSIBLY_OFFENSIVE) != 0;
    }

    static AK_FORCE_INLINE bool isNotAWord(const NodeFlags flags) {
        return (flags & FLAG_IS_NOT_A_WORD) != 0;
    }

    static AK_FORCE_INLINE bool isTerminal(const NodeFlags flags) {
        return (flags & FLAG_IS_TERMINAL) != 0;
    }

    static AK_FORCE_INLINE bool hasShortcutTargets(const NodeFlags flags) {
        return (flags & FLAG_HAS_SHORTCUT_TARGETS) != 0;
    }

    static AK_FORCE_INLINE bool hasBigrams(const NodeFlags flags) {
        return (flags & FLAG_HAS_BIGRAMS) != 0;
    }

    static AK_FORCE_INLINE bool hasMultipleChars(const NodeFlags flags) {
        return (flags & FLAG_HAS_MULTIPLE_CHARS) != 0;
    }

    static AK_FORCE_INLINE bool hasChildrenInFlags(const NodeFlags flags) {
        return FLAG_CHILDREN_POSITION_TYPE_NOPOSITION != (MASK_CHILDREN_POSITION_TYPE & flags);
    }

    static AK_FORCE_INLINE NodeFlags createAndGetFlags(const bool isPossiblyOffensive,
            const bool isNotAWord, const bool isTerminal, const bool hasShortcutTargets,
            const bool hasBigrams, const bool hasMultipleChars,
            const int childrenPositionFieldSize) {
        NodeFlags nodeFlags = 0;
        nodeFlags = isPossiblyOffensive ? (nodeFlags | FLAG_IS_POSSIBLY_OFFENSIVE) : nodeFlags;
        nodeFlags = isNotAWord ? (nodeFlags | FLAG_IS_NOT_A_WORD) : nodeFlags;
        nodeFlags = isTerminal ? (nodeFlags | FLAG_IS_TERMINAL) : nodeFlags;
        nodeFlags = hasShortcutTargets ? (nodeFlags | FLAG_HAS_SHORTCUT_TARGETS) : nodeFlags;
        nodeFlags = hasBigrams ? (nodeFlags | FLAG_HAS_BIGRAMS) : nodeFlags;
        nodeFlags = hasMultipleChars ? (nodeFlags | FLAG_HAS_MULTIPLE_CHARS) : nodeFlags;
        if (childrenPositionFieldSize == 1) {
            nodeFlags |= FLAG_CHILDREN_POSITION_TYPE_ONEBYTE;
        } else if (childrenPositionFieldSize == 2) {
            nodeFlags |= FLAG_CHILDREN_POSITION_TYPE_TWOBYTES;
        } else if (childrenPositionFieldSize == 3) {
            nodeFlags |= FLAG_CHILDREN_POSITION_TYPE_THREEBYTES;
        } else {
            nodeFlags |= FLAG_CHILDREN_POSITION_TYPE_NOPOSITION;
        }
        return nodeFlags;
    }

    // Reads the PtNode at ptNodePos. If the PtNode does not fit in the buffer (or is otherwise
    // malformed), outSiblingPos is past the end of the buffer (ByteArrayUtils::isPastEnd) and the
    // other values must not be used.
    static void readPtNodeInfo(const ReadOnlyByteArrayView dictBuf, const int ptNodePos,
            const DictionaryShortcutsStructurePolicy *const shortcutPolicy,
            const DictionaryBigramsStructurePolicy *const bigramPolicy,
            const int *const codePointTable, NodeFlags *const outFlags,
            int *const outCodePointCount, int *const outCodePoint, int *const outProbability,
            int *const outChildrenPos, int *const outShortcutPos, int *const outBigramPos,
            int *const outSiblingPos);

 private:
    template<typename Buffer>
    static AK_FORCE_INLINE int readChildrenPosition(const Buffer buffer, const NodeFlags flags, int *const pos);

    DISALLOW_IMPLICIT_CONSTRUCTORS(PatriciaTrieReadingUtils);

    static const NodeFlags MASK_CHILDREN_POSITION_TYPE;
    static const NodeFlags FLAG_CHILDREN_POSITION_TYPE_NOPOSITION;
    static const NodeFlags FLAG_CHILDREN_POSITION_TYPE_ONEBYTE;
    static const NodeFlags FLAG_CHILDREN_POSITION_TYPE_TWOBYTES;
    static const NodeFlags FLAG_CHILDREN_POSITION_TYPE_THREEBYTES;

    static const NodeFlags FLAG_HAS_MULTIPLE_CHARS;
    static const NodeFlags FLAG_IS_TERMINAL;
    static const NodeFlags FLAG_HAS_SHORTCUT_TARGETS;
    static const NodeFlags FLAG_HAS_BIGRAMS;
    static const NodeFlags FLAG_IS_NOT_A_WORD;
    static const NodeFlags FLAG_IS_POSSIBLY_OFFENSIVE;
};
} // namespace latinime
#endif /* LATINIME_PATRICIA_TRIE_NODE_READING_UTILS_H */
