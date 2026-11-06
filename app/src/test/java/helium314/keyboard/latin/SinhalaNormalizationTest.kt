// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard.latin

import helium314.keyboard.latin.common.combiningRange
import java.io.File
import java.text.Normalizer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

/** Shows Sinhala keystroke sequences that differ from their NFC form. Does not change input. */
class SinhalaNormalizationTest {
    @Test fun keystrokeSequenceDiffersFromNfc() {
        val typedEe = "\u0DD9\u0DCA" // layout keys ෙ then ්
        val composedEe = "\u0DDA" // ේ
        assertNotEquals(typedEe, composedEe)
        assertEquals(composedEe, Normalizer.normalize(typedEe, Normalizer.Form.NFC))

        val typedOo = "\u0DDC\u0DCA" // ො then ්
        val composedOo = "\u0DDD" // ෝ
        assertNotEquals(typedOo, composedOo)
        assertEquals(composedOo, Normalizer.normalize(typedOo, Normalizer.Form.NFC))

        // අපේ from sinhala_full_word_list_2010-09-10.txt. NFC is අපේ, which is also in the list.
        val sourceWord = "\u0D85\u0DB4\u0DD9\u0DCA"
        assertEquals("අපේ", Normalizer.normalize(sourceWord, Normalizer.Form.NFC))
        assertNotEquals(sourceWord, Normalizer.normalize(sourceWord, Normalizer.Form.NFC))
    }

    @Test fun sinhalaMarksAreOutsideTheLatinDeadKeyRange() {
        assertFalse(0x0DCA in combiningRange)
        assertFalse(0x0DD9 in combiningRange)
        assertTrue(0x0301 in combiningRange)
    }

    @Test fun combinedFileKeepsOriginalNonNfcSpellings() {
        val directory = listOf(
            File("../third_party/sinhala-dictionary"),
            File("third_party/sinhala-dictionary"),
        ).first { it.isDirectory }
        val originalLines = directory.resolve("sinhala_full_word_list_2010-09-10.txt")
            .readText()
            .split(Regex("\r?\n"))
            .filter { it.isNotEmpty() }
        assertEquals(83061, originalLines.size)
        val present = originalLines.toSet()
        val nonNfc = originalLines.filter { Normalizer.normalize(it, Normalizer.Form.NFC) != it }
        assertEquals(39, nonNfc.size)
        val withNfcTwin = nonNfc.filter { Normalizer.normalize(it, Normalizer.Form.NFC) in present }
        assertEquals(34, withNfcTwin.size)

        val combined = directory.resolve("main_si.combined").readText()
        assertTrue(combined.startsWith("dictionary=main:si,locale=si,"))
        nonNfc.forEach { word ->
            assertTrue(combined.contains(" word=$word,f=0\n"), word)
        }
        val onlyDecomposed = listOf("නේවිපිල්ලේ", "වින්ඳේ", "සැසඳේ", "හැටහතේ", "හේදෙන්න")
        onlyDecomposed.forEach { word ->
            val nfc = Normalizer.normalize(word, Normalizer.Form.NFC)
            assertNotEquals(word, nfc)
            assertFalse(present.contains(nfc), nfc)
            assertFalse(combined.contains(" word=$nfc,f=0\n"), nfc)
        }
    }
}
