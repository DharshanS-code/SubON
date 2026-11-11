package app.subon

import android.content.Context

/** Decode a single preference snapshot without typed getters throwing on corrupt data. */
internal object SavedConfig {
    fun decode(values: Map<String, *>, requestedProfile: OutputProfile? = null): FilterConfig {
        val profile = requestedProfile ?: OutputProfile.values()
            .firstOrNull { it.name == values["profile"] } ?: OutputProfile.WIRED
        val prefix = profile.name
        val steepness = Steepness.values()
            .firstOrNull { it.name == values["${prefix}_steep"] } ?: Steepness.STEEP
        return FilterConfig(
            lowHz = values["${prefix}_low"] as? Float ?: profile.defaultLow,
            highHz = values["${prefix}_high"] as? Float ?: profile.defaultHigh,
            steepness = steepness,
            limiterOn = values["${prefix}_limiter"] as? Boolean ?: true,
            gainDb = values["${prefix}_gain"] as? Float ?: 0f,
            profile = profile
        ).normalized()
    }
}

class Prefs(context: Context) {
    private val sp = context.getSharedPreferences("subon", Context.MODE_PRIVATE)

    fun load(): FilterConfig = SavedConfig.decode(sp.all)

    fun loadProfile(p: OutputProfile): FilterConfig = SavedConfig.decode(sp.all, p)

    fun save(c: FilterConfig) {
        val safe = c.normalized()
        val p = safe.profile.name
        sp.edit()
            .putString("profile", p)
            .putFloat("${p}_low", safe.lowHz)
            .putFloat("${p}_high", safe.highHz)
            .putString("${p}_steep", safe.steepness.name)
            .putBoolean("${p}_limiter", safe.limiterOn)
            .putFloat("${p}_gain", safe.gainDb)
            .apply()
    }
}
