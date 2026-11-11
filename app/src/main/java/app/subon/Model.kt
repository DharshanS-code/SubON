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
    const val GAIN_MIN = -12f
    const val GAIN_MAX = 6f
}

data class FilterConfig(
    val lowHz: Float = 30f,
    val highHz: Float = 120f,
    val steepness: Steepness = Steepness.STEEP,
    val limiterOn: Boolean = true,
    val gainDb: Float = 0f,
    val profile: OutputProfile = OutputProfile.WIRED
) {
    fun withRange(low: Float, high: Float): FilterConfig =
        copy(lowHz = low, highHz = high).normalized()

    /** Shared boundary for persisted values, UI edits and effect parameters. */
    fun normalized(): FilterConfig {
        val low = (if (lowHz.isFinite()) lowHz else profile.defaultLow)
            .coerceIn(Limits.LOW_MIN, Limits.LOW_MAX).roundToInt().toFloat()
        val high = (if (highHz.isFinite()) highHz else profile.defaultHigh)
            .coerceIn(Limits.HIGH_MIN, Limits.HIGH_MAX).roundToInt().toFloat()
            .coerceAtLeast(low + Limits.MIN_GAP)
        val gain = (if (gainDb.isFinite()) gainDb else 0f)
            .coerceIn(Limits.GAIN_MIN, Limits.GAIN_MAX)
        return copy(lowHz = low, highHz = high, gainDb = gain)
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

    @Synchronized
    fun ensureLoaded(load: () -> FilterConfig) {
        if (loaded) return
        config.value = load().normalized()
        loaded = true
    }
}
