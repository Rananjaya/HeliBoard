// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard

import android.view.inputmethod.InputMethodSubtype
import androidx.core.content.edit
import helium314.keyboard.latin.LatinIME
import helium314.keyboard.latin.common.LocaleUtils.constructLocale
import helium314.keyboard.latin.settings.Settings
import helium314.keyboard.latin.settings.SettingsSubtype.Companion.toSettingsSubtype
import helium314.keyboard.latin.utils.LayoutType
import helium314.keyboard.latin.utils.LayoutUtilsCustom
import helium314.keyboard.latin.utils.SubtypeSettings
import helium314.keyboard.latin.utils.SubtypeUtilsAdditional
import helium314.keyboard.latin.utils.locale
import helium314.keyboard.latin.utils.mainLayoutName
import helium314.keyboard.latin.utils.mainLayoutNameOrQwerty
import helium314.keyboard.latin.utils.prefs
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.Locale
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * RCBBoard allow-list: custom layouts (additional subtypes) are offered in Languages & Layouts
 * when their language is allowed. Custom layouts are created like SubtypeScreen does it, through
 * SubtypeUtilsAdditional.changeAdditionalSubtype, and checked with SubtypeSettings.getSelectableSubtypes.
 */
@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowInputMethodManager2::class])
class LanguageAllowListCustomLayoutTest {
    private val latinIME = Robolectric.setupService(LatinIME::class.java)
    private val enUs = "en-US".constructLocale()
    private val siLk = "si-LK".constructLocale()
    private val german = "de-DE".constructLocale()

    private val subtypePrefs = listOf(Settings.PREF_ADDITIONAL_SUBTYPES, Settings.PREF_ENABLED_SUBTYPES, Settings.PREF_SELECTED_SUBTYPE)

    // start each test without additional, enabled or selected subtypes
    @BeforeTest fun clearSubtypeSettings() {
        latinIME.prefs().edit { subtypePrefs.forEach { putString(it, "") } }
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
    }

    // back to the defaults, so other test classes are not affected
    @AfterTest fun restoreSubtypeSettings() {
        latinIME.prefs().edit { subtypePrefs.forEach { remove(it) } }
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
    }

    // SubtypeSettings.init appends on every LatinIME created in a test run, so compare by subtype id
    private fun List<InputMethodSubtype>.distinct() = distinctBy { it.hashCode() }

    private fun resourceSubtype(locale: Locale, mainLayout: String? = null): InputMethodSubtype =
        SubtypeSettings.getResourceSubtypesForLocale(locale).first { mainLayout == null || it.mainLayoutName() == mainLayout }

    /** Gives [from] a custom main layout, as done when picking a custom layout in the layout settings. */
    private fun createCustomLayout(from: InputMethodSubtype, displayName: String): String {
        val layoutName = LayoutUtilsCustom.getLayoutName(displayName, LayoutType.MAIN, from.locale())
        assertTrue(LayoutUtilsCustom.isCustomLayout(layoutName), layoutName)
        val to = from.toSettingsSubtype().withLayout(LayoutType.MAIN, layoutName)
        SubtypeUtilsAdditional.changeAdditionalSubtype(from.toSettingsSubtype(), to, latinIME)
        return layoutName
    }

    private fun selectable(): Set<Pair<Locale, String>> =
        SubtypeSettings.getSelectableSubtypes().distinct().mapTo(mutableSetOf()) { it.locale() to it.mainLayoutNameOrQwerty() }

    private fun additional(): Set<Pair<Locale, String>> =
        SubtypeSettings.getAdditionalSubtypes().mapTo(mutableSetOf()) { it.locale() to it.mainLayoutNameOrQwerty() }

    @Test fun `custom English (US) layout is offered`() {
        val layout = createCustomLayout(resourceSubtype(enUs), "My English")
        assertTrue((enUs to layout) in additional(), "${additional()}")
        assertTrue((enUs to layout) in selectable(), "${selectable()}")
    }

    @Test fun `custom Sinhala layout is offered`() {
        val layout = createCustomLayout(resourceSubtype(siLk, "sinhala"), "My Sinhala")
        assertTrue((siLk to layout) in additional(), "${additional()}")
        assertTrue((siLk to layout) in selectable(), "${selectable()}")
    }

    @Test fun `custom German layout is kept but not offered`() {
        val layout = createCustomLayout(resourceSubtype(german), "Mein Deutsch")
        // saved and loaded as before, so it returns if German is allowed again ...
        assertTrue((german to layout) in additional(), "${additional()}")
        // ... but not offered as a keyboard language
        assertFalse(selectable().any { it.first == german }, "${selectable()}")
    }

    @Test fun `built-in layouts stay offered next to custom layouts`() {
        val english = createCustomLayout(resourceSubtype(enUs), "My English")
        val sinhala = createCustomLayout(resourceSubtype(siLk, "sinhala_wijesekara"), "My Wijesekara")
        createCustomLayout(resourceSubtype(german), "Mein Deutsch")
        assertEquals(
            setOf(
                enUs to "qwerty", siLk to "sinhala", siLk to "sinhala_wijesekara", // built-in
                enUs to english, siLk to sinhala, // custom, allowed languages
            ),
            selectable()
        )
    }

    @Test fun `enabled custom Sinhala layout stays enabled after reload`() {
        val from = resourceSubtype(siLk, "sinhala")
        SubtypeSettings.addEnabledSubtype(latinIME.prefs(), from)
        val layout = createCustomLayout(from, "My Sinhala")
        SubtypeSettings.reloadEnabledSubtypes(latinIME)
        val enabled = SubtypeSettings.getEnabledSubtypes(false).map { it.locale() to it.mainLayoutNameOrQwerty() }
        assertEquals(listOf(siLk to layout), enabled)
    }
}
