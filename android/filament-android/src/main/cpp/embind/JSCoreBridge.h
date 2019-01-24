//
// Created by Aleksander on 1/22/19.
//

#ifndef HELLOREACT_JSCOREBRIDGE_H
#define HELLOREACT_JSCOREBRIDGE_H

#include <android/log.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

typedef void* NativeObject;

template<typename T>
struct JSCVal;

template<>
struct JSCVal<int> {
    static int read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return (int) result;
    }

    static JSValueRef write(JSContextRef ctx, int value) {
        return JSValueMakeNumber(ctx, value);
    }
};

template<>
struct JSCVal<int&&> {
    static int read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return (int) result;
    }

    static JSValueRef write(JSContextRef ctx, int value) {
        return JSValueMakeNumber(ctx, value);
    }
};

template<>
struct JSCVal<double> {

    static double read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return result;
    }

    static JSValueRef write(JSContextRef ctx, double value) {
        return JSValueMakeNumber(ctx, value);
    }
};

template<typename T>
struct JSCVal {

    static T read(JSContextRef ctx, JSValueRef jsValue) {
        return (T) JSObjectGetPrivate((JSObjectRef)jsValue);
    }

    static JSValueRef write(JSContextRef ctx, T value) {
        return (JSValueRef) JSObjectMake(ctx, nullptr, value);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCMethod {
    static JSValueRef call(ReturnType (ClassType::**method)(Args...), ClassType* thisObj, JSContextRef ctx, const JSValueRef jsargs[]) {
        int index = 0;
        auto result = (thisObj->**method)(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, result);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCFunction {
    static JSValueRef call(ReturnType (**f)(ClassType* thisObj, Args...), ClassType* thisObj, JSContextRef ctx, const JSValueRef jsargs[]) {
        int index = 0;
        auto result = (**f)(thisObj, JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, result);
    }
};

template<typename ReturnType, typename... Args>
struct JSCConstructor {
    static JSValueRef call(ReturnType (*f)(Args...), JSContextRef ctx, JSClassRef jsClass, const JSValueRef jsargs[]) {
        int index = 0;
        auto obj = f(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSObjectMake(ctx, jsClass, obj);
    }
};


#endif //HELLOREACT_JSCOREBRIDGE_H
