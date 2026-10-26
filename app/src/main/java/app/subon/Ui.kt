package app.subon

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.RangeSlider
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.exp
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
    val isOn = status.state != EffectState.OFF

    Surface(modifier = Modifier.fillMaxSize(), color = Zaffre, contentColor = Lavender) {
        Column(Modifier.fillMaxSize().systemBarsPadding()) {
            Column(
                Modifier
                    .weight(1f)
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 20.dp)
            ) {
                Spacer(Modifier.height(12.dp))
                Header(cfg, status)
                Spacer(Modifier.height(16.dp))

                Box(
                    Modifier
                        .fillMaxWidth()
                        .height(176.dp)
                        .clip(RoundedCornerShape(20.dp))
                        .background(ZaffreDark)
                ) {
                    ResponseGraph(cfg, Modifier.fillMaxSize())
                }
                Spacer(Modifier.height(16.dp))

                RangeSection(cfg) { low, high -> onChange { it.withRange(low, high) } }
                Spacer(Modifier.height(8.dp))

                PresetRow(cfg) { p ->
                    if (p != Preset.CUSTOM) onChange { it.withRange(p.low, p.high) }
                }
                Spacer(Modifier.height(20.dp))

                Text("Steepness", color = Muted, fontSize = 12.5.sp)
                Spacer(Modifier.height(8.dp))
                SteepnessRow(cfg) { s -> onChange { it.copy(steepness = s) } }
                Spacer(Modifier.height(20.dp))

                LimiterRow(cfg) { on -> onChange { it.copy(limiterOn = on) } }
                Spacer(Modifier.height(12.dp))
                GainRow(cfg) { g -> onChange { it.copy(gainDb = g) } }
                Spacer(Modifier.height(12.dp))
                OutputRow(cfg, onProfile)
                Spacer(Modifier.height(8.dp))

                val from = cfg.profile.usableFromHz
                val name = cfg.profile.label.lowercase()
                val hint = when {
                    cfg.highHz < from ->
                        "Your whole range is below what $name can play (about $from Hz). Expect near silence."
                    cfg.lowHz < from ->
                        "Part of your range is below what $name can play (about $from Hz)."
                    else -> "This range fits $name."
                }
                Text(
                    hint,
                    color = if (cfg.lowHz < from) Warn else Citron,
                    fontSize = 12.sp
                )
                Spacer(Modifier.height(16.dp))
            }

            Box(Modifier.padding(horizontal = 24.dp, vertical = 12.dp)) {
                PowerSwitch(isOn) { onPower(!isOn) }
            }
        }
    }
}

