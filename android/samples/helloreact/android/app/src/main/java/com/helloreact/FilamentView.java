package com.helloreact;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.support.annotation.Nullable;
import android.view.Choreographer;
import android.view.Surface;
import android.view.SurfaceView;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;
import com.google.android.filament.js.Bind;
import com.google.android.filament.*;
import com.google.android.filament.android.UiHelper;
import com.google.android.filament.RenderableManager.*;
import com.google.android.filament.VertexBuffer.VertexAttribute;

import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.Channels;
import java.nio.channels.ReadableByteChannel;

public class FilamentView extends SurfaceView implements Choreographer.FrameCallback {
    private UiHelper mUiHelper;

     // Filament specific APIs
    private Engine engine;
    private Renderer mRenderer;
    private View view; // com.google.android.filament.View, not android.view.View
    private SwapChain swapChain;
    private Camera camera;
    private Scene scene;

    private Material material;
    private VertexBuffer vertexBuffer;
    private IndexBuffer indexBuffer;

    private Choreographer choreographer;
    private ReactContext reactContext;


    public FilamentView(Context context) {
        super(context);

        reactContext = (ReactContext)getContext();
        JavaScriptContextHolder jsContext = reactContext.getJavaScriptContextHolder();

        synchronized (jsContext) {
            Bind.BindToContext(jsContext.get());
        }

        setupFilament();
        choreographer = Choreographer.getInstance();
        choreographer.postFrameCallback(this);

        /*reactContext.getJSModule(RCTEventEmitter.class).receiveEvent(
                getId(),
                "topChange",
                event);*/
    }

    void setupFilament()
    {
        mUiHelper = new UiHelper(UiHelper.ContextErrorPolicy.DONT_CHECK);

        // Attach the SurfaceView to the helper, you could do the same with a TextureView
        mUiHelper.attachTo(this);

        // Set a rendering callback that we will use to invoke Filament
        mUiHelper.setRenderCallback(new UiHelper.RendererCallback() {
            public void onNativeWindowChanged(Surface surface) {
                if (swapChain != null) engine.destroySwapChain(swapChain);
                swapChain = engine.createSwapChain(surface, mUiHelper.getSwapChainFlags());
                WritableMap params = Arguments.createMap();
                long ptr = view.getNativeObject();
                params.putString("viewPtr", Long.toString(ptr));
                sendEvent(reactContext, "filamentViewReady", params);
            }

            // The native surface went away, we must stop rendering.
            public void onDetachedFromSurface() {
                if (swapChain != null) {
                    engine.destroySwapChain(swapChain);
                    // Required to ensure we don't return before Filament is done executing the
                    // destroySwapChain command, otherwise Android might destroy the Surface
                    // too early
                    engine.flushAndWait();
                    swapChain = null;
                }
            }
            // The native surface has changed size. This is always called at least once
            // after the surface is created (after onNativeWindowChanged() is invoked).
            public void onResized(int width, int height) {
                // Compute camera projection and set the viewport on the view
                float zoom = 1.5f;
                double aspect = (double) width / (double) height;
                camera.setProjection(Camera.Projection.ORTHO,
                        -aspect * zoom, aspect * zoom, -zoom, zoom, 0.0, 10.0);

                view.setViewport(new Viewport(0, 0, width, height));
            }
        });

        engine = Engine.create();
        mRenderer = engine.createRenderer();
        camera = engine.createCamera();
        scene = engine.createScene();
        view = engine.createView();
        long nativeViewPtr = view.getNativeObject();


        view.setClearColor(0.035f, 0.035f, 0.035f, 1.0f);
        view.setCamera(camera);
        view.setScene(scene);
        setupScene();
        //view.setClearColor(1, 0, 0, 1);
    }

    private void setupScene() {
        loadMaterial();
        createMesh();

        // To create a renderable we first create a generic entity
        int renderable = EntityManager.get().create();

        // We then create a renderable component on that entity
        // A renderable is made of several primitives; in this case we declare only 1
        new RenderableManager.Builder(1)
                // Overall bounding box of the renderable
                .boundingBox(new Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.01f))
                // Sets the mesh data of the first primitive
                .geometry(0, PrimitiveType.TRIANGLES, vertexBuffer, indexBuffer, 0, 3)
                // Sets the material of the first primitive
                .material(0, material.getDefaultInstance())
                .build(engine, renderable);

