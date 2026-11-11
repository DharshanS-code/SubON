package app.subon

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Shared visual tokens: docs/design-system.md and WindowsApp.cpp.
val Zaffre = Color(0xFF10141C)
val ZaffreDark = Color(0xFF1B2230)
val Periwinkle = Color(0xFF344158)
val Citron = Color(0xFFCCCB66)
val Lavender = Color(0xFFF1F4FA)
val Muted = Color(0xFFA6B1C2)
val Warn = Color(0xFFFFB4A8)
val ResponseLine = Color(0xFF999832)

@Composable
fun SubOnTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = Citron,
            onPrimary = ZaffreDark,
            secondary = Muted,
            onSecondary = Zaffre,
            background = Zaffre,
            surface = ZaffreDark,
            surfaceVariant = Periwinkle,
            onBackground = Lavender,
            onSurface = Lavender,
            onSurfaceVariant = Muted,
            outline = Muted,
            error = Warn,
            onError = Zaffre
        ),
        content = content
    )
}
