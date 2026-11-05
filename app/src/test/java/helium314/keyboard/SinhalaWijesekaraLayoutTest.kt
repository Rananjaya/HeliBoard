// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard

import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodSubtype
import helium314.keyboard.keyboard.KeyboardElement
import helium314.keyboard.keyboard.KeyboardId
import helium314.keyboard.keyboard.KeyboardLayoutSet
import helium314.keyboard.keyboard.Key.KeyParams
import helium314.keyboard.keyboard.internal.KeyboardBuilder
import helium314.keyboard.keyboard.internal.KeyboardParams
import helium314.keyboard.keyboard.internal.UniqueKeysCache
import helium314.keyboard.keyboard.internal.keyboard_parser.floris.KeyCode
import helium314.keyboard.latin.LatinIME
import helium314.keyboard.latin.RichInputMethodSubtype
import helium314.keyboard.latin.common.LocaleUtils.constructLocale
import helium314.keyboard.latin.utils.LayoutType
import helium314.keyboard.latin.utils.LayoutUtils
import helium314.keyboard.latin.utils.SubtypeSettings
import helium314.keyboard.latin.utils.SubtypeLocaleUtils.displayName
import helium314.keyboard.latin.utils.mainLayoutName
import org.json.JSONArray
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.security.MessageDigest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Sinhala (Wijesekara) layout: checks every key against the mapping approved in
 * docs/sinhala-wijesekara-layout.md, using the real keyboard builder (so locale popups from
 * locale_key_texts/si.txt are included, as on a device).
 */
@RunWith(RobolectricTestRunner::class)
@Config(shadows = [
    ShadowInputMethodManager2::class,
    ShadowProximityInfo::class,
])
class SinhalaWijesekaraLayoutTest {
    private val latinIME = Robolectric.setupService(LatinIME::class.java)
    private val sinhala = "si-LK".constructLocale()

    private val al = "්"
    private val zwj = "‍"
    private val zwnj = "‌"

    // Expected outputs per row, in key order (Q-row, A-row, Z-row of the Wijesekara standard).
    private val expectedNormal = listOf(
        listOf("ු", "අ", "ැ", "ර", "එ", "හ", "ම", "ස", "ද", "ච", "ඤ"),
        listOf(al, "ි", "ා", "ෙ", "ට", "ය", "ව", "න", "ක", "ත", al + zwj + "ර"),
        listOf("ං", "ජ", "ඩ", "ඉ", "බ", "ප", "ල", "ග", al + zwj),
    )
    private val expectedShifted = listOf(
        listOf("ූ", "උ", "ෑ", "ඍ", "ඔ", "ශ", "ඹ", "ෂ", "ධ", "ඡ", "ඥ"),
        listOf("ෟ", "ී", "ෘ", "ෆ", "ඨ", al + zwj + "ය", "ළු", "ණ", "ඛ", "ථ", "ර" + al + zwj),
        listOf("ඞ", "ඣ", "ඪ", "ඊ", "භ", "ඵ", "ළ", "ඝ", zwj),
    )
    // Long-press outputs that must be present (keyed by the key's output). Locale popups may add more.
    private val expectedPopupsNormal = mapOf(
        "අ" to listOf("ආ", "ඇ", "ඈ"),                    // අ: ආ ඇ ඈ
        "එ" to listOf("ඒ", "ඓ"),                              // එ: ඒ ඓ
        al to listOf("ෳ"),                                              // ්: ෳ
        "ා" to listOf("ෲ"),                                        // ා: ෲ
        "ෙ" to listOf("ේ", "ෛ", "ො", "ෝ", "ෞ"), // ෙ: ේ ෛ ො ෝ ෞ
        "ං" to listOf("ඃ"),                                        // ං: ඃ
        "ල" to listOf("ඏ", "ඐ"),                              // ල: ඏ ඐ
        al + zwj to listOf(zwnj),                                            // join: ZWNJ
        // from locale_key_texts/si.txt (shared with the existing Sinhala layout)
        "ද" to listOf("ඳ"),                                        // ද: ඳ
        "ජ" to listOf("ඦ"),                                        // ජ: ඦ
        "ඩ" to listOf("ඬ"),                                        // ඩ: ඬ
        "ග" to listOf("ඟ"),                                        // ග: ඟ
    )
    private val expectedPopupsShifted = mapOf(
        "උ" to listOf("ඌ"),                                        // උ: ඌ
        "ඔ" to listOf("ඕ", "ඖ"),                              // ඔ: ඕ ඖ
        "ඍ" to listOf("ඎ"),                                        // ඍ: ඎ
    )

