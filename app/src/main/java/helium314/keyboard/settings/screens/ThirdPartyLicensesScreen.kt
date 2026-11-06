// SPDX-License-Identifier: GPL-3.0-only
package helium314.keyboard.settings.screens

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import helium314.keyboard.latin.R
import helium314.keyboard.settings.SearchScreen

private val packagedLicenceFiles = listOf(
    "Unicode-3.0-CLDR.txt",
    "Unicode-License-Signal-emoji-search-index.txt",
    "AGPL-3.0.txt",
    "THIRD_PARTY_NOTICES.md",
    "LICENSES-en.txt",
    "GPL-3.0.txt",
)

/** Shows the third-party licence files packaged at assets/third_party_licenses/. */
@Composable
fun ThirdPartyLicensesScreen(onClickBack: () -> Unit) {
    val context = LocalContext.current
    val documents = remember(context) {
        packagedLicenceFiles.map { name ->
            val text = runCatching {
                context.assets.open("third_party_licenses/$name").bufferedReader().use { it.readText() }
            }.getOrElse { "Could not read packaged file third_party_licenses/$name" }
            name to text
        }
    }
    SearchScreen(
        onClickBack = onClickBack,
        title = { Text(stringResource(R.string.about_third_party)) },
        filteredItems = { emptyList<Unit>() },
        itemContent = {},
        content = {
            Column(
                Modifier
                    .fillMaxWidth()
                    .fillMaxHeight()
                    .verticalScroll(rememberScrollState())
                    .padding(16.dp)
            ) {
                Text(
                    text = stringResource(R.string.about_third_party_description),
                    style = MaterialTheme.typography.bodyLarge,
                )
                documents.forEach { (name, text) ->
                    Text(
                        text = name,
                        style = MaterialTheme.typography.titleSmall,
                        modifier = Modifier.padding(top = 20.dp, bottom = 8.dp),
                    )
                    Text(text = text, style = MaterialTheme.typography.bodySmall)
                }
            }
        },
    )
}
