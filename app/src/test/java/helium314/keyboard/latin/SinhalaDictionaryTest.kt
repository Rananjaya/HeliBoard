// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard.latin

import helium314.keyboard.latin.SuggestedWords.SuggestedWordInfo
import helium314.keyboard.latin.common.Constants
import helium314.keyboard.latin.common.LocaleUtils
import helium314.keyboard.latin.define.DecoderSpecificConstants
import helium314.keyboard.latin.dictionary.DictionaryInfoUtils
import helium314.keyboard.latin.makedict.DictIO
import helium314.keyboard.latin.makedict.FusionDictionary
import java.io.File
import java.text.Normalizer
import java.util.Locale
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Checks the prepared Sinhala word list, asset discovery, and the Sinhala-only
 * lookup helper. Native binary lookup needs the Android dictionary library, so
 * these tests read the combined source that was compiled into main_si.dict.
 */
class SinhalaDictionaryTest {
    @Test fun assetNameSelectsSinhalaForSiLkAndLeavesEnglishAlone() {
        val assets = dictAssets().list()?.toList().orEmpty()
        assertTrue(assets.contains("main_si.dict"))
        assertTrue(assets.contains("main_en-US.dict"))
        assertTrue(assets.contains("emoji_en.dict"))
        assertTrue(assets.contains("emoji_si.dict"))
        assertFalse(assets.any { it.endsWith(".combined") })

        val locale = DictionaryInfoUtils.extractLocaleFromAssetsDictionaryFile("main_si.dict")
        assertEquals("si", locale.language)
        assertTrue(
            LocaleUtils.getMatchLevel(Locale.forLanguageTag("si-LK"), locale) >= LocaleUtils.LOCALE_GOOD_MATCH
        )
        val mainDictionaries = assets.filter { it.startsWith("main_") }
        assertEquals(
            "main_si.dict",
            LocaleUtils.getBestMatch(Locale.forLanguageTag("si-LK"), mainDictionaries) {
                DictionaryInfoUtils.extractLocaleFromAssetsDictionaryFile(it)
            }
        )
        assertEquals(
            "main_en-US.dict",
            LocaleUtils.getBestMatch(Locale.forLanguageTag("en-US"), mainDictionaries) {
                DictionaryInfoUtils.extractLocaleFromAssetsDictionaryFile(it)
            }
        )
    }

    @Test fun compiledHeaderKeepsSourceIdentity() {
        val bytes = compiledDictionary().readBytes()
        val header = bytes.decodeToString(0, minOf(bytes.size, 512))
        assertTrue(header.contains("main:si"))
        assertTrue(header.contains("1284076800"))
        assertTrue(header.contains("laknath/Sinhala-Dictionary commit cfcfb50"))
    }

    @Test fun wordListHasFrequencyZeroAndRejectsInvalidEntries() {
        val dictionary = loadCombined()
        assertEquals(83029, dictionary.count())
        assertTrue(dictionary.hasWord("අංකනය"))
        assertTrue(dictionary.hasWord("අංකනයක්"))
        assertTrue(dictionary.hasWord("අකර්මණ්‍ය"))
        assertTrue(dictionary.hasWord("අක්‍රමවත්"))
        assertFalse(dictionary.hasWord("අතර15වන"))
        assertFalse(dictionary.hasWord("අවක්ෂේපx"))
        assertFalse(dictionary.hasWord("hello"))

        val ankana = FusionDictionary.findWordInTree(dictionary.mRootNodeArray, "අංකනය")
        assertNotNull(ankana)
        assertEquals(0, ankana.probability)

        var frequencyNotZero = 0
        var prefixAnka = 0
        var prefixAnkana = 0
        var yansaya = 0
        var rakaransaya = 0
        for (word in dictionary) {
            if (word.mProbabilityInfo.mProbability != 0) frequencyNotZero++
            val text = word.mWord
            if (text.startsWith("අංක")) prefixAnka++
            if (text.startsWith("අංකන")) prefixAnkana++
            if (text.contains("\u0DCA\u200D\u0DBA")) yansaya++
            if (text.contains("\u0DCA\u200D\u0DBB")) rakaransaya++
        }
        assertEquals(0, frequencyNotZero)
        assertTrue(prefixAnka > prefixAnkana)
        assertTrue(prefixAnkana > 1)
        assertEquals(1626, yansaya)
        assertEquals(3927, rakaransaya)
    }

    @Test fun decomposedVowelSignsAreNotStoredButNfcFormIs() {
        val dictionary = loadCombined()
        val stored = "බිඳීමේ"
        val typed = stored.replace("\u0DDA", "\u0DD9\u0DCA")
        assertTrue(dictionary.hasWord(stored))
        assertFalse(dictionary.hasWord(typed))
        assertEquals(stored, SinhalaLookup.nfcFormIfDifferent(typed))
        assertNull(SinhalaLookup.nfcFormIfDifferent(stored))
        assertNull(SinhalaLookup.nfcFormIfDifferent("අකර්මණ්‍ය"))
    }

