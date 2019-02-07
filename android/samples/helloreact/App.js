/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 * @flow
 */

import React, {Component} from 'react';
import {Platform, StyleSheet, Text, View, Button, Alert} from 'react-native';
import RCTFilamentView from './FilamentView';

const instructions = Platform.select({
  ios: 'Press Cmd+R to reload,\n' + 'Cmd+D or shake for dev menu',
  android:
    'Double tap R on your keyboard to reload,\n' +
    'Shake or press menu button for dev menu',
});

var testCounter = 0;
function assert(condition, msgErr = '', msgOK = '')
{  
  console.log(`Test# ${testCounter}` + (condition ? ` passed ${msgOK}` : ` failed ${msgErr}`));
  testCounter++;
}

function testBindings() 
{
  console.log('Testing constructor bindings...');

  var c1 = new Filament.Counter(1);
  var c2 = new Filament.Counter(2);
  var c3 = new Filament.Counter(3);
  assert(c1.counter == 1);
  assert(c2.counter == 2);
  assert(c3.counter == 3);

  console.log('Testing int accessors...');

  c1.counter = 11;
  c2.counter = 22;
  c3.counter = 33;
  assert(c1.counter == 11);
  assert(c2.counter == 22);
  assert(c3.counter == 33);

  c1.counter = 1;
  c2.counter = 2;
  c3.counter = 3;
  assert(c1.counter == 1);
  assert(c2.counter == 2);
  assert(c3.counter == 3);

  console.log('Testing object passing...');

  assert(c1.plus(c1) == 2);
  assert(c1.plus(c2) == 3);
  assert(c1.plus(c3) == 4);

  assert(c2.plus(c1) == 3);
  assert(c2.plus(c2) == 4);
  assert(c2.plus(c3) == 5);

  assert(c3.plus(c1) == 4);
  assert(c3.plus(c2) == 5);
  assert(c3.plus(c3) == 6);

  assert(c3.minus(c1) == 2);
  assert(c2.minus(c3) == -1);

  console.log('Testing void methods...');

  c1.increase();
  c2.increase();
  c3.increase();

  assert(c1.counter == 2, `Condition ${c1.counter} == 2`);
  assert(c2.counter == 3, `Condition ${c2.counter} == 3`);
  assert(c3.counter == 4, `Condition ${c3.counter} == 4`);

  console.log('Testing add function...');

  c1.counter = 1;
  c2.counter = 2;
  c3.counter = 3;

  c1.add(10);
  c2.add(20);
  c3.add(30);

  assert(c1.counter == 11, `Condition ${c1.counter} == 11`);
  assert(c2.counter == 22, `Condition ${c2.counter} == 22`);
  assert(c3.counter == 33, `Condition ${c3.counter} == 33`);

  console.log('Testing double accessors...');

  c1.someDouble = 0.1;
  c2.someDouble = 0.2;
  c3.someDouble = 0.3;

  assert(c1.someDouble == 0.1);
  assert(c2.someDouble == 0.2);
  assert(c3.someDouble == 0.3);

  console.log('Testing double methods...');

  c1.counter = 2;
  c2.counter = 3;
  c3.counter = 4;

  assert(c1.squareCounter() == 4.0);
  assert(c2.squareCounter() == 9.0);
  assert(c3.squareCounter() == 16.0);      

  console.log('Testing object creation...');
  var c1a = c1.clone();
  var c2a = c2.clone();
  var c3a = c3.clone();

  assert(c3.counter == c3a.counter, `Condition ${c3.counter} == ${c3a.counter}`);
  assert(c2.counter == c2a.counter, `Condition ${c2.counter} == ${c2a.counter}`);            
  assert(c1.counter == c1a.counter, `Condition ${c1.counter} == ${c1a.counter}`);

  var c1b = c1.clone2();
  var c2b = c2.clone2();
  var c3b = c3.clone2();

  assert(c3.counter == c3b.counter, `Condition ${c3.counter} == ${c3b.counter}`);
  assert(c2.counter == c2b.counter, `Condition ${c2.counter} == ${c2b.counter}`);            
  assert(c1.counter == c1b.counter, `Condition ${c1.counter} == ${c1b.counter}`);

  c1a.increase();
  c2a.increase();
  c3a.increase();

  assert(c1.counter != c1a.counter, `Condition ${c1.counter} != ${c1a.counter}`);
  assert(c2.counter != c2a.counter, `Condition ${c2.counter} != ${c2a.counter}`);
  assert(c3.counter != c3a.counter, `Condition ${c3.counter} != ${c3a.counter}`);

  console.log('Testing structures as properties...');

  var kv1 = c1.kv;

  assert(kv1.key == 1, `Condition ${kv1.key} == 1`);
  assert(kv1.value == 2.0, `Condition ${kv1.value} == 2.0`);

  assert(c1.kv.key == 1, `Condition ${c1.kv.key} == 1`);
  assert(c1.kv.value == 2.0, `Condition ${c1.kv.value} == 2.0`);

  kv1.key = 2;
  kv1.value = 4.0;

  assert(kv1.key == 2, `Condition ${kv1.key} == 2`);
  assert(kv1.value == 4.0, `Condition ${kv1.value} == 4.0`);
  assert(c1.kv.key == kv1.key, `Condition ${c1.kv.key} == ${kv1.key}`);
  assert(c1.kv.value == kv1.value, `Condition ${c1.kv.value} == ${kv1.value}`);
  assert(c2.kv.key != kv1.key, `Condition ${c2.kv.key} != ${kv1.key}`);
  assert(c2.kv.value != kv1.value, `Condition ${c2.kv.value} != ${kv1.value}`);

  console.log('Testing static methods...');

  var c5 = Filament.Counter.create(5);
  var c6 = Filament.Counter.create(6);

  assert(c5.counter == 5);
  assert(c6.counter == 6);

  var c7 = Filament.Counter.create2(7);
  var c8 = Filament.Counter.create2(8);

  assert(c7.counter == 7);
  assert(c8.counter == 8);

  console.log('Testing multiple parameters of different types...');

  var kv2 = c2.kv;
  kv2.key = 3;
  var sum = Filament.Counter.sumAll(1, 2.0, kv1, kv2);
  assert(sum == 8.0);

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

var f;
function initFilament()
{
  //f = new HelloTriangle();
  var vb = Filament.VertexBuffer.Builder();

  vb.vertexCount(3);
  vb.bufferCount(2);
}

function testBuilder()
{
  Filament.testBuilder();
}

type Props = {};
export default class App extends Component<Props> {
  render() {
    return (
      <View style={styles.container}>        
        <RCTFilamentView style={styles.viewport}></RCTFilamentView>
        <View style={styles.buttons}>
          <Button style={styles.btn} onPress={testBindings} title="test bindings"/>
          <Button style={styles.btn} onPress={initFilament} title="init filament"/>
          <Button style={styles.btn} onPress={testBuilder} title="test builder"/>
        </View>
      </View>
    );
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'stretch',
    backgroundColor: 'white',
  },
  buttons: {    
    justifyContent: 'space-between',
    flexDirection:"row"
  },
  btn: {
    flex: 1,    
    margin: 10,
  },  
  viewport: {    
    flex: 1
  }
});