        // Add the entity to the scene to render it
        scene.addEntity(renderable);

        //startAnimation()
    }

    private void loadMaterial() {
        ByteBuffer buff = readUncompressedAsset("materials/baked_color.filamat");

        material = new Material.Builder().payload(buff, buff.remaining()).build(engine);
    }

    private void putVertex(ByteBuffer b, double x, double y, double z, int color)
    {
        b.putFloat((float)x);
        b.putFloat((float)y);
        b.putFloat((float)z);
        b.putInt(color);
    }

    private void createMesh() {
        int intSize = 4;
        int floatSize = 4;
        int shortSize = 2;
        // A vertex is a position + a color:
        // 3 floats for XYZ position, 1 integer for color
        int vertexSize = 3 * floatSize + intSize;

        // We are going to generate a single triangle
        int vertexCount = 3;
        double a1 = Math.PI * 2.0f / 3.0f;
        double a2 = Math.PI * 4.0f / 3.0f;

        ByteBuffer vertexData = ByteBuffer.allocate(vertexCount * vertexSize);

        // It is important to respect the native byte order
        vertexData.order(ByteOrder.nativeOrder());
        putVertex(vertexData, 1.0f,0.0f,0.0f,0xffff0000);
        putVertex(vertexData, Math.cos(a1), Math.sin(a1), 0.0f, 0xff00ff00);
        putVertex(vertexData, Math.cos(a2), Math.sin(a2), 0.0f, 0xff0000ff);
        // Make sure the cursor is pointing in the right place in the byte buffer
        vertexData.flip();

        // Declare the layout of our mesh
        vertexBuffer = new VertexBuffer.Builder()
                .bufferCount(1)
                .vertexCount(vertexCount)
                // Because we interleave position and color data we must specify offset and stride
                // We could use de-interleaved data by declaring two buffers and giving each
                // attribute a different buffer index
                .attribute(VertexAttribute.POSITION, 0, VertexBuffer.AttributeType.FLOAT3, 0,             vertexSize)
                .attribute(VertexAttribute.COLOR,    0, VertexBuffer.AttributeType.UBYTE4, 3 * floatSize, vertexSize)
                // We store colors as unsigned bytes but since we want values between 0 and 1
                // in the material (shaders), we must mark the attribute as normalized
                .normalized(VertexAttribute.COLOR)
                .build(engine);

        // Feed the vertex data to the mesh
        // We only set 1 buffer because the data is interleaved
        vertexBuffer.setBufferAt(engine, 0, vertexData);

        // Create the indices
        ByteBuffer indexData = ByteBuffer.allocate(vertexCount * shortSize);
        indexData.order(ByteOrder.nativeOrder())
                .putShort((short) 0)
                .putShort((short) 1)
                .putShort((short) 2)
                .flip();

        indexBuffer = new IndexBuffer.Builder()
                .indexCount(3)
                .bufferType(IndexBuffer.Builder.IndexType.USHORT)
                .build(engine);

        indexBuffer.setBuffer(engine, indexData);
    }

    private void sendEvent(ReactContext reactContext,
                           String eventName,
                           @Nullable WritableMap params) {
        reactContext
                .getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
                .emit(eventName, params);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
    }

    // This is an example of a render function. You will most likely invoke this from
    // a Choreographer callback to trigger rendering at vsync.
    public void doFrame(long frameTimeNanos) {

        choreographer.postFrameCallback(this);

        if (mUiHelper.isReadyToRender()) {
            // If beginFrame() returns false you should skip the frame
            // This means you are sending frames too quickly to the GPU
            if (mRenderer.beginFrame(swapChain)) {
                mRenderer.render(view);
                mRenderer.endFrame();
            }
        }
    }

    private ByteBuffer readUncompressedAsset(String assetName) {
        try {
            AssetFileDescriptor fd = getResources().getAssets().openFd(assetName);
            FileInputStream input = fd.createInputStream();

            ByteBuffer dst = ByteBuffer.allocate((int) fd.getLength());
            ReadableByteChannel src = Channels.newChannel(input);

            src.read(dst);
            src.close();
            dst.rewind();

            return dst;
        } catch (Exception e) {
            return  null;
        }
    }

}
