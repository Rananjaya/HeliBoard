// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard.settings.screens

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.text.method.LinkMovementMethod
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import helium314.keyboard.latin.BuildConfig
import helium314.keyboard.latin.R
import helium314.keyboard.latin.common.Links
import helium314.keyboard.latin.settings.DebugSettings
import helium314.keyboard.latin.settings.Defaults
import helium314.keyboard.latin.utils.Log
import helium314.keyboard.latin.utils.SpannableStringUtils
import helium314.keyboard.latin.utils.getActivity
import helium314.keyboard.latin.utils.prefs
import helium314.keyboard.settings.SettingsDestination
import helium314.keyboard.settings.SettingsContainer
import helium314.keyboard.settings.SettingsWithoutKey
import helium314.keyboard.settings.Setting
import helium314.keyboard.settings.preferences.Preference
import helium314.keyboard.settings.SearchSettingsScreen
import helium314.keyboard.settings.SettingsActivity
import helium314.keyboard.latin.utils.Theme
import helium314.keyboard.latin.utils.previewDark
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Calendar
import androidx.core.content.edit
import helium314.keyboard.latin.utils.IntentUtils
import java.util.Locale

@Composable
fun AboutScreen(
    onClickBack: () -> Unit,
) {
    val items = listOf(
        SettingsWithoutKey.APP,
        SettingsWithoutKey.VERSION,
        SettingsWithoutKey.DEVELOPER,
        SettingsWithoutKey.THIRD_PARTY,
        SettingsWithoutKey.LICENSE,
        SettingsWithoutKey.HIDDEN_FEATURES,
        SettingsWithoutKey.GITHUB_WIKI,
        SettingsWithoutKey.COMMUNITY_LINKS,
        SettingsWithoutKey.GITHUB,
        SettingsWithoutKey.SAVE_LOG,
    )
    SearchSettingsScreen(
        onClickBack = onClickBack,
        title = stringResource(R.string.settings_screen_about),
        settings = items
    )
}

