package com.helloreact

import com.facebook.react.ReactActivity
import com.google.android.filament.*
import com.google.android.filament.RenderableManager.PrimitiveType
import com.google.android.filament.VertexBuffer.AttributeType
import com.google.android.filament.VertexBuffer.VertexAttribute
import com.google.android.filament.android.UiHelper
import com.google.android.filament.js.Bind;
import android.os.Bundle
import android.view.SurfaceView
import com.facebook.react.shell.MainReactPackage
import com.facebook.react.ReactPackage
import java.util.*
import java.util.Arrays.asList



class MainActivity : ReactActivity() {

    companion object {
        init {
            Filament.init()
        }
    }


    // Engine creates and destroys Filament resources
    // Each engine must be accessed from a single thread of your choosing
    // Resources cannot be shared across engines
    private lateinit var engine: Engine
    // A renderer instance is tied to a single surface (SurfaceView, TextureView, etc.)
    private lateinit var renderer: Renderer
    // A scene holds all the renderable, lights, etc. to be drawn
    private lateinit var scene: Scene
    // A view defines a viewport, a scene and a camera for rendering
    private lateinit var view: View
    // Should be pretty obvious :)
    private lateinit var camera: Camera

    // The View we want to render into
    private lateinit var surfaceView: SurfaceView

    /**
     * Returns the name of the main component registered from JavaScript.
     * This is used to schedule rendering of the component.
     */
    override fun getMainComponentName(): String? {
        return "helloreact"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        //surfaceView = SurfaceView(this)
        //setContentView(surfaceView)

        //choreographer = Choreographer.getInstance()

        //setupSurfaceView()
        setupFilament()



    }


    private fun setupFilament() {
        engine = Engine.create()
        renderer = engine.createRenderer()
        scene = engine.createScene()
        view = engine.createView()
        camera = engine.createCamera()

        //var context = this.reactInstanceManager.currentReactContext
       // Bind.BindToContext(0)

    }

}
