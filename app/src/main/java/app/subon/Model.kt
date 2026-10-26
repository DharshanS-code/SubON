package app.subon

import kotlin.math.roundToInt
import kotlinx.coroutines.flow.MutableStateFlow

enum class Steepness(val label: String, val order: Int) {
    GENTLE("Gentle", 2),
    STEEP("Steep", 6),
    MAX("Max", 16)
}

enum class OutputProfile(
    val label: String,
    val usableFromHz: Int,
    val defaultLow: Float,
    val defaultHigh: Float
) {
    PHONE("Phone speaker", 150, 100f, 200f),
    WIRED("Wired headphones", 20, 30f, 120f),
    BLUETOOTH("Bluetooth", 60, 50f, 150f),
    SUBWOOFER("Subwoofer", 20, 25f, 80f)
}

enum class Preset(val label: String, val low: Float, val high: Float) {
    TRUE_SUB("True sub", 25f, 60f),
    SUB_BASS("Sub + bass", 30f, 120f),
    SMALL_SPEAKER("Small speaker", 100f, 200f),
    CUSTOM("Custom", 0f, 0f)
}

object Limits {
    const val LOW_MIN = 20f
    const val LOW_MAX = 100f
    const val HIGH_MIN = 40f
    const val HIGH_MAX = 300f
    const val MIN_GAP = 10f
}

data class FilterConfig(
    val lowHz: Float = 30f,
    val highHz: Float = 120f,
    val steepness: Steepness = Steepness.STEEP,
    val limiterOn: Boolean = true,
    val gainDb: Float = 0f,
    val profile: OutputProfile = OutputProfile.WIRED
) {
    fun withRange(low: Float, high: Float): FilterConfig {
        val l = low.roundToInt().toFloat().coerceIn(Limits.LOW_MIN, Limits.LOW_MAX)
        val h = high.roundToInt().toFloat()
            .coerceIn(Limits.HIGH_MIN, Limits.HIGH_MAX)
            .coerceAtLeast(l + Limits.MIN_GAP)
        return copy(lowHz = l, highHz = h)
    }

    fun preset(): Preset =
        Preset.values().firstOrNull {
            it != Preset.CUSTOM && it.low == lowHz && it.high == highHz
        } ?: Preset.CUSTOM
}

enum class EffectState { OFF, ACTIVE, WAITING, FAILED }

data class EngineStatus(val state: EffectState, val detail: String = "")

object SubOnState {
    val config = MutableStateFlow(FilterConfig())
    val status = MutableStateFlow(EngineStatus(EffectState.OFF))
    @Volatile
    var loaded = false
}