    @Test fun `layout json is valid and has the approved shape`() {
        val json = JSONArray(layoutFile("sinhala_wijesekara").readText())
        assertEquals(listOf(11, 11, 9), (0 until json.length()).map { json.getJSONArray(it).length() })
        for (r in 0 until json.length()) {
            val row = json.getJSONArray(r)
            for (k in 0 until row.length()) {
                val key = row.getJSONObject(k)
                assertEquals("shift_state_selector", key.getString("$"), "row $r key $k")
                assertTrue(key.has("default") && key.has("manualOrLocked"), "row $r key $k")
            }
        }
    }

    @Test fun `normal layer matches the Wijesekara standard`() {
        assertEquals(expectedNormal, letterKeyOutputs(KeyboardElement.ALPHABET))
    }

    @Test fun `shift layer matches the Wijesekara standard`() {
        assertEquals(expectedShifted, letterKeyOutputs(KeyboardElement.ALPHABET_MANUAL_SHIFTED))
        assertEquals(expectedShifted, letterKeyOutputs(KeyboardElement.ALPHABET_SHIFT_LOCKED))
    }

    @Test fun `automatic shift does not change Sinhala keys`() {
        // Sinhala has no case, so auto-capitalisation at sentence start must not switch layers.
        assertEquals(expectedNormal, letterKeyOutputs(KeyboardElement.ALPHABET_AUTOMATIC_SHIFTED))
    }

    @Test fun `long press alternatives`() {
        assertPopups(KeyboardElement.ALPHABET, expectedPopupsNormal)
        assertPopups(KeyboardElement.ALPHABET_MANUAL_SHIFTED, expectedPopupsShifted)
    }

    @Test fun `independent vowels are single code points, never vowel-sign combinations`() {
        val independent = listOf("අ", "ආ", "ඇ", "ඈ", "ඉ", "ඊ", "උ", "ඌ", "ඍ", "ඎ", "ඏ", "ඐ", "එ", "ඒ", "ඓ", "ඔ", "ඕ", "ඖ")
        val reachable = allOutputs()
        for (vowel in independent) {
            assertEquals(1, vowel.codePointCount(0, vowel.length), vowel)
            assertTrue(Character.UnicodeBlock.of(vowel.codePointAt(0)) == Character.UnicodeBlock.SINHALA)
            assertTrue(vowel in reachable, "$vowel must be typeable directly")
        }
    }

    @Test fun `every output is Sinhala or a joiner, with no stray characters`() {
        // Main keys: only Sinhala and joiners.
        for (output in letterKeyOutputs(KeyboardElement.ALPHABET).flatten() +
                letterKeyOutputs(KeyboardElement.ALPHABET_MANUAL_SHIFTED).flatten()) {
            output.codePoints().forEach { cp ->
                assertTrue(cp in 0x0D80..0x0DFF || cp == 0x200C || cp == 0x200D,
                    "unexpected U+%04X in '%s'".format(cp, output))
            }
        }
        // Long-press: the app adds number-row and symbol popups to the top row of every layout.
        // Anything that is not Sinhala or a joiner must therefore also be on the existing Sinhala
        // layout, i.e. the new layout itself adds no foreign characters.
        fun isSinhalaOrJoiner(s: String) = s.codePoints().allMatch { it in 0x0D80..0x0DFF || it == 0x200C || it == 0x200D }
        val appAdded = allOutputs("sinhala").filterNot { isSinhalaOrJoiner(it) }.toSet()
        val foreign = allOutputs().filterNot { isSinhalaOrJoiner(it) }.toSet()
        assertTrue(appAdded.containsAll(foreign), "not from the app's own popups: ${foreign - appAdded}")
        // every Sinhala letter and sign that the standard places on the keyboard (incl. AltGr) is reachable
        val standardSet = (expectedNormal + expectedShifted).flatten() +
            expectedPopupsNormal.values.flatten() + expectedPopupsShifted.values.flatten()
        assertTrue(allOutputs().containsAll(standardSet))
    }

    @Test fun `layout is registered as a separate Sinhala subtype next to the existing one`() {
        val subtypes = sinhalaSubtypes()
        val layouts = subtypes.mapNotNull { it.mainLayoutName() }
        assertEquals(listOf("sinhala", "sinhala_wijesekara"), layouts)
        assertTrue("sinhala" in layouts, "$layouts")
        assertTrue("sinhala_wijesekara" in layouts, "$layouts")
        assertTrue(LayoutUtils.getAvailableLayouts(LayoutType.MAIN, latinIME, sinhala)
            .containsAll(listOf("sinhala", "sinhala_wijesekara")))
        val wijesekara = subtypes.single { it.mainLayoutName() == "sinhala_wijesekara" }
        assertTrue(wijesekara.displayName().endsWith("(Wijesekara)"), wijesekara.displayName())
        assertEquals(0x4ab35e8d, wijesekara.hashCode()) // hashCode is the declared subtypeId
    }

