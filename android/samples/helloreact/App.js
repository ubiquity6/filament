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

type Props = {};
export default class App extends Component<Props> {
  render() {
    return (
      <View style={styles.container}>
        <Text style={styles.welcome}>Welcome to React Native!</Text>
        <Text style={styles.instructions}>To get started, edit App.js</Text>
        <Text style={styles.instructions}>{instructions}</Text>
        <RCTFilamentView></RCTFilamentView>
        <Button
          onPress={() => {            

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

          }}
          title="Press Me"
        />
      </View>
    );
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#F5FCFF',
  },
  welcome: {
    fontSize: 20,
    textAlign: 'center',
    margin: 10,
  },
  instructions: {
    textAlign: 'center',
    color: '#333333',
    marginBottom: 5,
  },
});
