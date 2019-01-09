package com.helloreact;

import android.content.Context;
import android.view.View;

import com.facebook.react.bridge.JavaScriptContextHolder;
import com.facebook.react.bridge.ReactContext;
import com.google.android.filament.js.Bind;

public class FilamentView extends View {

    public FilamentView(Context context) {
        super(context);

        ReactContext reactContext = (ReactContext)getContext();

        JavaScriptContextHolder jsContext = reactContext.getJavaScriptContextHolder();
        synchronized (jsContext) {
            Bind.BindToContext(jsContext.get());
        }

        /*reactContext.getJSModule(RCTEventEmitter.class).receiveEvent(
                getId(),
                "topChange",
                event);*/
    }
}
