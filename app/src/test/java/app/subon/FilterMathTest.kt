package app.subon

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.sqrt

class FilterMathTest {
    @Test
    fun `bands use ascending logarithmic edges and geometric centers`() {
        for (count in listOf(1, 24, 40, 64)) {
            val cutoffs = FilterMath.cutoffs(count)
            val centers = FilterMath.centers(count)
            assertEquals(count, cutoffs.size)
            assertEquals(20000f, cutoffs.last(), 0f)
            for (i in cutoffs.indices) {
                val lower = if (i == 0) FilterMath.MIN_HZ else cutoffs[i - 1].toDouble()
                assertTrue(cutoffs[i] > lower)
                assertEquals(sqrt(lower * cutoffs[i]), centers[i], centers[i] * 0.000001)
            }
        }
    }

    @Test
    fun `response is bounded finite and attenuates both ends`() {
        for (steepness in Steepness.values()) {
            val config = FilterConfig(steepness = steepness)
            val passBand = FilterMath.gainDb(60.0, config)
            assertTrue(passBand > -1.0)
            assertTrue(FilterMath.gainDb(20.0, config) < passBand)
            assertTrue(FilterMath.gainDb(1000.0, config) < passBand)
            for (gain in FilterMath.bandGains(64, config)) {
                assertTrue(gain.isFinite() && gain in -60f..0f)
            }
        }
    }

    @Test
    fun `cutoff has approximately three decibels attenuation`() {
        val config = FilterConfig(steepness = Steepness.MAX)
        assertEquals(-3.0103, FilterMath.gainDb(config.lowHz.toDouble(), config), 0.0001)
        assertEquals(-3.0103, FilterMath.gainDb(config.highHz.toDouble(), config), 0.0001)
    }

    @Test
    fun `steeper slopes reject more audio outside range`() {
        val gentle = FilterConfig(steepness = Steepness.GENTLE)
        val steep = gentle.copy(steepness = Steepness.STEEP)
        assertTrue(FilterMath.gainDb(15.0, steep) < FilterMath.gainDb(15.0, gentle))
        assertTrue(FilterMath.gainDb(240.0, steep) < FilterMath.gainDb(240.0, gentle))
    }

    @Test
    fun `invalid configurations cannot create non finite band gains`() {
        val corrupt = FilterConfig(Float.NaN, 0f, gainDb = Float.POSITIVE_INFINITY)
        assertTrue(FilterMath.bandGains(24, corrupt).all { it.isFinite() && it in -60f..0f })
    }

    @Test(expected = IllegalArgumentException::class)
    fun `zero band count is rejected`() {
        FilterMath.cutoffs(0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `negative band count is rejected`() {
        FilterMath.centers(-1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `invalid frequency is rejected explicitly`() {
        FilterMath.gainDb(Double.NaN, FilterConfig())
    }
}
