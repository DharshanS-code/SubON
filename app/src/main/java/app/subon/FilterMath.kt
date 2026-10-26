package app.subon

import kotlin.math.log10
import kotlin.math.max
import kotlin.math.pow

object FilterMath {
    const val MIN_HZ = 20.0
    const val MAX_HZ = 20000.0
    const val FLOOR_DB = -60.0

    private fun ratio(count: Int): Double = (MAX_HZ / MIN_HZ).pow(1.0 / count)

    fun cutoffs(count: Int): FloatArray {
        val r = ratio(count)
        return FloatArray(count) { i ->
            if (i == count - 1) MAX_HZ.toFloat() else (MIN_HZ * r.pow(i + 1.0)).toFloat()
        }
    }

    fun centers(count: Int): DoubleArray {
        val r = ratio(count)
        return DoubleArray(count) { i -> MIN_HZ * r.pow(i + 0.5) }
    }

    fun gainDb(f: Double, c: FilterConfig): Double {
        val n2 = 2.0 * c.steepness.order
        val hp = 10.0 * log10(1.0 + (c.lowHz / f).pow(n2))
        val lp = 10.0 * log10(1.0 + (f / c.highHz).pow(n2))
        return max(-(hp + lp), FLOOR_DB)
    }

    fun bandGains(count: Int, c: FilterConfig): FloatArray {
        val centers = centers(count)
        return FloatArray(count) { i -> gainDb(centers[i], c).toFloat() }
    }
}
