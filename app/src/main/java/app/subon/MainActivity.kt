package app.subon

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat

class MainActivity : ComponentActivity() {
    private lateinit var prefs: Prefs
    private val notifPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
            navigationBarStyle = SystemBarStyle.dark(Color.TRANSPARENT)
        )
        prefs = Prefs(this)
        if (!SubOnState.loaded) {
            SubOnState.config.value = prefs.load()
            SubOnState.loaded = true
        }
        setContent {
            SubOnTheme {
                SubOnScreen(
                    onPower = { setPower(it) },
                    onChange = { change(it) },
                    onProfile = { selectProfile(it) }
                )
            }
        }
    }

    private fun change(block: (FilterConfig) -> FilterConfig) {
        val next = block(SubOnState.config.value)
        SubOnState.config.value = next
        prefs.save(next)
    }

    private fun selectProfile(p: OutputProfile) {
        val next = prefs.loadProfile(p)
        SubOnState.config.value = next
        prefs.save(next)
    }

    private fun setPower(on: Boolean) {
        if (on) {
            if (Build.VERSION.SDK_INT >= 33 &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED
            ) {
                notifPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
            ContextCompat.startForegroundService(
                this,
                Intent(this, SubOnService::class.java).setAction(SubOnService.ACTION_START)
            )
        } else {
            stopService(Intent(this, SubOnService::class.java))
        }
    }
}
