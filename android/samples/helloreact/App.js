/**
 * Sample React Native App
 * https://github.com/facebook/react-native
 *
 * @format
 * @flow
 */

import React, {Component} from 'react';
import {Platform, StyleSheet, Text, View, Button, Alert} from 'react-native';
import FilamentView from './FilamentView';
import {vec3} from 'gl-matrix';

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

  console.log('Testing value arrays');
  var v1 = vec3.fromValues(1,2,3);
  var v2 = vec3.fromValues(4,5,6);
  var dotP1 = vec3.dot(v1, v2);
  var dotP2 = Filament.dotProduct(v1,v2);  

  assert(dotP1 == dotP2, `Condition: ${dotP1} == ${dotP2}`);

  var v3 = Filament.makeVec3(1, 2, 3);
  assert(v3[0] == 1 && v3[1] == 2 && v3[2] == 3, `Vector should be (1,2,3): ${v3}` );

}

type Props = {};
export default class App extends Component<Props> {
  render() {
    return (
      <View style={styles.container}>        
        <FilamentView style={styles.viewport}></FilamentView>
        <View style={styles.buttons}>
          <Button style={styles.btn} onPress={testBindings} title="test bindings"/>          
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
    justifyContent: 'space-evenly',
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
