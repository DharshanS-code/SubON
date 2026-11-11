package app.subon

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.roundToInt

@Composable
fun SubOnScreen(
    onPower: (Boolean) -> Unit,
    onChange: ((FilterConfig) -> FilterConfig) -> Unit,
    onProfile: (OutputProfile) -> Unit
) {
    val cfg by SubOnState.config.collectAsState()
    val status by SubOnState.status.collectAsState()
    var confirmReset by rememberSaveable { mutableStateOf(false) }
    Surface(Modifier.fillMaxSize(), color = Zaffre, contentColor = Lavender) {
        BoxWithConstraints(Modifier.fillMaxSize().safeDrawingPadding()) {
            val wide = maxWidth >= 840.dp
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState())
                    .padding(if (wide) 32.dp else 16.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Column(Modifier.widthIn(max = 1120.dp).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(20.dp)) {
                    Header(status)
                    if (wide) {
                        Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                            Column(Modifier.weight(1.2f), verticalArrangement = Arrangement.spacedBy(20.dp)) {
                                ResponseSection(cfg)
                                FilterSection(cfg, onChange)
                            }
                            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(20.dp)) {
                                OutputSection(cfg, onChange, onProfile) { confirmReset = true }
                                EngineSection(status, onPower)
                            }
                        }
                    } else {
                        ResponseSection(cfg)
                        FilterSection(cfg, onChange)
                        OutputSection(cfg, onChange, onProfile) { confirmReset = true }
                        EngineSection(status, onPower)
                    }
                    Text(
                        "Start at a comfortable volume. An attached effect does not confirm audible or system-wide processing.",
                        color = Muted, style = MaterialTheme.typography.bodySmall
                    )
                }
            }
        }
    }
    if (confirmReset) {
        AlertDialog(
            onDismissRequest = { confirmReset = false },
            title = { Text("Reset ${cfg.profile.label.lowercase()}?") },
            text = { Text("Restore this profile's range, steepness, gain and limiter defaults. Other profiles stay unchanged.") },
            confirmButton = {
                TextButton(onClick = {
                    onChange { FilterConfig(lowHz = it.profile.defaultLow, highHz = it.profile.defaultHigh, profile = it.profile) }
                    confirmReset = false
                }) { Text("Reset profile") }
            },
            dismissButton = { TextButton(onClick = { confirmReset = false }) { Text("Cancel") } }
        )
    }
}

@Composable
private fun Header(status: EngineStatus) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("SubON", fontSize = 32.sp, fontWeight = FontWeight.Bold, letterSpacing = (-1).sp,
            modifier = Modifier.semantics { heading() })
        Text("Shape your sound", color = Muted, style = MaterialTheme.typography.bodyLarge)
        val label = when (status.state) {
            EffectState.OFF -> "Off"
            EffectState.ACTIVE -> "Effect attached"
            EffectState.WAITING -> "Waiting for audio"
            EffectState.FAILED -> "Needs attention"
        }
        Surface(shape = RoundedCornerShape(8.dp), color = ZaffreDark) {
            Text(label, Modifier.padding(horizontal = 12.dp, vertical = 8.dp).semantics {
                liveRegion = LiveRegionMode.Polite
            }, color = if (status.state == EffectState.FAILED) Warn else Lavender,
                style = MaterialTheme.typography.labelLarge)
        }
    }
}

@Composable
private fun Section(title: String, subtitle: String, content: @Composable ColumnScope.() -> Unit) {
    Surface(Modifier.fillMaxWidth(), shape = RoundedCornerShape(20.dp), color = ZaffreDark) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold,
                modifier = Modifier.semantics { heading() })
            Text(subtitle, color = Muted, style = MaterialTheme.typography.bodySmall)
            content()
        }
    }
}

