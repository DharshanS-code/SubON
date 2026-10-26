package app.subon

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Zaffre = Color(0xFF1826A4)
val ZaffreDark = Color(0xFF121C85)
val Periwinkle = Color(0xFF5C7BC6)
val Citron = Color(0xFFCCCB66)
val Lavender = Color(0xFFC1C5E9)
val Muted = Color(0xFF9AA2DA)
val Warn = Color(0xFFFFB4A8)

@Composable
fun SubOnTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Citron,
            onPrimary = ZaffreDark,
            secondary = Periwinkle,
            background = Zaffre,
            surface = Zaffre,
            onBackground = Lavender,
            onSurface = Lavender
        ),
        content = content
    )
}
