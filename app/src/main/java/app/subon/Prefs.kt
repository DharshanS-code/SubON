package app.subon

import android.content.Context

class Prefs(context: Context) {
    private val sp = context.getSharedPreferences("subon", Context.MODE_PRIVATE)

    fun load(): FilterConfig {
        val name = sp.getString("profile", null)
        val profile = OutputProfile.values().firstOrNull { it.name == name } ?: OutputProfile.WIRED
        return loadProfile(profile)
    }

    fun loadProfile(p: OutputProfile): FilterConfig {
        val steepName = sp.getString("${p.name}_steep", null)
        val steep = Steepness.values().firstOrNull { it.name == steepName } ?: Steepness.STEEP
        return FilterConfig(
            lowHz = sp.getFloat("${p.name}_low", p.defaultLow),
            highHz = sp.getFloat("${p.name}_high", p.defaultHigh),
            steepness = steep,
            limiterOn = sp.getBoolean("${p.name}_limiter", true),
            gainDb = sp.getFloat("${p.name}_gain", 0f),
            profile = p
        )
    }

    fun save(c: FilterConfig) {
        val p = c.profile.name
        sp.edit()
            .putString("profile", p)
            .putFloat("${p}_low", c.lowHz)
            .putFloat("${p}_high", c.highHz)
            .putString("${p}_steep", c.steepness.name)
            .putBoolean("${p}_limiter", c.limiterOn)
            .putFloat("${p}_gain", c.gainDb)
            .apply()
    }
}
