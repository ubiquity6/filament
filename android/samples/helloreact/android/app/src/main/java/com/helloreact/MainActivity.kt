package com.helloreact

import com.facebook.react.ReactActivity
import com.google.android.filament.*




class MainActivity : ReactActivity() {


    companion object {
        init {
            Filament.init()
        }
    }

    /**
     * Returns the name of the main component registered from JavaScript.
     * This is used to schedule rendering of the component.
     */
    override fun getMainComponentName(): String? {
        return "helloreact"
    }

}