fun createAboutSettings(context: Context) = listOf(
    Setting(context, SettingsWithoutKey.APP, R.string.english_ime_name, R.string.app_slogan) {
        Preference(
            name = it.title,
            description = it.description,
            onClick = { },
            icon = R.mipmap.ic_launcher_round
        )
    },
    Setting(context, SettingsWithoutKey.VERSION, R.string.version) {
        var count by rememberSaveable { mutableIntStateOf(0) }
        val ctx = LocalContext.current
        val prefs = ctx.prefs()
        Preference(
            name = it.title,
            description = stringResource(R.string.version_text, BuildConfig.VERSION_NAME),
            onClick = {
                if (prefs.getBoolean(DebugSettings.PREF_SHOW_DEBUG_SETTINGS, Defaults.PREF_SHOW_DEBUG_SETTINGS) || BuildConfig.DEBUG)
                    return@Preference
                count++
                if (count < 5) return@Preference
                prefs.edit { putBoolean(DebugSettings.PREF_SHOW_DEBUG_SETTINGS, true) }
                Toast.makeText(ctx, R.string.prefs_debug_settings_enabled, Toast.LENGTH_LONG).show()
            },
            icon = R.drawable.ic_settings_about_version
        )
    },
    Setting(context, SettingsWithoutKey.DEVELOPER, R.string.about_developer, R.string.about_developer_name) {
        DeveloperCredit(title = it.title)
    },
    Setting(context, SettingsWithoutKey.THIRD_PARTY, R.string.about_third_party, R.string.about_third_party_description) {
        Preference(
            name = it.title,
            description = it.description,
            onClick = { SettingsDestination.navigateTo(SettingsDestination.ThirdPartyLicenses) },
            icon = R.drawable.ic_settings_about_license
        )
    },
    Setting(context, SettingsWithoutKey.LICENSE, R.string.license, R.string.gnu_gpl) {
        val ctx = LocalContext.current
        Preference(
            name = it.title,
            description = it.description,
            onClick = {
                val intent = Intent()
                intent.data = Links.LICENSE.toUri()
                intent.action = Intent.ACTION_VIEW
                ctx.startActivity(intent)
            },
            icon = R.drawable.ic_settings_about_license
        )
    },
    Setting(context, SettingsWithoutKey.HIDDEN_FEATURES, R.string.hidden_features_title, R.string.hidden_features_summary) {
        val ctx = LocalContext.current
        Preference(
            name = it.title,
            description = it.description,
            onClick = {
                // Compose dialogs are in a rather sad state. They don't understand HTML, and don't scroll without customization.
                // this should be re-done in compose, but... bah
                val link = ("<a href=\"https://developer.android.com/reference/android/content/Context#createDeviceProtectedStorageContext()\">"
                        + ctx.getString(R.string.hidden_features_text) + "</a>")
                val message = ctx.getString(R.string.hidden_features_message, link)
                val dialogMessage = SpannableStringUtils.fromHtml(message)
                val builder = AlertDialog.Builder(ctx)
                    .setIcon(R.drawable.ic_settings_about_hidden_features)
                    .setTitle(R.string.hidden_features_title)
                    .setMessage(dialogMessage)
                    .setPositiveButton(R.string.dialog_close, null)
                    .create()
                builder.show()
                (builder.findViewById<View>(android.R.id.message) as TextView).movementMethod = LinkMovementMethod.getInstance()
            },
            icon = R.drawable.ic_settings_about_hidden_features
        )
    },
    Setting(context, SettingsWithoutKey.GITHUB_WIKI, R.string.about_wiki_link, R.string.about_wiki_link_description) {
        val ctx = LocalContext.current
        Preference(
            name = it.title,
            description = it.description,
            onClick = {
                val intent = Intent()
                intent.data = Links.WIKI_URL.toUri()
                intent.action = Intent.ACTION_VIEW
                ctx.startActivity(intent)
            },
            icon = R.drawable.ic_settings_about_wiki
        )
    },
    Setting(context, SettingsWithoutKey.COMMUNITY_LINKS, R.string.about_community_links, R.string.about_community_links_description) {
        val ctx = LocalContext.current
        Preference(
            name = it.title,
            description = it.description,
            onClick = {
                val intent = Intent()
                intent.data = Links.COMMUNITY_LINKS.toUri()
                intent.action = Intent.ACTION_VIEW
                ctx.startActivity(intent)
            },
            icon = R.drawable.ic_settings_about_community
        )
     },
    Setting(context, SettingsWithoutKey.GITHUB, R.string.about_github_link, R.string.about_source_code_description) {
        val ctx = LocalContext.current
        Preference(
            name = it.title,
            description = it.description,
            onClick = {
                val intent = Intent()
                intent.data = Links.SOURCE_CODE.toUri()
                intent.action = Intent.ACTION_VIEW
                ctx.startActivity(intent)
            },
            icon = R.drawable.ic_settings_about_github
        )
    },
    Setting(context, SettingsWithoutKey.SAVE_LOG, R.string.save_log) { setting ->
        val ctx = LocalContext.current
        val scope = rememberCoroutineScope()
        val launcher = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode != Activity.RESULT_OK) return@rememberLauncherForActivityResult
            val uri = result.data?.data ?: return@rememberLauncherForActivityResult
            scope.launch(Dispatchers.IO) {
                ctx.getActivity()?.contentResolver?.openOutputStream(uri)?.use { os ->
                    os.writer().use { writer ->
                        val logcat = Runtime.getRuntime().exec("logcat -d -b all *:W").inputStream.use { it.reader().readText() }
                        val internal = Log.getLog().joinToString("\n")
                        writer.write(logcat + "\n\n" + internal)
                    }
                }
            }
        }
        Preference(
            name = setting.title,
            description = setting.description,
            onClick = {
                val date = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(Calendar.getInstance().time)
                val intent = IntentUtils.getResolvableTypeIntent(ctx, "text/plain")
                    .setAction(Intent.ACTION_CREATE_DOCUMENT)
                    .putExtra(
                        Intent.EXTRA_TITLE,
                        ctx.getString(R.string.english_ime_name)
                            .replace(" ", "_") + "_log_$date.txt"
                    )
                launcher.launch(intent)
            },
            icon = R.drawable.ic_settings_about_log
        )
    },
)

@Composable
private fun DeveloperCredit(title: String) {
    val ctx = LocalContext.current
    val name = stringResource(R.string.about_developer_name)
    val portrait = remember(ctx) {
        runCatching {
            ctx.assets.open("about_developer_photo.jpg").use { input ->
                BitmapFactory.decodeStream(input)?.asImageBitmap()
            }
        }.getOrNull()
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable {
                ctx.startActivity(Intent(Intent.ACTION_VIEW, Links.DEVELOPER.toUri()))
            }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (portrait != null) {
            Image(
                bitmap = portrait,
                contentDescription = name,
                contentScale = ContentScale.Crop,
                alignment = Alignment.TopCenter,
                modifier = Modifier
                    .size(72.dp)
                    .clip(CircleShape)
                    .border(1.5.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.45f), CircleShape),
            )
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = stringResource(R.string.about_developer_description),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 2.dp),
            )
            Text(
                text = name,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(top = 1.dp),
            )
        }
    }
}

@Preview
@Composable
private fun Preview() {
    SettingsActivity.settingsContainer = SettingsContainer(LocalContext.current)
    Theme(previewDark) {
        Surface {
            AboutScreen {  }
        }
    }
}
