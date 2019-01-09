package com.helloreact;
import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ThemedReactContext;


public class FilamentViewManager extends SimpleViewManager<FilamentView> {
    public static final String REACT_CLASS = "RCTFilamentView";

    @Override
    public String getName() {
        return REACT_CLASS;
    }

    @Override
    public FilamentView createViewInstance(ThemedReactContext context) {
        return new FilamentView(context);
    }
}
