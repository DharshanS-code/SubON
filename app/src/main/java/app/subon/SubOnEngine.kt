package app.subon

import android.media.audiofx.AudioEffect
import android.media.audiofx.DynamicsProcessing

/** Small platform boundary so ownership, failures and lifecycle can be tested on the JVM. */
internal interface EngineEffect {
    fun hasControl(): Boolean
    fun isEnabled(): Boolean
    fun enable(): Int
    fun apply(c: FilterConfig)
    fun listen(onChange: (restoreConfig: Boolean) -> Unit)
    fun release()
}

internal fun interface EngineEffectFactory {
    fun create(session: Int, bands: Int, config: FilterConfig): EngineEffect
}

class SubOnEngine internal constructor(
    private val factory: EngineEffectFactory,
    private val onStatusChanged: () -> Unit = {}
) {
    constructor(onStatusChanged: () -> Unit = {}) : this(AndroidEffectFactory, onStatusChanged)

    private class Slot(val effect: EngineEffect, val bands: Int, val session: Int)

    private var global: Slot? = null
    private val sessions = HashMap<Int, Slot>()
    private val openSessions = HashSet<Int>()
    private var config = FilterConfig()
    private var running = false
    private var lastError = ""
    private var updateFailed = false
    private var retryGlobal = false

    @Synchronized
    fun start(c: FilterConfig): EngineStatus {
        stop()
        running = true
        config = c.normalized()
        global = create(0)
        return status()
    }

    @Synchronized
    fun update(c: FilterConfig) {
        if (!running) return
        val next = c.normalized()
        if (config == next && !updateFailed) return
        config = next
        updateFailed = false
        global?.let { slot ->
            if (!apply(slot)) {
                global = null
                release(slot)
                retryGlobal = true
            }
        } ?: run {
            // Retry an attachment lost to a runtime error, not unsupported session zero on every drag.
            if (retryGlobal && sessions.isEmpty()) {
                global = create(0)
                retryGlobal = global == null
                updateFailed = retryGlobal
            }
        }
        for ((session, slot) in sessions.toMap()) {
            if (!apply(slot)) {
                sessions.remove(session)
                release(slot)
            }
        }
        // Remember open sessions after errors, so the next edit can recover without a new broadcast.
        if (!updateFailed && global == null) {
            for (session in openSessions) {
                if (session !in sessions) create(session)?.let { sessions[session] = it }
            }
        }
    }

    @Synchronized
    fun onSessionOpen(session: Int) {
        if (!running || session <= 0) return
        openSessions.add(session)
        if (global?.let { isActive(it) } == true || session in sessions) return
        create(session)?.let {
            // Do not stack a global effect and session-specific effects if global control returns.
            global?.let { old -> release(old) }
            global = null
            retryGlobal = false
            sessions[session] = it
            updateFailed = false
        }
    }

    @Synchronized
    fun onSessionClose(session: Int) {
        openSessions.remove(session)
        sessions.remove(session)?.let { release(it) }
        if (openSessions.isEmpty()) updateFailed = false
    }

    @Synchronized
    fun stop() {
        running = false
        global?.let { release(it) }
        global = null
        sessions.values.forEach { release(it) }
        sessions.clear()
        openSessions.clear()
        lastError = ""
        updateFailed = false
        retryGlobal = false
    }

    @Synchronized
    fun status(): EngineStatus {
        if (!running) return EngineStatus(EffectState.OFF)
        val g = global
        if (g != null && isActive(g)) {
            return EngineStatus(
                EffectState.ACTIVE,
                "Output-mix effect attached (${g.bands} bands). Coverage depends on the device and app."
            )
        }
        val active = sessions.values.count { isActive(it) }
        if (active > 0) {
            return EngineStatus(EffectState.ACTIVE, "Effect attached to $active app audio session(s); other apps may bypass it.")
        }
        if (updateFailed) {
            return EngineStatus(EffectState.FAILED, "Effect update failed ($lastError). Change a setting or restart to retry.")
        }
        val reason = if (lastError.isEmpty()) "" else " ($lastError)"
        return EngineStatus(
            EffectState.WAITING,
            "No controlled, enabled effect$reason. Waiting for a compatible app audio session; try reopening playback."
        )
    }

    private fun isActive(slot: Slot): Boolean = try {
        slot.effect.hasControl() && slot.effect.isEnabled()
    } catch (e: RuntimeException) {
        recordError(e)
        false
    }

    private fun apply(slot: Slot): Boolean = try {
        // Keep the handle while another controller owns it; the control callback restores our settings.
        if (slot.effect.hasControl()) {
            slot.effect.apply(config)
            check(slot.effect.enable() == AudioEffect.SUCCESS && slot.effect.isEnabled()) {
                "effect could not be enabled"
            }
        }
        true
    } catch (e: RuntimeException) {
        recordError(e)
        updateFailed = true
        false
    }

    private fun create(session: Int): Slot? {
        for (count in BAND_COUNTS) {
            var created: EngineEffect? = null
            try {
                val effect = factory.create(session, count, config)
                created = effect
                check(effect.hasControl()) { "another app controls the effect" }
                check(effect.enable() == AudioEffect.SUCCESS && effect.isEnabled()) {
                    "effect could not be enabled"
                }
                val slot = Slot(effect, count, session)
                effect.listen { restore -> effectChanged(slot, restore) }
                lastError = ""
                return slot
            } catch (e: RuntimeException) {
                recordError(e)
                try {
                    created?.release()
                } catch (releaseError: RuntimeException) {
                    recordError(releaseError)
                }
            }
        }
        return null
    }

    @Synchronized
    private fun effectChanged(slot: Slot, restoreConfig: Boolean) {
        if (!running || (global !== slot && sessions[slot.session] !== slot)) return
        if (restoreConfig && !apply(slot)) {
            if (global === slot) {
                global = null
                retryGlobal = true
            } else {
                sessions.remove(slot.session)
            }
            release(slot)
        }
        onStatusChanged()
    }

    private fun recordError(e: RuntimeException) {
        lastError = e.javaClass.simpleName + ": " + (e.message ?: "audio effect error")
    }

    private fun release(slot: Slot) {
        try {
            slot.effect.release()
        } catch (e: RuntimeException) {
            recordError(e)
        }
    }

    companion object {
        private val BAND_COUNTS = intArrayOf(64, 40, 24)
    }
}

