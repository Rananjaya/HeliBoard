// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard

import android.view.inputmethod.InputMethodSubtype
import androidx.core.content.edit
import helium314.keyboard.latin.LatinIME
import helium314.keyboard.latin.R
import helium314.keyboard.latin.common.LocaleUtils.constructLocale
import helium314.keyboard.latin.dictionary.getDictionaryLocales
import helium314.keyboard.latin.settings.Defaults
import helium314.keyboard.latin.settings.Settings
import helium314.keyboard.latin.settings.SettingsSubtype.Companion.toSettingsSubtype
import helium314.keyboard.latin.utils.SubtypeSettings
import helium314.keyboard.latin.utils.locale
import helium314.keyboard.latin.utils.mainLayoutName
import helium314.keyboard.latin.utils.mainLayoutNameOrQwerty
import helium314.keyboard.latin.utils.prefs
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.Locale
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** RCBBoard: Languages & Layouts offers only the languages in R.array.rcbboard_enabled_locales. */
@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowInputMethodManager2::class])
class LanguageAllowListTest {
    private val latinIME = Robolectric.setupService(LatinIME::class.java)
    private val enUs = "en-US".constructLocale()
    private val siLk = "si-LK".constructLocale()

    // SubtypeSettings.init appends on every LatinIME created in a test run (once per process in
    // the app), so compare by subtype id.
    private fun List<InputMethodSubtype>.distinct() = distinctBy { it.hashCode() }

    // other test classes may leave a changed additional-subtypes setting behind, so set the default
    private fun loadDefaultAdditionalSubtypes() {
        latinIME.prefs().edit { putString(Settings.PREF_ADDITIONAL_SUBTYPES, Defaults.PREF_ADDITIONAL_SUBTYPES) }
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
    }

    private fun selectable() = SubtypeSettings.getSelectableSubtypes().distinct()
        .map { it.locale().toLanguageTag() to it.mainLayoutNameOrQwerty() }

    @Test fun `configured allow-list is en-US and si-LK`() {
        assertEquals(listOf("en-US", "si-LK"), latinIME.resources.getStringArray(R.array.rcbboard_enabled_locales).toList())
    }

    @Test fun `exactly three primary layouts are offered`() {
        loadDefaultAdditionalSubtypes() // German/French/Hungarian defaults present, and still not offered
        assertEquals(
            setOf("en-US" to "qwerty", "si-LK" to "sinhala", "si-LK" to "sinhala_wijesekara"),
            selectable().toSet()
        )
        assertEquals(3, selectable().size)
    }

    @Test fun `both Sinhala layouts are offered`() {
        val sinhala = SubtypeSettings.getSelectableSubtypes().distinct().filter { it.locale() == siLk }
        assertEquals(setOf("sinhala", "sinhala_wijesekara"), sinhala.mapNotNull { it.mainLayoutName() }.toSet())
    }

    @Test fun `other languages and default additional layouts are not offered`() {
        loadDefaultAdditionalSubtypes()
        val offered = selectable().map { it.first }.toSet()
        for (tag in listOf("de", "fr", "hu", "en-GB", "en-AU", "en-CA", "en-IN", "ko", "es", "ta", "zz")) {
            assertFalse(tag in offered, "$tag must not be offered")
            assertFalse(SubtypeSettings.isAllowedLocale(tag.constructLocale()), tag)
        }
        // German/French/Hungarian default additional subtypes still exist, they are only not offered
        val additionalLanguages = SubtypeSettings.getAdditionalSubtypes().map { it.locale().language }
        assertTrue(additionalLanguages.containsAll(listOf("de", "fr", "hu")), "$additionalLanguages")
    }

    @Test fun `locale matching is exact`() {
        val allowed = SubtypeSettings.parseAllowedLocales(arrayOf(" en-US ", "si-LK", ""))!!
        assertTrue(SubtypeSettings.isAllowedLocale(enUs, allowed))
        assertTrue(SubtypeSettings.isAllowedLocale(siLk, allowed))
        assertTrue(SubtypeSettings.isAllowedLocale(Locale.US, allowed))
        assertFalse(SubtypeSettings.isAllowedLocale("en-GB".constructLocale(), allowed))
        assertFalse(SubtypeSettings.isAllowedLocale(Locale.ENGLISH, allowed)) // "en" without country
        assertFalse(SubtypeSettings.isAllowedLocale("si".constructLocale(), allowed))
        // the resource subtypes really have these locales, so the allow-list matches them
        assertTrue(SubtypeSettings.getResourceSubtypesForLocale(enUs).isNotEmpty())
        assertEquals(2, SubtypeSettings.getResourceSubtypesForLocale(siLk).distinct().size)
    }

