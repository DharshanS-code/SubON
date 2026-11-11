package app.subon

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SubOnEngineTest {
    private class FakeEffect : EngineEffect {
        var controlled = true
        var enabled = false
        var enableResult = 0
        var rejectUpdates = false
        var throwOnControl = false
        var released = false
        var updates = 0
        var applied = FilterConfig()
        var listener: ((Boolean) -> Unit)? = null

        override fun hasControl(): Boolean {
            check(!throwOnControl) { "control query failed" }
            return controlled
        }
        override fun isEnabled() = enabled
        override fun enable(): Int {
            if (enableResult == 0) enabled = true
            return enableResult
        }
        override fun apply(c: FilterConfig) {
            check(!rejectUpdates) { "update failed" }
            updates++
            applied = c
        }
        override fun listen(onChange: (Boolean) -> Unit) { listener = onChange }
        override fun release() { released = true }
    }

    private class FakeFactory : EngineEffectFactory {
        val attempts = mutableListOf<Pair<Int, Int>>()
        val effects = mutableListOf<FakeEffect>()
        var failGlobal = false
        var maxBands = 64
        var configure: (FakeEffect) -> Unit = {}
        override fun create(session: Int, bands: Int, config: FilterConfig): EngineEffect {
            attempts += session to bands
            check(!(failGlobal && session == 0) && bands <= maxBands) { "unsupported" }
            return FakeEffect().also { it.applied = config; configure(it); effects += it }
        }
    }

    @Test
    fun `update applies in place and ignores unchanged config`() {
        val factory = FakeFactory()
        val engine = SubOnEngine(factory)
        assertEquals(EffectState.ACTIVE, engine.start(FilterConfig()).state)
        val effect = factory.effects.single()
        val initialUpdates = effect.updates
        engine.update(FilterConfig())
        assertEquals(initialUpdates, effect.updates)
        engine.update(FilterConfig(gainDb = -4f))
        assertEquals(initialUpdates + 1, effect.updates)
        assertEquals(-4f, effect.applied.gainDb, 0f)
        assertEquals(1, factory.attempts.size)
        assertFalse(effect.released)
    }

    @Test
    fun `unsupported global waits and compatible session becomes active`() {
        val factory = FakeFactory().apply { failGlobal = true }
        val engine = SubOnEngine(factory)
        assertEquals(EffectState.WAITING, engine.start(FilterConfig()).state)
        engine.onSessionOpen(42)
        assertEquals(EffectState.ACTIVE, engine.status().state)
        engine.onSessionOpen(42)
        assertEquals(1, factory.effects.size)
        engine.onSessionClose(42)
        assertTrue(factory.effects.single().released)
        assertEquals(EffectState.WAITING, engine.status().state)
    }

    @Test
    fun `fewer bands are tried when device rejects high counts`() {
        val factory = FakeFactory().apply { maxBands = 24 }
        val engine = SubOnEngine(factory)
        assertEquals(EffectState.ACTIVE, engine.start(FilterConfig()).state)
        assertEquals(listOf(0 to 64, 0 to 40, 0 to 24), factory.attempts)
    }

    @Test
    fun `enable failure never reports active and releases failed effects`() {
        val factory = FakeFactory().apply { configure = { it.enableResult = -5 } }
        val engine = SubOnEngine(factory)
        assertEquals(EffectState.WAITING, engine.start(FilterConfig()).state)
        assertEquals(3, factory.effects.size)
        assertTrue(factory.effects.all { it.released })
    }

    @Test
    fun `uncontrolled or throwing effects are released during creation`() {
        for (configure in listOf<(FakeEffect) -> Unit>(
            { it.controlled = false },
            { it.throwOnControl = true }
        )) {
            val factory = FakeFactory().apply { this.configure = configure }
            val engine = SubOnEngine(factory)
            assertEquals(EffectState.WAITING, engine.start(FilterConfig()).state)
            assertTrue(factory.effects.all { it.released })
        }
    }

    @Test
    fun `session opens ignored while off and invalid session IDs never reach platform`() {
        val factory = FakeFactory().apply { failGlobal = true }
        val engine = SubOnEngine(factory)
        engine.onSessionOpen(17)
        assertTrue(factory.attempts.isEmpty())
        engine.start(FilterConfig())
        engine.onSessionOpen(-1)
        engine.onSessionOpen(0)
        assertEquals(3, factory.attempts.size)
    }

    @Test
    fun `control loss and regain publish status and restore latest settings`() {
        var changes = 0
        val factory = FakeFactory()
        val engine = SubOnEngine(factory) { changes++ }
        engine.start(FilterConfig())
        val effect = factory.effects.single()
        effect.controlled = false
        effect.listener!!.invoke(false)
        assertEquals(EffectState.WAITING, engine.status().state)
        engine.update(FilterConfig(gainDb = -8f))
        effect.controlled = true
        effect.listener!!.invoke(true)
        assertEquals(-8f, effect.applied.gainDb, 0f)
        assertEquals(EffectState.ACTIVE, engine.status().state)
        assertTrue(changes >= 2)
    }

    @Test
    fun `disabled effect and failed control query are not reported active`() {
        val factory = FakeFactory()
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        val effect = factory.effects.single()
        effect.enabled = false
        assertEquals(EffectState.WAITING, engine.status().state)
        effect.throwOnControl = true
        assertEquals(EffectState.WAITING, engine.status().state)
    }

    @Test
    fun `failed runtime update releases stale effect and reports failure`() {
        val factory = FakeFactory().apply { failGlobal = true }
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        engine.onSessionOpen(7)
        factory.effects.single().rejectUpdates = true
        engine.update(FilterConfig(gainDb = -3f))
        assertTrue(factory.effects.first().released)
        assertEquals(EffectState.FAILED, engine.status().state)
        engine.update(FilterConfig(gainDb = -6f))
        assertEquals(EffectState.ACTIVE, engine.status().state)
        assertEquals(-6f, factory.effects.last().applied.gainDb, 0f)
    }

    @Test
    fun `stop rejects late updates callbacks and session opens until restarted`() {
        val factory = FakeFactory()
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        val effect = factory.effects.single()
        engine.stop()
        engine.update(FilterConfig(gainDb = 6f))
        engine.onSessionOpen(99)
        effect.listener!!.invoke(true)
        assertTrue(effect.released)
        assertEquals(1, factory.effects.size)
        assertEquals(EffectState.OFF, engine.status().state)
        engine.start(FilterConfig(gainDb = -2f))
        val oldUpdates = effect.updates
        effect.listener!!.invoke(true)
        assertEquals(oldUpdates, effect.updates)
        assertEquals(2, factory.effects.size)
        assertEquals(EffectState.ACTIVE, engine.status().state)
    }

    @Test
    fun `lost global control can fall back to a session without stacking effects`() {
        val factory = FakeFactory()
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        val global = factory.effects.single()
        global.controlled = false
        engine.onSessionOpen(14)
        assertTrue(global.released)
        assertEquals(2, factory.effects.size)
        assertEquals(EffectState.ACTIVE, engine.status().state)
        global.controlled = true
        val updates = global.updates
        global.listener!!.invoke(true)
        assertEquals(updates, global.updates)
    }

    @Test
    fun `retrying same settings recovers an effect lost to a runtime error`() {
        val factory = FakeFactory()
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        factory.effects.single().rejectUpdates = true
        val next = FilterConfig(gainDb = -3f)
        engine.update(next)
        assertEquals(EffectState.FAILED, engine.status().state)
        engine.update(next)
        assertEquals(EffectState.ACTIVE, engine.status().state)
        assertEquals(next, factory.effects.last().applied)
    }

    @Test
    fun `same settings retry retains already playing sessions without global support`() {
        val factory = FakeFactory().apply { failGlobal = true }
        val engine = SubOnEngine(factory)
        engine.start(FilterConfig())
        engine.onSessionOpen(7)
        factory.effects.single().rejectUpdates = true
        val next = FilterConfig(gainDb = -3f)
        engine.update(next)
        assertEquals(EffectState.FAILED, engine.status().state)
        engine.update(next)
        assertEquals(EffectState.ACTIVE, engine.status().state)
        assertEquals(next, factory.effects.last().applied)
    }

    @Test
    fun `global attachment never claims guaranteed system wide coverage`() {
        val engine = SubOnEngine(FakeFactory())
        val detail = engine.start(FilterConfig()).detail
        assertFalse(detail.contains("System-wide", ignoreCase = true))
        assertTrue(detail.contains("depends"))
    }
}
