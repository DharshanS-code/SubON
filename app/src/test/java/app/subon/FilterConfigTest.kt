package app.subon

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class FilterConfigTest {
    @After
    fun resetProcessState() {
        SubOnState.config.value = FilterConfig()
        SubOnState.loaded = false
    }

    @Test
    fun `first activity or service loads once without overwriting live edits`() {
        SubOnState.loaded = false
        SubOnState.ensureLoaded { FilterConfig(25f, 80f) }
        assertEquals(FilterConfig(25f, 80f), SubOnState.config.value)
        val live = FilterConfig(35f, 140f)
        SubOnState.config.value = live
        SubOnState.ensureLoaded { throw AssertionError("Already loaded preferences must not be read again") }
        assertEquals(live, SubOnState.config.value)
    }

    @Test
    fun `non finite values recover to profile defaults`() {
        for (profile in OutputProfile.values()) {
            for (invalid in listOf(Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY)) {
                val actual = FilterConfig(invalid, invalid, gainDb = invalid, profile = profile).normalized()
                assertEquals(profile.defaultLow, actual.lowHz, 0f)
                assertEquals(profile.defaultHigh, actual.highHz, 0f)
                assertEquals(0f, actual.gainDb, 0f)
            }
        }
    }

    @Test
    fun `ranges round clamp and maintain minimum gap`() {
        val config = FilterConfig().withRange(99.6f, -100f)
        assertEquals(100f, config.lowHz, 0f)
        assertEquals(110f, config.highHz, 0f)
        assertEquals(20f, config.withRange(-Float.MAX_VALUE, Float.MAX_VALUE).lowHz, 0f)
        assertEquals(300f, config.withRange(-Float.MAX_VALUE, Float.MAX_VALUE).highHz, 0f)
        assertEquals(31f, config.withRange(30.5f, 120.4f).lowHz, 0f)
        assertEquals(120f, config.withRange(30.5f, 120.4f).highHz, 0f)
    }

    @Test
    fun `withRange never rounds NaN`() {
        assertEquals(FilterConfig(), FilterConfig().withRange(Float.NaN, Float.NaN))
    }

    @Test
    fun `normalization is idempotent and limits gain`() {
        for (gain in listOf(-Float.MAX_VALUE, -12f, -0.5f, 6f, Float.MAX_VALUE)) {
            val actual = FilterConfig(1000f, 1f, gainDb = gain).normalized()
            assertEquals(actual, actual.normalized())
            assertTrue(actual.gainDb in -12f..6f)
            assertTrue(actual.highHz - actual.lowHz >= Limits.MIN_GAP)
        }
    }

    @Test
    fun `known presets retain identity`() {
        for (preset in Preset.values().filter { it != Preset.CUSTOM }) {
            assertEquals(preset, FilterConfig().withRange(preset.low, preset.high).preset())
        }
        assertEquals(Preset.CUSTOM, FilterConfig().withRange(35f, 170f).preset())
    }
}
