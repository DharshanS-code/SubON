package app.subon

import android.media.audiofx.DynamicsProcessing

class SubOnEngine {
    private class Slot(val dp: DynamicsProcessing, val bands: Int, val session: Int)

    private var global: Slot? = null
    private val sessions = HashMap<Int, Slot>()
    private var config = FilterConfig()
    private var lastError = ""

    @Synchronized
    fun start(c: FilterConfig): EngineStatus {
        stop()
        config = c
        global = create(0, c)
        return status()
    }

    @Synchronized
    fun update(c: FilterConfig) {
        config = c
        global?.let {
            release(it)
            global = create(0, c)
        }
        val activeSessions = sessions.keys.toList()
        for (session in activeSessions) {
            sessions.remove(session)?.let { release(it) }
            create(session, c)?.let { sessions[session] = it }
        }
    }

    @Synchronized
    fun onSessionOpen(session: Int) {
        if (global != null || session <= 0 || sessions.containsKey(session)) return
        create(session, config)?.let { sessions[session] = it }
    }

    @Synchronized
    fun onSessionClose(session: Int) {
        sessions.remove(session)?.let { release(it) }
    }

    @Synchronized
    fun stop() {
        global?.let { release(it) }
        global = null
        sessions.values.forEach { release(it) }
        sessions.clear()
    }

    @Synchronized
    fun status(): EngineStatus {
        val g = global
        if (g != null) {
            return EngineStatus(EffectState.ACTIVE, "System-wide filter attached (${g.bands} bands)")
        }
        if (sessions.isNotEmpty()) {
            return EngineStatus(EffectState.ACTIVE, "Attached to ${sessions.size} app audio session(s)")
        }
        val why = if (lastError.isEmpty()) "unknown reason" else lastError
        return EngineStatus(
            EffectState.FAILED,
            "System-wide effect was blocked ($why). Waiting for apps to open an audio session."
        )
    }

    private fun limiterFor(c: FilterConfig) =
        DynamicsProcessing.Limiter(true, c.limiterOn, 0, 1f, 60f, 10f, -2f, 0f)

    private fun create(session: Int, c: FilterConfig): Slot? {
        for (count in BAND_COUNTS) {
            var created: DynamicsProcessing? = null
            try {
                val cutoffs = FilterMath.cutoffs(count)
                val gains = FilterMath.bandGains(count, c)
                val dp = DynamicsProcessing(0, session, buildConfig(count, c))
                created = dp
                if (!dp.hasControl()) {
                    throw IllegalStateException("another app controls the effect")
                }
                dp.setEnabled(true)
                return Slot(dp, count, session)
            } catch (e: Throwable) {
                lastError = e.javaClass.simpleName + ": " + (e.message ?: "")
                try {
                    created?.release()
                } catch (e2: Throwable) {
                    // ignore
                }
            }
        }
        return null
    }

    private fun buildConfig(count: Int, c: FilterConfig): DynamicsProcessing.Config {
        val cutoffs = FilterMath.cutoffs(count)
        val gains = FilterMath.bandGains(count, c)
        val preEq = DynamicsProcessing.Eq(true, true, count)
        for (i in 0 until count) {
            preEq.setBand(i, DynamicsProcessing.EqBand(true, cutoffs[i], gains[i]))
        }
        val builder = DynamicsProcessing.Config.Builder(
            DynamicsProcessing.VARIANT_FAVOR_FREQUENCY_RESOLUTION,
            CHANNELS,
            true, count,
            false, 0,
            false, 0,
            true
        )
        builder.setPreEqAllChannelsTo(preEq)
        builder.setInputGainAllChannelsTo(c.gainDb)
        builder.setLimiterAllChannelsTo(limiterFor(c))
        return builder.build()
    }

    private fun release(slot: Slot) {
        try {
            slot.dp.release()
        } catch (e: Throwable) {
            // ignore
        }
    }

    companion object {
        private const val CHANNELS = 2
        private val BAND_COUNTS = intArrayOf(64, 40, 24)
    }
}
