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

            /*

            const c = new Filament.Counter(22);            
            console.log('Counter 1 value after constructor: '+c.counter); 
            console.log('Counter 1 value after increase: '+c.increase()); 
            console.log('Counter 1 add 5: '+c.add(5)); 
            console.log('Counter 1 value squared: '+c.squareCounter()); 
            console.log('Counter 1 value: '+c.counter); 
            const c2 = new Filament.Counter(42);            
            console.log('Counter 2 value after constructor: '+c2.counter);             
            console.log('Counter 2 value after increase: '+c2.increase()); 
            console.log('Counter 2 add 5: '+c2.add(5)); 
            console.log('Counter 2 value squared: '+c2.squareCounter());
            console.log('Counter 2 value: '+c2.counter); 

            */

            var c1 = new Filament.Counter(1);
            var c2 = new Filament.Counter(2);
            var c3 = new Filament.Counter(3);

            console.log('c1: '+ c1.counter);
            console.log('c2: '+ c2.counter);
            console.log('c3: '+ c3.counter);

            console.log('c1 + c1: ' + c1.plus(c1) );
            console.log('c1 + c2: ' + c1.plus(c2) );
            console.log('c1 + c3: ' + c1.plus(c3) );
            
            console.log('c2 + c1: ' + c2.plus(c1) );
            console.log('c2 + c2: ' + c2.plus(c2) );
            console.log('c2 + c3: ' + c2.plus(c3) );

            console.log('c3 + c1: ' + c3.plus(c1) );
            console.log('c3 + c2: ' + c3.plus(c2) );
            console.log('c3 + c3: ' + c3.plus(c3) );
            
            /*
            var testOutput = Filament.Engine.test();
            console.log('Test testOutput ' + testOutput);*/
            //var view = engine.createView();            
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
