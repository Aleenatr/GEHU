package com.example.demolition

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.cardview.widget.CardView

class SettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        
        setupToolbar()
        setupDeveloperProfiles()
        setupAppInfo()
    }
    
    private fun setupToolbar() {
        val backButton = findViewById<ImageButton>(R.id.btnBack)
        backButton.setOnClickListener {
            finish()
        }
    }
    
    private fun setupDeveloperProfiles() {
        // Developer 1 - Mayank Kumar (Android Developer + Web Developer)
        findViewById<CardView>(R.id.cardDev1).setOnClickListener {
            openUrl("https://github.com/vortex-m")
        }
        
        // Developer 2 - Aleena (AIML & Idea Generator)
        findViewById<CardView>(R.id.cardDev2).setOnClickListener {
            openUrl("https://github.com/Aleenatr")
        }
        
        // Developer 3 - Harshit Tandon (Android Developer)
        findViewById<CardView>(R.id.cardDev3).setOnClickListener {
            openUrl("https://github.com/hartan9124")
        }

        // Developer 4 - Geetika Saini (Data Researcher & AI Integration)
        findViewById<CardView>(R.id.cardDev4).setOnClickListener {
            openUrl("https://github.com/GeetikaSaini13")
        }
        
        // Email contact
        findViewById<CardView>(R.id.cardEmail).setOnClickListener {
            val intent = Intent(Intent.ACTION_SENDTO).apply {
                data = Uri.parse("mailto:mail.mayank001@gmail.com")
                putExtra(Intent.EXTRA_SUBJECT, "Feedback for Demolition App")
            }
            try {
                startActivity(Intent.createChooser(intent, "Send Email"))
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
    
    private fun openUrl(url: String) {
        try {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
            startActivity(intent)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
    
    private fun setupAppInfo() {
        val versionText = findViewById<TextView>(R.id.tvAppVersion)
        try {
            val versionName = packageManager.getPackageInfo(packageName, 0).versionName
            versionText.text = "Version $versionName"
        } catch (e: Exception) {
            versionText.text = "Version 1.0.0"
        }
    }
}
