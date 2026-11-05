// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard.latin

import helium314.keyboard.latin.SuggestedWords.SuggestedWordInfo
import helium314.keyboard.latin.common.ComposedData
import helium314.keyboard.latin.common.Constants
import helium314.keyboard.latin.common.InputPointers
import java.text.Normalizer
import java.util.ArrayList
import java.util.LinkedHashMap

/**
 * Sinhala-only dictionary lookup helper.
 *
 * The Sinhala layout types vowel signs as separate code points. NFC composes
 * U+0DD9 U+0DCA into U+0DDA (ේ) and U+0DDC U+0DCA into U+0DDD (ෝ). The bundled
 * word list stores the composed forms. This helper asks the dictionary for the
 * NFC form as a second exact query. It does not change Latin, English, or Korean input.
 */
object SinhalaLookup {
    private const val SINHALA_START = 0x0D80
    private const val SINHALA_END = 0x0DFF

    /**
     * User-dictionary acceptance used by [helium314.keyboard.latin.dictionary.DictionaryInfoUtils].
     * Sinhala al-lakuna and ZWJ/ZWNJ are accepted only when the word already contains Sinhala.
     * Digits still cannot make up the whole word, and they are not word characters.
     */
    @JvmStatic
    fun acceptedForUserDictionary(
        text: CharSequence?,
        maxLength: Int,
        isWordCodePoint: (Int) -> Boolean
    ): Boolean {
        if (text.isNullOrEmpty() || text.length > maxLength) return false
        var digitCount = 0
        val containsSinhala = containsSinhala(text)
        var index = 0
        while (index < text.length) {
            val codePoint = Character.codePointAt(text, index)
            val charCount = Character.charCount(codePoint)
            if (Character.isDigit(codePoint)) {
                digitCount += charCount
            } else if (!isWordCodePoint(codePoint) && !isSinhalaOnlyCodePoint(codePoint, containsSinhala)) {
                return false
            }
            index += charCount
        }
        // Reject strings made only of digits. A PIN or card number must not be learned.
        return digitCount < text.length
    }

    @JvmStatic
    fun isSinhalaOnlyCodePoint(codePoint: Int, containsSinhala: Boolean): Boolean {
        if (!containsSinhala) return false
        return codePoint in SINHALA_START..SINHALA_END || codePoint == 0x200C || codePoint == 0x200D
    }

    @JvmStatic
    fun containsSinhala(text: CharSequence): Boolean {
        var index = 0
        while (index < text.length) {
            val codePoint = Character.codePointAt(text, index)
            if (codePoint in SINHALA_START..SINHALA_END) return true
            index += Character.charCount(codePoint)
        }
        return false
    }

    /**
     * NFC form of a Sinhala word, or null when the word has no Sinhala character
     * or is already in NFC. A null result means the caller must not run a second query.
     */
    @JvmStatic
    fun nfcFormIfDifferent(word: String?): String? {
        if (word.isNullOrEmpty() || !containsSinhala(word)) return null
        val normalized = Normalizer.normalize(word, Normalizer.Form.NFC)
        return if (normalized == word) null else normalized
    }

    /**
     * Composed data for an exact dictionary query. Coordinates are
     * [Constants.NOT_A_COORDINATE] so proximity correction is not applied to the
     * normalized spelling.
     */
    @JvmStatic
    fun exactLookupData(word: String): ComposedData {
        val codePointCount = Character.codePointCount(word, 0, word.length).coerceAtLeast(1)
        val pointers = InputPointers(codePointCount)
        repeat(codePointCount) {
            pointers.addPointer(Constants.NOT_A_COORDINATE, Constants.NOT_A_COORDINATE, 0, 0)
        }
        return ComposedData(pointers, false, word)
    }

    /**
     * Merge a typed-form suggestion list with the NFC-form list. Equal scores keep
     * the NFC spelling, so ෙ + ් and ේ do not both appear.
     */
    @JvmStatic
    fun mergeSuggestions(
        primary: ArrayList<SuggestedWordInfo>?,
        normalized: ArrayList<SuggestedWordInfo>?
    ): ArrayList<SuggestedWordInfo>? {
        if (normalized.isNullOrEmpty()) return primary
        if (primary.isNullOrEmpty()) return normalized
        val merged = LinkedHashMap<String, SuggestedWordInfo>()
        for (info in primary) {
            merged[dedupeKey(info.mWord)] = info
        }
        for (info in normalized) {
            val key = dedupeKey(info.mWord)
            val existing = merged[key]
            if (existing == null || preferNormalized(existing, info)) {
                merged[key] = info
            }
        }
        return ArrayList(merged.values)
    }

    private fun preferNormalized(existing: SuggestedWordInfo, candidate: SuggestedWordInfo): Boolean {
        if (candidate.mScore != existing.mScore) return candidate.mScore > existing.mScore
        return isNfc(candidate.mWord) && !isNfc(existing.mWord)
    }

    private fun isNfc(word: String): Boolean =
        word == Normalizer.normalize(word, Normalizer.Form.NFC)

    private fun dedupeKey(word: String): String =
        Normalizer.normalize(word, Normalizer.Form.NFC)
}
