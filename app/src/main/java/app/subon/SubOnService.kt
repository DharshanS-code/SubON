package app.subon

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.media.audiofx.AudioEffect
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import kotlin.math.roundToInt
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.launch

class SubOnService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var engine: SubOnEngine? = null
    private var watchJob: Job? = null
    private var receiver: BroadcastReceiver? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, "SubON filter", NotificationManager.IMPORTANCE_LOW)
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            shutdown()
            return START_NOT_STICKY
        }
        startEffect()
        return START_NOT_STICKY
    }

    private fun startEffect() {
        val cfg = SubOnState.config.value
        val notification = buildNotification(cfg)
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIF_ID, notification)
        }
        if (engine != null) return

        val eng = SubOnEngine()
        engine = eng
        SubOnState.status.value = eng.start(cfg)
        registerSessionReceiver()

        watchJob?.cancel()
        watchJob = scope.launch {
            SubOnState.config.drop(1).collect { c ->
                eng.update(c)
                getSystemService(NotificationManager::class.java)
                    .notify(NOTIF_ID, buildNotification(c))
            }
        }
    }

    private fun registerSessionReceiver() {
        if (receiver != null) return
        val r = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                if (intent == null) return
                val eng = engine ?: return
                val session = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, -1)
                when (intent.action) {
                    AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION -> eng.onSessionOpen(session)
                    AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION -> eng.onSessionClose(session)
                }
                SubOnState.status.value = eng.status()
            }
        }
        val filter = IntentFilter().apply {
            addAction(AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION)
            addAction(AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION)
        }
        ContextCompat.registerReceiver(this, r, filter, ContextCompat.RECEIVER_EXPORTED)
        receiver = r
    }

    private fun buildNotification(c: FilterConfig): Notification {
        val stop = PendingIntent.getService(
            this, 0,
            Intent(this, SubOnService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_IMMUTABLE
        )
        val open = PendingIntent.getActivity(
            this, 1,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_stat_subon)
            .setContentTitle("SubON is on")
            .setContentText("${c.lowHz.roundToInt()} to ${c.highHz.roundToInt()} Hz")
            .setContentIntent(open)
            .addAction(0, "Turn off", stop)
            .setOngoing(true)
            .build()
    }

    private fun cleanup() {
        watchJob?.cancel()
        watchJob = null
        receiver?.let {
            try {
                unregisterReceiver(it)
            } catch (e: Exception) {
                // ignore
            }
        }
        receiver = null
        engine?.stop()
        engine = null
        SubOnState.status.value = EngineStatus(EffectState.OFF)
    }

    private fun shutdown() {
        cleanup()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        cleanup()
        scope.cancel()
        super.onDestroy()
    }

    companion object {
        const val ACTION_START = "app.subon.START"
        const val ACTION_STOP = "app.subon.STOP"
        private const val CHANNEL_ID = "subon_filter"
        private const val NOTIF_ID = 1
    }
}
