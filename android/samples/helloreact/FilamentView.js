// @flow

import {requireNativeComponent, DeviceEventEmitter} from 'react-native';

import React from 'react';
var RCTFilamentView = requireNativeComponent('RCTFilamentView');

export default class FilamentView extends React.Component {

    componentWillMount() {
        DeviceEventEmitter.addListener('filamentViewReady', (e:Event) => {            
            console.log(`Filament event received: ${e.viewPtr}`);
            var c1 = new Filament.Counter(1);
            console.log(`Filament counter: ${c1.counter}`);

            var v = Filament.getViewFromJavaPtr(e.viewPtr);
            v.setClearColor(1,0,0,1);

        });
    }

    render() {
        
        return <RCTFilamentView style={{flex: 1}}></RCTFilamentView>
    }
}