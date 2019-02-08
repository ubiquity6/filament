// @flow

import {requireNativeComponent, DeviceEventEmitter} from 'react-native';
import {mat4} from 'gl-matrix';

import React from 'react';
var RCTFilamentView = requireNativeComponent('RCTFilamentView');

export default class FilamentView extends React.Component {
    engine:Engine;
    view:View;
    scene:Scene;        

    componentWillMount() {
        DeviceEventEmitter.addListener('filamentViewReady', (e:Event) => {            
            console.log(`Filament view ready!`);

            this.view = Filament.getViewFromJavaPtr(e.viewPtr);
            this.engine = Filament.getEngineFromJavaPtr(e.enginePtr);
            this.scene = Filament.getSceneFromJavaPtr(e.scenePtr);            

            this.view.setClearColor(0,0,0,1);

            const radians = Math.PI;
            const transform = mat4.fromRotation(mat4.create(), radians, [0, 0, 1]);
            const tcm = this.engine.getTransformManager();
            const inst = Filament.getTransformInstance(tcm, e.triangleId);
            //tcm.setTransform(inst, transform);
            inst.delete();
        });
    }

    render() {        
        return <RCTFilamentView style={{flex: 1}}></RCTFilamentView>
    }
}

class HelloTriangle {
    constructor() {
        //this.canvas = canvas;
        //const engine = this.engine = Filament.Engine.create(this.canvas);
        const engine = this.engine = Filament.Engine.create();
        this.scene = engine.createScene();
        this.triangle = Filament.EntityManager.get().create();
        this.scene.addEntity(this.triangle);
  
        const TRIANGLE_POSITIONS = new Float32Array([
            1, 0,
            Math.cos(Math.PI * 2 / 3), Math.sin(Math.PI * 2 / 3),
            Math.cos(Math.PI * 4 / 3), Math.sin(Math.PI * 4 / 3),
        ]);
  
        const TRIANGLE_COLORS = new Uint32Array([0xffff0000, 0xff00ff00, 0xff0000ff]);
  
        this.vb = Filament.VertexBuffer.Builder()
            .vertexCount(3)
            .bufferCount(2)
            .attribute(VertexAttribute.POSITION, 0, AttributeType.FLOAT2, 0, 8)
            .attribute(VertexAttribute.COLOR, 1, AttributeType.UBYTE4, 0, 4)
            .normalized(VertexAttribute.COLOR)
            .build(engine);
  
        this.vb.setBufferAt(engine, 0, TRIANGLE_POSITIONS);
        this.vb.setBufferAt(engine, 1, TRIANGLE_COLORS);
  
        this.ib = Filament.IndexBuffer.Builder()
            .indexCount(3)
            .bufferType(Filament.IndexBuffer$IndexType.USHORT)
            .build(engine);
  
        this.ib.setBuffer(engine, new Uint16Array([0, 1, 2]));
  
        const mat = engine.createMaterial('nonlit.filamat');
        const matinst = mat.getDefaultInstance();
        Filament.RenderableManager.Builder(1)
            .boundingBox([[ -1, -1, -1 ], [ 1, 1, 1 ]])
            .material(0, matinst)
            .geometry(0, Filament.RenderableManager$PrimitiveType.TRIANGLES, this.vb, this.ib)
            .build(engine, this.triangle);
  
        this.swapChain = engine.createSwapChain();
        this.renderer = engine.createRenderer();
        this.camera = engine.createCamera();
        this.view = engine.createView();
        this.view.setCamera(this.camera);
        this.view.setScene(this.scene);
        this.resize();
        this.render = this.render.bind(this);
        this.resize = this.resize.bind(this);
        window.addEventListener('resize', this.resize);
        window.requestAnimationFrame(this.render);
    }
  
    render() {
        const radians = Date.now() / 1000;
        const transform = mat4.fromRotation(mat4.create(), radians, [0, 0, 1]);
        const tcm = this.engine.getTransformManager();
        const inst = tcm.getInstance(this.triangle);
        tcm.setTransform(inst, transform);
        inst.delete();
        this.renderer.render(this.swapChain, this.view);
        window.requestAnimationFrame(this.render);
    }
  
    resize() {
        const dpr = window.devicePixelRatio;
        const width = this.canvas.width = window.innerWidth * dpr;
        const height = this.canvas.height = window.innerHeight * dpr;
        this.view.setViewport([0, 0, width, height]);
        const aspect = width / height;
        this.camera.setProjection(Projection.ORTHO, -aspect, aspect, -1, 1, 0, 1);
    }
  }