@Composable
private fun ResponseSection(cfg: FilterConfig) {
    var showValues by rememberSaveable { mutableStateOf(false) }
    Section("Frequency response", "Calculated filter + gain · not a live measurement") {
        Text("${cfg.lowHz.roundToInt()}–${cfg.highHz.roundToInt()} Hz", style = MaterialTheme.typography.headlineMedium)
        Text("Level (dB) / frequency (Hz) · excludes limiter dynamics", color = Muted,
            style = MaterialTheme.typography.bodySmall)
        ResponseGraph(cfg, Modifier.fillMaxWidth().height(210.dp))
        TextButton(onClick = { showValues = !showValues }, modifier = Modifier.heightIn(min = 48.dp)) {
            Text(if (showValues) "Hide response values" else "Show response values")
        }
        if (showValues) {
            Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                for (hz in listOf(20, 50, 100, 200, 500)) {
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("$hz Hz", style = MaterialTheme.typography.bodyMedium)
                        Text("${(FilterMath.gainDb(hz.toDouble(), cfg) + cfg.gainDb).roundToInt()} dB",
                            style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun FilterSection(cfg: FilterConfig, onChange: ((FilterConfig) -> FilterConfig) -> Unit) {
    Section("Filter range", "Keep the low end you want. Each edge is adjustable independently.") {
        LabeledSlider("Low edge", "${cfg.lowHz.roundToInt()} Hz", cfg.lowHz,
            Limits.LOW_MIN..minOf(Limits.LOW_MAX, cfg.highHz - Limits.MIN_GAP)) { low ->
            onChange { it.withRange(low, it.highHz) }
        }
        LabeledSlider("High edge", "${cfg.highHz.roundToInt()} Hz", cfg.highHz,
            maxOf(Limits.HIGH_MIN, cfg.lowHz + Limits.MIN_GAP)..Limits.HIGH_MAX) { high ->
            onChange { it.withRange(it.lowHz, high) }
        }
        Text("Preset · ${cfg.preset().label}", color = Muted, style = MaterialTheme.typography.labelLarge)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            for (preset in Preset.values().filter { it != Preset.CUSTOM }) {
                FilterChip(selected = cfg.preset() == preset,
                    onClick = { onChange { it.withRange(preset.low, preset.high) } },
                    label = { Text(preset.label) }, modifier = Modifier.heightIn(min = 48.dp))
            }
        }
        Text("Steepness", style = MaterialTheme.typography.titleSmall)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            for (steepness in Steepness.values()) {
                FilterChip(selected = cfg.steepness == steepness,
                    onClick = { onChange { it.copy(steepness = steepness) } },
                    label = { Text(steepness.label) }, modifier = Modifier.heightIn(min = 48.dp))
            }
        }
    }
}

@Composable
private fun LabeledSlider(label: String, valueText: String, value: Float, range: ClosedFloatingPointRange<Float>, onChange: (Float) -> Unit) {
    Column {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
            Text(valueText, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.bodyLarge)
        }
        Slider(value = value, onValueChange = { onChange(it.roundToInt().toFloat()) }, valueRange = range,
            steps = (range.endInclusive - range.start).roundToInt().minus(1).coerceAtLeast(0),
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp).semantics {
                contentDescription = label
                stateDescription = valueText
            }, colors = SliderDefaults.colors(thumbColor = Citron, activeTrackColor = Citron, inactiveTrackColor = Periwinkle))
    }
}

@Composable
private fun OutputSection(cfg: FilterConfig, onChange: ((FilterConfig) -> FilterConfig) -> Unit,
                          onProfile: (OutputProfile) -> Unit, onReset: () -> Unit) {
    Section("Output & protection", "Settings are saved separately for each output profile.") {
        var menuOpen by remember { mutableStateOf(false) }
        Box {
            OutlinedButton(onClick = { menuOpen = true }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                Text("Profile: ${cfg.profile.label}")
            }
            DropdownMenu(expanded = menuOpen, onDismissRequest = { menuOpen = false }) {
                for (profile in OutputProfile.values()) {
                    DropdownMenuItem(text = { Text(profile.label) }, onClick = {
                        menuOpen = false
                        onProfile(profile)
                    }, modifier = Modifier.semantics { selected = profile == cfg.profile })
                }
            }
        }
        Text("Profiles are starting points, not hardware detection. Actual bass extension depends on your device.",
            color = Muted, style = MaterialTheme.typography.bodySmall)
        LabeledSlider("Output gain", "${if (cfg.gainDb > 0) "+" else ""}${cfg.gainDb.roundToInt()} dB",
            cfg.gainDb, -12f..6f) { gain -> onChange { it.copy(gainDb = gain) } }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Limiter", style = MaterialTheme.typography.titleSmall)
                Text("Helps control peaks; does not guarantee distortion-free or safe listening.",
                    color = Muted, style = MaterialTheme.typography.bodySmall)
            }
            Switch(checked = cfg.limiterOn, onCheckedChange = { on -> onChange { it.copy(limiterOn = on) } },
                modifier = Modifier.semantics { contentDescription = "Limiter" })
        }
        if (cfg.gainDb > 0f) {
            Text("Positive gain can increase clipping and listening volume. Start low.",
                color = Warn, style = MaterialTheme.typography.bodySmall)
        }
        TextButton(onClick = onReset, modifier = Modifier.heightIn(min = 48.dp)) { Text("Reset this profile") }
    }
}

