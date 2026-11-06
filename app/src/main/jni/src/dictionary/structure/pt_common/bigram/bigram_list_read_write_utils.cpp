/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "dictionary/structure/pt_common/bigram/bigram_list_read_write_utils.h"

#include "dictionary/utils/byte_array_utils.h"
#include "dictionary/utils/buffer_with_extendable_buffer.h"

namespace latinime {

const BigramListReadWriteUtils::BigramFlags BigramListReadWriteUtils::MASK_ATTRIBUTE_ADDRESS_TYPE =
        0x30;
const BigramListReadWriteUtils::BigramFlags
        BigramListReadWriteUtils::FLAG_ATTRIBUTE_ADDRESS_TYPE_ONEBYTE = 0x10;
const BigramListReadWriteUtils::BigramFlags
        BigramListReadWriteUtils::FLAG_ATTRIBUTE_ADDRESS_TYPE_TWOBYTES = 0x20;
const BigramListReadWriteUtils::BigramFlags
        BigramListReadWriteUtils::FLAG_ATTRIBUTE_ADDRESS_TYPE_THREEBYTES = 0x30;
const BigramListReadWriteUtils::BigramFlags
        BigramListReadWriteUtils::FLAG_ATTRIBUTE_OFFSET_NEGATIVE = 0x40;
// Flag for presence of more attributes
const BigramListReadWriteUtils::BigramFlags BigramListReadWriteUtils::FLAG_ATTRIBUTE_HAS_NEXT =
        0x80;
// Mask for attribute probability, stored on 4 bits inside the flags byte.
const BigramListReadWriteUtils::BigramFlags
        BigramListReadWriteUtils::MASK_ATTRIBUTE_PROBABILITY = 0x0F;

/* static */ bool BigramListReadWriteUtils::getBigramEntryPropertiesAndAdvancePosition(
        const ReadOnlyByteArrayView buffer, BigramFlags *const outBigramFlags,
        int *const outTargetPtNodePos, int *const bigramEntryPos) {
    if (static_cast<int>(buffer.size()) <= *bigramEntryPos) {
        AKLOGE("Read invalid pos in getBigramEntryPropertiesAndAdvancePosition(). bufSize: %zd, "
                "bigramEntryPos: %d.", buffer.size(), *bigramEntryPos);
        return false;
    }
    const BigramFlags bigramFlags = ByteArrayUtils::readUint8AndAdvancePosition(buffer,
            bigramEntryPos);
    if (outBigramFlags) {
        *outBigramFlags = bigramFlags;
    }
    const int targetPos = getBigramAddressAndAdvancePosition(buffer, bigramFlags, bigramEntryPos);
    if (outTargetPtNodePos) {
        *outTargetPtNodePos = targetPos;
    }
    return true;
}

/* static */ bool BigramListReadWriteUtils::skipExistingBigrams(const ReadOnlyByteArrayView buffer,
        int *const bigramListPos) {
    // Only the size of each entry is needed: flags and an address of 0-3 bytes (see
    // getBigramAddressAndAdvancePosition). Each whole entry must be inside the buffer.
    BigramFlags flags;
    do {
        if (!ByteArrayUtils::isInBounds(buffer, *bigramListPos, 1)) {
            AKLOGE("Read invalid pos in skipExistingBigrams(). bufSize: %zd, bigramListPos: %d.",
                    buffer.size(), *bigramListPos);
            return false;
        }
        flags = buffer.data()[*bigramListPos];
        int addressSize = 0;
        switch (MASK_ATTRIBUTE_ADDRESS_TYPE & flags) {
            case FLAG_ATTRIBUTE_ADDRESS_TYPE_ONEBYTE:
                addressSize = 1;
                break;
            case FLAG_ATTRIBUTE_ADDRESS_TYPE_TWOBYTES:
                addressSize = 2;
                break;
            case FLAG_ATTRIBUTE_ADDRESS_TYPE_THREEBYTES:
                addressSize = 3;
                break;
        }
        if (!ByteArrayUtils::isInBounds(buffer, *bigramListPos, 1 + addressSize)) {
            ByteArrayUtils::movePastEnd(buffer, bigramListPos);
            return false;
        }
        *bigramListPos += 1 + addressSize;
    } while (hasNext(flags));
    return true;
}

/* static */ int BigramListReadWriteUtils::getBigramAddressAndAdvancePosition(
        const ReadOnlyByteArrayView buffer, const BigramFlags flags, int *const pos) {
    int offset = 0;
    const int origin = *pos;
    switch (MASK_ATTRIBUTE_ADDRESS_TYPE & flags) {
        case FLAG_ATTRIBUTE_ADDRESS_TYPE_ONEBYTE:
            offset = ByteArrayUtils::readUint8AndAdvancePosition(buffer, pos);
            break;
        case FLAG_ATTRIBUTE_ADDRESS_TYPE_TWOBYTES:
            offset = ByteArrayUtils::readUint16AndAdvancePosition(buffer, pos);
            break;
        case FLAG_ATTRIBUTE_ADDRESS_TYPE_THREEBYTES:
            offset = ByteArrayUtils::readUint24AndAdvancePosition(buffer, pos);
            break;
    }
    if (ByteArrayUtils::isPastEnd(buffer, *pos)) {
        // A truncated offset must not produce a position that looks valid.
        return NOT_A_DICT_POS;
    }
    if (isOffsetNegative(flags)) {
        return origin - offset;
    } else {
        return origin + offset;
    }
}

} // namespace latinime