@Composable
private fun Header(cfg: FilterConfig, status: EngineStatus) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        LogoMark(44.dp)
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(
                buildAnnotatedString {
                    withStyle(SpanStyle(color = Lavender)) { append("Sub") }
                    withStyle(SpanStyle(color = Citron)) { append("ON") }
                },
                fontSize = 31.sp,
                fontWeight = FontWeight.ExtraBold,
                letterSpacing = (-0.8).sp
            )
            Text(
                "${cfg.preset().label}, ${cfg.lowHz.roundToInt()} to ${cfg.highHz.roundToInt()} Hz",
                color = Muted,
                fontSize = 13.sp
            )
        }
        StatusPill(status)
    }
    if (status.state != EffectState.OFF && status.detail.isNotEmpty()) {
        Text(
            status.detail,
            color = Muted,
            fontSize = 12.sp,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}

@Composable
fun LogoMark(markSize: Dp) {
    Canvas(Modifier.size(markSize)) {
        val s = this.size.width / 44f
        fun pt(v: Float): Float = (v + 2f) * s
        drawRoundRect(Citron, cornerRadius = CornerRadius(13f * s, 13f * s))
        val path = Path()
        path.moveTo(pt(30f), pt(13f))
        path.cubicTo(pt(27f), pt(11f), pt(24f), pt(10f), pt(20f), pt(10f))
        path.cubicTo(pt(13f), pt(10f), pt(12f), pt(17f), pt(20f), pt(20f))
        path.cubicTo(pt(28f), pt(23f), pt(27f), pt(30f), pt(20f), pt(30f))
        path.cubicTo(pt(16f), pt(30f), pt(13f), pt(29f), pt(10f), pt(27f))
        drawPath(
            path,
            Zaffre,
            style = Stroke(width = 4.6f * s, cap = StrokeCap.Round, join = StrokeJoin.Round)
        )
    }
}

@Composable
private fun StatusPill(status: EngineStatus) {
    val (dot, label) = when (status.state) {
        EffectState.ACTIVE -> Citron to "Attached"
        EffectState.WAITING -> Periwinkle to "Waiting"
        EffectState.FAILED -> Warn to "Failed"
        EffectState.OFF -> Muted to "Off"
    }
    Row(
        Modifier
            .clip(RoundedCornerShape(50))
            .background(ZaffreDark)
            .padding(horizontal = 14.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            Modifier
                .size(9.dp)
                .clip(CircleShape)
                .background(dot)
        )
        Spacer(Modifier.width(8.dp))
        Text(label, fontSize = 12.5.sp, fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun ResponseGraph(cfg: FilterConfig, modifier: Modifier) {
    val measurer = rememberTextMeasurer()
    Canvas(modifier) {
        val left = 16.dp.toPx()
        val right = size.width - 16.dp.toPx()
        val top = 16.dp.toPx()
        val bottom = size.height - 30.dp.toPx()
        val w = right - left
        val labelStyle = TextStyle(color = Muted, fontSize = 10.sp)

        fun xOf(hz: Double): Float = left + (ln(hz / 20.0) / ln(25.0)).toFloat() * w
        fun yOf(db: Double): Float = top + ((6.0 - db) / 66.0).toFloat() * (bottom - top)

        val gridColor = Periwinkle.copy(alpha = 0.35f)
        for (db in listOf(0.0, -20.0, -40.0)) {
            val y = yOf(db)
            drawLine(gridColor, Offset(left, y), Offset(right, y), strokeWidth = 1.dp.toPx())
        }
        for (hz in listOf(20.0, 50.0, 100.0, 200.0, 500.0)) {
            val x = xOf(hz)
            drawLine(gridColor, Offset(x, top), Offset(x, bottom), strokeWidth = 1.dp.toPx())
            val text = if (hz == 20.0) "20 Hz" else hz.toInt().toString()
            val t = measurer.measure(text, labelStyle)
            val tx = (x - t.size.width / 2f).coerceIn(
                2.dp.toPx(),
                size.width - t.size.width - 2.dp.toPx()
            )
            drawText(t, topLeft = Offset(tx, bottom + 8.dp.toPx()))
        }

        val line = Path()
        val fill = Path()
        fill.moveTo(left, bottom)
        val steps = 160
        for (i in 0..steps) {
            val hz = 20.0 * 25.0.pow(i / steps.toDouble())
            val x = xOf(hz)
            val y = yOf(FilterMath.gainDb(hz, cfg))
            if (i == 0) line.moveTo(x, y) else line.lineTo(x, y)
            fill.lineTo(x, y)
        }
        fill.lineTo(right, bottom)
        fill.close()

        drawPath(
            fill,
            Brush.verticalGradient(
                listOf(Citron.copy(alpha = 0.45f), Citron.copy(alpha = 0.02f)),
                startY = top,
                endY = bottom
            )
        )
        drawPath(
            line,
            Citron,
            style = Stroke(width = 2.5.dp.toPx(), cap = StrokeCap.Round, join = StrokeJoin.Round)
        )

        val dash = PathEffect.dashPathEffect(floatArrayOf(8f, 8f))
        for (hz in listOf(cfg.lowHz.toDouble(), cfg.highHz.toDouble())) {
            val x = xOf(hz)
            drawLine(
                Citron.copy(alpha = 0.7f),
                Offset(x, top),
                Offset(x, bottom),
                strokeWidth = 1.dp.toPx(),
                pathEffect = dash
            )
            val c = Offset(x, yOf(FilterMath.gainDb(hz, cfg)))
            drawCircle(ZaffreDark, 5.dp.toPx(), c)
            drawCircle(Citron, 5.dp.toPx(), c, style = Stroke(width = 2.dp.toPx()))
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun RangeSection(cfg: FilterConfig, onRange: (Float, Float) -> Unit) {
    val span = ln(15.0)
    fun toPos(hz: Float): Float = (ln(hz / 20.0) / span).toFloat()
    fun toHz(p: Float): Float = (20.0 * exp(p.toDouble() * span)).toFloat()

    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Column {
            Text("Low edge", color = Muted, fontSize = 12.5.sp)
            Text("${cfg.lowHz.roundToInt()} Hz", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
        Column(horizontalAlignment = Alignment.End) {
            Text("High edge", color = Muted, fontSize = 12.5.sp)
            Text("${cfg.highHz.roundToInt()} Hz", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
    }
    RangeSlider(
        value = toPos(cfg.lowHz)..toPos(cfg.highHz),
        onValueChange = { r -> onRange(toHz(r.start), toHz(r.endInclusive)) },
        valueRange = 0f..1f,
        colors = SliderDefaults.colors(
            thumbColor = Lavender,
            activeTrackColor = Citron,
            inactiveTrackColor = Periwinkle
        )
    )
}

@Composable
private fun PresetRow(cfg: FilterConfig, onPreset: (Preset) -> Unit) {
    val current = cfg.preset()
    Row(
        Modifier.horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        for (p in Preset.values()) {
            val sel = p == current
            Box(
                Modifier
                    .clip(RoundedCornerShape(50))
                    .background(if (sel) Citron else ZaffreDark)
                    .clickable { onPreset(p) }
                    .padding(horizontal = 16.dp, vertical = 9.dp)
            ) {
                Text(
                    p.label,
                    color = if (sel) ZaffreDark else Lavender,
                    fontSize = 13.sp,
                    fontWeight = if (sel) FontWeight.Bold else FontWeight.Normal
                )
            }
        }
    }
}

@Composable
private fun SteepnessRow(cfg: FilterConfig, onSteep: (Steepness) -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(50))
            .background(ZaffreDark)
            .padding(4.dp)
    ) {
        for (s in Steepness.values()) {
            val sel = s == cfg.steepness
            Box(
                Modifier
                    .weight(1f)
                    .clip(RoundedCornerShape(50))
                    .background(if (sel) Lavender else Color.Transparent)
                    .clickable { onSteep(s) }
                    .padding(vertical = 9.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    s.label,
                    color = if (sel) Zaffre else Muted,
                    fontSize = 13.5.sp,
                    fontWeight = if (sel) FontWeight.Bold else FontWeight.Normal
                )
            }
        }
    }
}

@Composable
private fun LimiterRow(cfg: FilterConfig, onToggle: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text("Limiter", fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
            Text("Prevents distortion at high gain", color = Muted, fontSize = 11.5.sp)
        }
        Switch(
            checked = cfg.limiterOn,
            onCheckedChange = onToggle,
            colors = SwitchDefaults.colors(
                checkedThumbColor = ZaffreDark,
                checkedTrackColor = Citron,
                checkedBorderColor = Citron,
                uncheckedThumbColor = Lavender,
                uncheckedTrackColor = ZaffreDark,
                uncheckedBorderColor = Periwinkle
            )
        )
    }
}

@Composable
private fun GainRow(cfg: FilterConfig, onGain: (Float) -> Unit) {
    val g = cfg.gainDb.roundToInt()
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("Output gain", fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
        Text(if (g > 0) "+$g dB" else "$g dB", fontSize = 15.sp, fontWeight = FontWeight.Bold)
    }
    Slider(
        value = cfg.gainDb,
        onValueChange = { onGain(it.roundToInt().toFloat()) },
        valueRange = -12f..6f,
        steps = 17,
        colors = SliderDefaults.colors(
            thumbColor = Lavender,
            activeTrackColor = Citron,
            inactiveTrackColor = Periwinkle,
            activeTickColor = Color.Transparent,
            inactiveTickColor = Color.Transparent
        )
    )
}

@Composable
private fun OutputRow(cfg: FilterConfig, onProfile: (OutputProfile) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box(Modifier.fillMaxWidth()) {
        Row(
            Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(12.dp))
                .background(ZaffreDark)
                .clickable { open = true }
                .padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Output", color = Muted, fontSize = 13.5.sp)
            Spacer(Modifier.width(12.dp))
            Text(
                cfg.profile.label,
                fontSize = 13.5.sp,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.weight(1f)
            )
            Text("\u25BE", color = Muted, fontSize = 14.sp)
        }
        DropdownMenu(
            expanded = open,
            onDismissRequest = { open = false },
            modifier = Modifier.background(ZaffreDark)
        ) {
            for (p in OutputProfile.values()) {
                DropdownMenuItem(
                    text = { Text(p.label, color = Lavender) },
                    onClick = {
                        open = false
                        onProfile(p)
                    }
                )
            }
        }
    }
}

@Composable
private fun PowerSwitch(on: Boolean, onToggle: () -> Unit) {
    val bg = if (on) Citron else Periwinkle
    val iconColor = if (on) Citron else Lavender
    Row(
        Modifier
            .fillMaxWidth()
            .height(64.dp)
            .clip(RoundedCornerShape(32.dp))
            .background(bg)
            .clickable(onClick = onToggle)
            .padding(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (on) {
            Text(
                "Filter on",
                color = ZaffreDark,
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 20.dp)
            )
            PowerKnob(iconColor)
        } else {
            PowerKnob(iconColor)
            Text(
                "Filter off",
                color = ZaffreDark,
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.End,
                modifier = Modifier
                    .weight(1f)
                    .padding(end = 20.dp)
            )
        }
    }
}

@Composable
private fun PowerKnob(iconColor: Color) {
    Box(
        Modifier
            .size(48.dp)
            .clip(CircleShape)
            .background(Zaffre),
        contentAlignment = Alignment.Center
    ) {
        Canvas(Modifier.size(22.dp)) {
            val s = size.minDimension
            val stroke = s * 0.12f
            val d = s * 0.78f
            drawArc(
                iconColor,
                startAngle = -60f,
                sweepAngle = 300f,
                useCenter = false,
                topLeft = Offset((s - d) / 2f, s - d - stroke / 2f),
                size = Size(d, d),
                style = Stroke(width = stroke, cap = StrokeCap.Round)
            )
            drawLine(
                iconColor,
                Offset(s / 2f, stroke / 2f),
                Offset(s / 2f, s * 0.48f),
                strokeWidth = stroke,
                cap = StrokeCap.Round
            )
        }
    }
}