    @Test fun sinhalaLookupDoesNotNormalizeEnglishOrKorean() {
        assertNull(SinhalaLookup.nfcFormIfDifferent("the"))
        assertNull(SinhalaLookup.nfcFormIfDifferent("e\u0301"))
        val decomposedHangul = "\u1100\u1161"
        assertEquals("가", Normalizer.normalize(decomposedHangul, Normalizer.Form.NFC))
        assertNull(SinhalaLookup.nfcFormIfDifferent(decomposedHangul))
    }

    @Test fun equalScoresPreferTheNfcSpelling() {
        val decomposed = suggestion("\u0D85\u0DB4\u0DD9\u0DCA", 0)
        val composed = suggestion("අපේ", 0)
        val merged = SinhalaLookup.mergeSuggestions(
            arrayListOf(decomposed),
            arrayListOf(composed)
        )
        assertNotNull(merged)
        assertEquals(1, merged.size)
        assertEquals("අපේ", merged[0].mWord)
    }

    @Test fun normalizedQueryUsesUnknownCoordinates() {
        val data = SinhalaLookup.exactLookupData("\u0DD9\u0DCA")
        assertFalse(data.mIsBatchMode)
        assertEquals(2, data.mInputPointers.pointerSize)
        assertEquals(Constants.NOT_A_COORDINATE, data.mInputPointers.xCoordinates[0])
        assertEquals(Constants.NOT_A_COORDINATE, data.mInputPointers.yCoordinates[1])
    }

    @Test fun cachedEmojiDictionaryNameIsRecognized() {
        assertEquals("emoji", DictionaryInfoUtils.dictionaryTypeFromCachedFileName("emoji.dict"))
        assertEquals("emoji", DictionaryInfoUtils.dictionaryTypeFromCachedFileName("emoji_user.dict"))
        assertEquals("main", DictionaryInfoUtils.dictionaryTypeFromCachedFileName("main.dict"))
    }

    @Test fun learnedWordsAcceptSinhalaMarksWithoutAcceptingOtherJoiners() {
        val connectors = spacingConfig().readText()
        assertTrue(connectors.contains("<string name=\"symbols_word_connectors\">\\'-</string>"))
        assertFalse(Character.isLetter(0x0DCA))
        assertFalse(Character.getType(0x0DCA) == Character.COMBINING_SPACING_MARK.toInt())
        assertFalse(defaultIsWordCodePoint(0x0DCA))
        assertFalse(defaultIsWordCodePoint(0x200D))
        assertTrue(accepts("අංකනයක්"))
        assertTrue(accepts("අකර්මණ්‍ය"))
        assertTrue(accepts("hello"))
        assertFalse(accepts("hello\u200D"))
        assertFalse(accepts("\u200D"))
        assertFalse(accepts("hello!"))
        // Digits are allowed inside a word and rejected only when the whole word is digits.
        assertTrue(accepts("hello2"))
        assertTrue(accepts("අතර15වන"))
        assertFalse(accepts("12345"))
    }

    private fun accepts(text: String) = SinhalaLookup.acceptedForUserDictionary(
        text,
        DecoderSpecificConstants.DICTIONARY_MAX_WORD_LENGTH,
        ::defaultIsWordCodePoint
    )

    /** Same rule as SpacingAndPunctuations.isWordCodePoint for the default connector string. */
    private fun defaultIsWordCodePoint(code: Int): Boolean =
        Character.isLetter(code) || code == '\''.code || code == '-'.code
            || Character.getType(code) == Character.COMBINING_SPACING_MARK.toInt()

    private fun suggestion(word: String, score: Int) =
        SuggestedWordInfo(word, "", score, 0, null, 0, 0)

    private fun loadCombined() = DictIO.readCombinedDictionary(combinedFile().path)

    private fun combinedFile(): File {
        val candidates = listOf(
            File("third_party/sinhala-dictionary/main_si.combined"),
            File("../third_party/sinhala-dictionary/main_si.combined")
        )
        return candidates.first { it.isFile }
    }

    private fun compiledDictionary(): File {
        val candidates = listOf(
            File("src/main/assets/dicts/main_si.dict"),
            File("app/src/main/assets/dicts/main_si.dict")
        )
        return candidates.first { it.isFile }
    }

    private fun dictAssets(): File {
        val candidates = listOf(
            File("src/main/assets/dicts"),
            File("app/src/main/assets/dicts")
        )
        return candidates.first { it.isDirectory }
    }

    private fun spacingConfig(): File {
        val candidates = listOf(
            File("src/main/res/values/donottranslate-config-spacing-and-punctuations.xml"),
            File("app/src/main/res/values/donottranslate-config-spacing-and-punctuations.xml")
        )
        return candidates.first { it.isFile }
    }
}