@Composable
private fun EngineSection(status: EngineStatus, onPower: (Boolean) -> Unit) {
    val running = status.state == EffectState.ACTIVE || status.state == EffectState.WAITING
    Section("Audio engine", "Android audio effects · device and session dependent") {
        Text(status.detail.ifBlank { "Enable the engine, then start playback in a compatible audio app." },
            color = if (status.state == EffectState.FAILED) Warn else Lavender,
            style = MaterialTheme.typography.bodyMedium, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite })
        Button(onClick = { onPower(!running) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            shape = RoundedCornerShape(12.dp)) {
            Text(if (running) "Turn off engine" else if (status.state == EffectState.FAILED) "Retry audio engine" else "Enable audio engine")
        }
        // A failed update leaves the foreground service alive for recovery.
        // Always offer a way to stop it, including when notifications are denied.
        if (status.state == EffectState.FAILED) {
            OutlinedButton(onClick = { onPower(false) }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
                Text("Turn off engine")
            }
        }
    }
}

@Composable
private fun ResponseGraph(cfg: FilterConfig, modifier: Modifier) {
    val measurer = rememberTextMeasurer()
    Canvas(modifier.semantics {
        contentDescription = "Calculated response from 20 to 500 hertz, minus 72 to plus 6 decibels. " +
            "Pass band ${cfg.lowHz.roundToInt()} to ${cfg.highHz.roundToInt()} hertz; gain ${cfg.gainDb.roundToInt()} decibels. " +
            "Use Show response values for the numeric values."
    }) {
        val labelStyle = TextStyle(color = Muted, fontSize = 10.sp)
        val labelWidth = measurer.measure("-72", labelStyle).size.width.toFloat()
        val labelHeight = measurer.measure("500 Hz", labelStyle).size.height.toFloat()
        val left = labelWidth + 10.dp.toPx()
        val right = size.width - 14.dp.toPx()
        val top = labelHeight / 2 + 4.dp.toPx()
        val bottom = size.height - labelHeight - 14.dp.toPx()
        if (right <= left || bottom <= top) return@Canvas
        fun xOf(hz: Double) = left + (ln(hz / 20.0) / ln(25.0)).toFloat() * (right - left)
        fun yOf(db: Double) = top + ((6.0 - db.coerceIn(-72.0, 6.0)) / 78.0).toFloat() * (bottom - top)
        for (db in listOf(0, -24, -48, -72)) {
            val y = yOf(db.toDouble())
            drawLine(Periwinkle, Offset(left, y), Offset(right, y), strokeWidth = 1.dp.toPx())
            val label = measurer.measure(db.toString(), labelStyle)
            drawText(label, topLeft = Offset(left - label.size.width - 6.dp.toPx(), y - label.size.height / 2))
        }
        // Reduce tick density when system font scaling leaves too little room.
        val ticks = if (right - left < measurer.measure("500 Hz", labelStyle).size.width * 5)
            listOf(20, 100, 500) else listOf(20, 50, 100, 200, 500)
        for (hz in ticks) {
            val x = xOf(hz.toDouble())
            val label = measurer.measure(hz.toString(), labelStyle)
            drawText(label, topLeft = Offset((x - label.size.width / 2).coerceIn(0f, (size.width - label.size.width).coerceAtLeast(0f)), bottom + 8.dp.toPx()))
        }
        val line = Path()
        for (i in 0..160) {
            val hz = 20.0 * 25.0.pow(i / 160.0)
            val x = xOf(hz)
            val y = yOf(FilterMath.gainDb(hz, cfg) + cfg.gainDb)
            if (i == 0) line.moveTo(x, y) else line.lineTo(x, y)
        }
        drawPath(line, ResponseLine, style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round))
        for (hz in listOf(cfg.lowHz.toDouble(), cfg.highHz.toDouble())) {
            val x = xOf(hz)
            drawLine(Muted, Offset(x, top), Offset(x, bottom), strokeWidth = 1.dp.toPx(),
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx())))
            val point = Offset(x, yOf(FilterMath.gainDb(hz, cfg) + cfg.gainDb))
            drawCircle(ZaffreDark, 6.dp.toPx(), point)
            drawCircle(ResponseLine, 4.dp.toPx(), point)
        }
    }
}