    @Test fun `empty allow-list restores upstream behaviour`() {
        assertNull(SubtypeSettings.parseAllowedLocales(emptyArray()))
        assertNull(SubtypeSettings.parseAllowedLocales(arrayOf("", "  ")))
        val all = SubtypeSettings.getAllAvailableSubtypes().distinct()
        assertTrue(all.all { SubtypeSettings.isAllowedLocale(it.locale(), null) })
        assertTrue(all.size > 100, "all languages: ${all.size}")
    }

    @Test fun `en-US fallback can not be removed`() {
        assertTrue(Locale.US in SubtypeSettings.parseAllowedLocales(arrayOf("si-LK"))!!)
        assertTrue(SubtypeSettings.isAllowedLocale(Locale.US))
    }

    @Test fun `spell checker, dictionaries and multilingual typing are not limited`() {
        // internal language data stays complete
        assertTrue(SubtypeSettings.getResourceSubtypesForLocale("de".constructLocale()).isNotEmpty())
        assertTrue(SubtypeSettings.getAvailableSubtypeLocales().size > 100)
        // spell checker picks layouts for any language (would throw for e.g. Tamil if the data were filtered)
        assertEquals("tamil", SubtypeSettings.getMatchingMainLayoutNameForLocale("ta".constructLocale()))
        // dictionary-based second-language choices still include other languages
        val dictionaryLocales = getDictionaryLocales(latinIME)
        assertTrue(dictionaryLocales.any { it.language == "de" }, "$dictionaryLocales")
        assertTrue(dictionaryLocales.any { it.language == "si" }, "$dictionaryLocales")
    }

    @Test fun `saved unsupported languages are skipped safely and kept in settings`() {
        val prefs = latinIME.prefs()
        val german = SubtypeSettings.getResourceSubtypesForLocale("de".constructLocale()).first()
        val wijesekara = SubtypeSettings.getResourceSubtypesForLocale(siLk).first { it.mainLayoutName() == "sinhala_wijesekara" }
        val enabledPref = SubtypeSettings.createPrefSubtypes(listOf(german.toSettingsSubtype(), wijesekara.toSettingsSubtype()))
        prefs.edit {
            putString(Settings.PREF_ENABLED_SUBTYPES, enabledPref)
            putString(Settings.PREF_SELECTED_SUBTYPE, german.toSettingsSubtype().toPref())
            putBoolean(Settings.PREF_AUTO_CORRECTION, false) // unrelated setting
        }
        SubtypeSettings.reloadEnabledSubtypes(latinIME)

        assertEquals(listOf(wijesekara.hashCode()), SubtypeSettings.getEnabledSubtypes(false).distinct().map { it.hashCode() })
        // the selected German subtype is not used, the keyboard falls back to an allowed one
        assertEquals(siLk, SubtypeSettings.getSelectedSubtype(prefs).locale())
        // saved settings are unchanged, so German returns if it is allowed again
        assertEquals(enabledPref, prefs.getString(Settings.PREF_ENABLED_SUBTYPES, ""))
        assertEquals(german.toSettingsSubtype().toPref(), prefs.getString(Settings.PREF_SELECTED_SUBTYPE, ""))
        assertFalse(prefs.getBoolean(Settings.PREF_AUTO_CORRECTION, true))
    }

    @Test fun `selected German additional layout falls back to an allowed layout`() {
        loadDefaultAdditionalSubtypes()
        val prefs = latinIME.prefs()
        val germanQwerty = SubtypeSettings.getAdditionalSubtypes().first { it.locale().language == "de" }
        prefs.edit {
            putString(Settings.PREF_ENABLED_SUBTYPES, germanQwerty.toSettingsSubtype().toPref())
            putString(Settings.PREF_SELECTED_SUBTYPE, germanQwerty.toSettingsSubtype().toPref())
        }
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
        assertTrue(SubtypeSettings.getEnabledSubtypes(false).isEmpty())
        val selected = SubtypeSettings.getSelectedSubtype(prefs)
        assertTrue(SubtypeSettings.isAllowedLocale(selected.locale()), "${selected.locale()}")
    }

    @Test
    @Config(qualifiers = "de-rDE")
    fun `system language German starts with English (US)`() {
        latinIME.prefs().edit { putString(Settings.PREF_ENABLED_SUBTYPES, "") }
        SubtypeSettings.reloadSystemLocales(latinIME)
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
        assertTrue(SubtypeSettings.getSystemLocales().any { it.language == "de" }, "${SubtypeSettings.getSystemLocales()}")
        assertEquals(listOf(Locale.US), SubtypeSettings.getEnabledSubtypes(true).map { it.locale() })
    }
}
