package app.subon

import org.junit.Assert.assertEquals
import org.junit.Test

class PrefsTest {
    @Test
    fun `empty preferences use wired defaults`() {
        assertEquals(FilterConfig(), SavedConfig.decode(emptyMap<String, Any>()))
    }

    @Test
    fun `selected and explicitly requested profiles restore independent values`() {
        val values = mapOf(
            "profile" to "BLUETOOTH", "BLUETOOTH_low" to 55f, "BLUETOOTH_high" to 155f,
            "BLUETOOTH_gain" to -4f, "BLUETOOTH_limiter" to false,
            "BLUETOOTH_steep" to "MAX", "WIRED_low" to 25f, "WIRED_high" to 80f
        )
        assertEquals(
            FilterConfig(55f, 155f, Steepness.MAX, false, -4f, OutputProfile.BLUETOOTH),
            SavedConfig.decode(values)
        )
        assertEquals(FilterConfig(25f, 80f), SavedConfig.decode(values, OutputProfile.WIRED))
    }

    @Test
    fun `wrong preference types and unknown enum names recover without throwing`() {
        val values = mapOf(
            "profile" to 13, "WIRED_low" to "30", "WIRED_high" to true,
            "WIRED_gain" to "loud", "WIRED_limiter" to "true", "WIRED_steep" to "obsolete"
        )
        assertEquals(FilterConfig(), SavedConfig.decode(values))
        assertEquals(FilterConfig(), SavedConfig.decode(mapOf("profile" to "obsolete")))
    }

    @Test
    fun `corrupt saved floats are sanitized before reaching UI or engine`() {
        assertEquals(
            FilterConfig(100f, 110f, gainDb = 6f),
            SavedConfig.decode(mapOf("WIRED_low" to 1000f, "WIRED_high" to -30f, "WIRED_gain" to 100f))
        )
        assertEquals(
            FilterConfig(profile = OutputProfile.PHONE, lowHz = 100f, highHz = 200f),
            SavedConfig.decode(mapOf("PHONE_low" to Float.NaN, "PHONE_high" to Float.NEGATIVE_INFINITY,
                "PHONE_gain" to Float.POSITIVE_INFINITY), OutputProfile.PHONE)
        )
    }
}