    @Test fun `existing Sinhala layout is unchanged`() {
        // sha256 of app/src/main/assets/layouts/main/sinhala.json as in upstream HeliBoard (commit 5ba40f78)
        val sha = MessageDigest.getInstance("SHA-256").digest(layoutFile("sinhala").readBytes())
            .joinToString("") { "%02x".format(it) }
        assertEquals(EXISTING_SINHALA_SHA256, sha)
        // and it still builds, with its own first keys
        val keys = letterKeyOutputs(KeyboardElement.ALPHABET, "sinhala")
        assertEquals(listOf(11, 11, 9), keys.map { it.size })
        assertEquals("ඏ", keys[1][10]) // existing layout keeps ඏ where Wijesekara has rakaransaya
    }

    private fun layoutFile(name: String): File =
        listOf(File("src/main/assets/layouts/main/$name.json"), File("app/src/main/assets/layouts/main/$name.json"))
            .first { it.exists() }

    // SubtypeSettings.init appends on every LatinIME created in a test run (once per process in the
    // app), so resource subtypes can repeat here; compare by subtype id.
    private fun sinhalaSubtypes(): List<InputMethodSubtype> =
        SubtypeSettings.getResourceSubtypesForLocale(sinhala).distinctBy { it.hashCode() }

    private fun subtype(layout: String): InputMethodSubtype =
        sinhalaSubtypes().single { it.mainLayoutName() == layout }

    private fun keyOutput(code: Int, outputText: String?): String =
        if (code == KeyCode.MULTIPLE_CODE_POINTS) outputText!! else String(Character.toChars(code))

    /** Outputs of the Sinhala letter keys, grouped by row; functional and bottom-row keys are left out. */
    private fun letterKeyOutputs(element: KeyboardElement, layout: String = "sinhala_wijesekara"): List<List<String>> =
        buildKeys(element, layout).take(3).map { row ->
            row.filter { isLetterKey(it) }.map { keyOutput(it.mCode, it.outputText) }
        }

    private fun isLetterKey(key: KeyParams): Boolean =
        key.mCode == KeyCode.MULTIPLE_CODE_POINTS || key.mCode in 0x0D80..0x0DFF || key.mCode == 0x200D

    private fun popupOutputs(key: KeyParams): List<String> =
        key.mPopupKeys.orEmpty().filter { it.mCode != KeyCode.UNSPECIFIED || it.mOutputText != null }
            .map { keyOutput(it.mCode, it.mOutputText) }

    private fun assertPopups(element: KeyboardElement, expected: Map<String, List<String>>) {
        val keys = buildKeys(element, "sinhala_wijesekara").take(3).flatten().filter { isLetterKey(it) }
        for ((keyOutputText, popups) in expected) {
            val key = keys.singleOrNull { keyOutput(it.mCode, it.outputText) == keyOutputText }
            assertNotNull(key, "key $keyOutputText")
            val actual = popupOutputs(key)
            assertTrue(actual.containsAll(popups), "key $keyOutputText: expected $popups in $actual")
        }
    }

    private fun allOutputs(layout: String = "sinhala_wijesekara"): Set<String> {
        val result = mutableSetOf<String>()
        for (element in listOf(KeyboardElement.ALPHABET, KeyboardElement.ALPHABET_MANUAL_SHIFTED)) {
            buildKeys(element, layout).take(3).flatten().filter { isLetterKey(it) }.forEach {
                result.add(keyOutput(it.mCode, it.outputText))
                result.addAll(popupOutputs(it))
            }
        }
        return result
    }

    private fun buildKeys(element: KeyboardElement, layout: String): List<List<KeyParams>> {
        val layoutParams = KeyboardLayoutSet.Params()
        fun set(name: String, value: Any) = KeyboardLayoutSet.Params::class.java.getDeclaredField(name)
            .apply { isAccessible = true }.set(layoutParams, value)
        set("editorInfo", EditorInfo())
        set("subtype", RichInputMethodSubtype.get(subtype(layout)))
        KeyboardLayoutSet.Params::class.java.getDeclaredField("keyboardWidth").apply { isAccessible = true }.setInt(layoutParams, 500)
        KeyboardLayoutSet.Params::class.java.getDeclaredField("keyboardHeight").apply { isAccessible = true }.setInt(layoutParams, 300)
        val builder = KeyboardBuilder(latinIME, KeyboardParams(UniqueKeysCache.NO_CACHE))
        builder.load(KeyboardId(element, layoutParams))
        builder.build()
        @Suppress("UNCHECKED_CAST")
        return KeyboardBuilder::class.java.getDeclaredField("keysInRows").apply { isAccessible = true }
            .get(builder) as List<List<KeyParams>>
    }

    companion object {
        const val EXISTING_SINHALA_SHA256 = "9008f8663f007251dd7ef4c931413fce246511ed9b26dbcaa5129b09a6364e96"
    }
}
