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
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import kotlin.math.roundToInt
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch

class SubOnService : Service() {
    // Lifecycle, broadcasts, effect callbacks and config changes share one queue. A cancelled old
    // collector can no longer recreate an effect or notification after stop / restart.
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var engine: SubOnEngine? = null
    private var watchJob: Job? = null
    private var receiver: BroadcastReceiver? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        SubOnState.ensureLoaded { Prefs(this).load() }
        getSystemService(NotificationManager::class.java).createNotificationChannel(
            NotificationChannel(CHANNEL_ID, "SubON filter", NotificationManager.IMPORTANCE_LOW)
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> shutdown(startId)
            ACTION_START -> {
                try {
                    startEffect()
                } catch (e: RuntimeException) {
                    Log.e(TAG, "Unable to start the filter service", e)
                    shutdown(startId)
                    SubOnState.status.value = EngineStatus(EffectState.OFF, "Could not start the filter: ${e.message ?: e.javaClass.simpleName}")
                }
            }
            else -> if (engine == null) stopSelf(startId)
        }
        return START_NOT_STICKY
    }

    private fun startEffect() {
        val cfg = SubOnState.config.value
        val starting = engine?.status() ?: EngineStatus(EffectState.WAITING, "Checking audio effect support…")
        val notification = buildNotification(cfg, starting)
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIF_ID, notification)
        }
        engine?.let { current ->
            // Keep session IDs: players may not broadcast OPEN again while already playing.
            // update retries failed handles even if the configuration is unchanged.
            current.update(cfg)
            publishStatus(current)
            return
        }

        lateinit var eng: SubOnEngine
        eng = SubOnEngine {
            scope.launch {
                if (engine === eng) publishStatus(eng)
            }
        }
        engine = eng
        registerSessionReceiver()
        eng.start(cfg)
        publishStatus(eng)

        watchJob = scope.launch {
            // StateFlow replays the latest config. Do not drop it: it may have changed since start.
            SubOnState.config.collect { c ->
                if (engine === eng) {
                    eng.update(c)
                    publishStatus(eng)
                }
            }
        }
    }

    private fun publishStatus(eng: SubOnEngine) {
        val status = eng.status()
        SubOnState.status.value = status
        try {
            getSystemService(NotificationManager::class.java)
                .notify(NOTIF_ID, buildNotification(SubOnState.config.value, status))
        } catch (e: RuntimeException) {
            Log.w(TAG, "Unable to refresh filter notification", e)
        }
    }

    private fun registerSessionReceiver() {
        if (receiver != null) return
        val r = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                if (intent == null) return
                val eng = engine ?: return
                val session = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, -1)
                if (session <= 0) return
                when (intent.action) {
                    AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION -> eng.onSessionOpen(session)
                    AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION -> eng.onSessionClose(session)
                    else -> return
                }
                publishStatus(eng)
            }
        }
        val filter = IntentFilter().apply {
            addAction(AudioEffect.ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION)
            addAction(AudioEffect.ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION)
        }
        // Other players send this platform-defined opt-in broadcast; it must be exported.
        ContextCompat.registerReceiver(this, r, filter, ContextCompat.RECEIVER_EXPORTED)
        receiver = r
    }

    private fun buildNotification(c: FilterConfig, status: EngineStatus): Notification {
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
        val title = when (status.state) {
            EffectState.ACTIVE -> "SubON · effect attached"
            EffectState.WAITING -> "SubON · waiting for audio support"
            EffectState.FAILED -> "SubON · effect needs attention"
            EffectState.OFF -> "SubON · off"
        }
        val text = "${c.lowHz.roundToInt()} to ${c.highHz.roundToInt()} Hz · ${status.detail}"
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_stat_subon)
            .setContentTitle(title)
            .setContentText(text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(text))
            .setContentIntent(open)
            .addAction(0, "Turn off", stop)
            .setOnlyAlertOnce(true)
            .setOngoing(true)
            .build()
    }

    private fun cleanup() {
        watchJob?.cancel()
        watchJob = null
        receiver?.let {
            try {
                unregisterReceiver(it)
            } catch (e: IllegalArgumentException) {
                Log.w(TAG, "Session receiver was already unregistered", e)
            }
        }
        receiver = null
        val old = engine
        engine = null
        old?.stop()
        if (old != null || SubOnState.status.value.state != EffectState.OFF) {
            SubOnState.status.value = EngineStatus(EffectState.OFF)
        }
    }

    private fun shutdown(startId: Int) {
        cleanup()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf(startId)
    }

    override fun onDestroy() {
        cleanup()
        scope.cancel()
        super.onDestroy()
    }

    companion object {
        const val ACTION_START = "app.subon.START"
        const val ACTION_STOP = "app.subon.STOP"
        private const val TAG = "SubOnService"
        private const val CHANNEL_ID = "subon_filter"
        private const val NOTIF_ID = 1
    }
}