private object AndroidEffectFactory : EngineEffectFactory {
    override fun create(session: Int, bands: Int, config: FilterConfig): EngineEffect =
        AndroidEffect(DynamicsProcessing(0, session, buildConfig(bands, config)), bands)

    private fun limiter(c: FilterConfig) =
        DynamicsProcessing.Limiter(true, c.limiterOn, 0, 1f, 60f, 10f, -2f, 0f)

    private fun equalizer(count: Int, c: FilterConfig): DynamicsProcessing.Eq {
        val cutoffs = FilterMath.cutoffs(count)
        val gains = FilterMath.bandGains(count, c)
        return DynamicsProcessing.Eq(true, true, count).apply {
            for (i in 0 until count) {
                setBand(i, DynamicsProcessing.EqBand(true, cutoffs[i], gains[i]))
            }
        }
    }

    private fun buildConfig(count: Int, c: FilterConfig): DynamicsProcessing.Config =
        DynamicsProcessing.Config.Builder(
            DynamicsProcessing.VARIANT_FAVOR_FREQUENCY_RESOLUTION,
            2,
            true, count,
            false, 0,
            false, 0,
            true
        ).apply {
            setPreEqAllChannelsTo(equalizer(count, c))
            setInputGainAllChannelsTo(c.gainDb)
            setLimiterAllChannelsTo(limiter(c))
        }.build()

    private class AndroidEffect(private val dp: DynamicsProcessing, private val bands: Int) : EngineEffect {
        override fun hasControl(): Boolean = dp.hasControl()
        override fun isEnabled(): Boolean = dp.enabled
        override fun enable(): Int = dp.setEnabled(true)
        override fun apply(c: FilterConfig) {
            dp.setPreEqAllChannelsTo(equalizer(bands, c))
            dp.setInputGainAllChannelsTo(c.gainDb)
            dp.setLimiterAllChannelsTo(limiter(c))
        }
        override fun listen(onChange: (Boolean) -> Unit) {
            dp.setControlStatusListener { _, granted -> onChange(granted) }
            dp.setEnableStatusListener { _, _ -> onChange(false) }
        }
        override fun release() {
            try {
                dp.setControlStatusListener(null)
                dp.setEnableStatusListener(null)
            } finally {
                dp.release()
            }
        }
    }
}
