//
// Created by Aleksander on 1/22/19.
//

#ifndef HELLOREACT_JSCOREBRIDGE_H
#define HELLOREACT_JSCOREBRIDGE_H

#include <android/log.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

template<typename T>
struct JSCVal {

    static T read(JSContextRef ctx, JSValueRef jsValue) {
        T* ptr = static_cast<T*>(JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, T& value, JSClassRef jsClassRef) {
        auto ptr = &value;
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, ptr);
    }
};

template<typename T>
struct JSCVal<T*> {
    static T* read(JSContextRef ctx, JSValueRef jsValue) {
        return (T*) JSObjectGetPrivate((JSObjectRef)jsValue);
    }

    static JSValueRef write(JSContextRef ctx, T* value, JSClassRef jsClassRef) {
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, value);
    }
};

template<>
struct JSCVal<int> {
    static int read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return (int) result;
    }

    static JSValueRef write(JSContextRef ctx, int value, JSClassRef jsClassRef) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %d", value);
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

    static JSValueRef write(JSContextRef ctx, int value, JSClassRef jsClassRef) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %d", value);
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

    static JSValueRef write(JSContextRef ctx, double value, JSClassRef jsClassRef) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %f", value);
        return JSValueMakeNumber(ctx, value);
    }
};

template<>
struct JSCVal<double&&> {
    static double read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return result;
    }

    static JSValueRef write(JSContextRef ctx, double value, JSClassRef jsClassRef) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %f", value);
        return JSValueMakeNumber(ctx, value);
    }
};

template<typename ReturnType, typename ClassType, typename... Args>
struct JSCMethod {
    static JSValueRef call(ReturnType (ClassType::**method)(Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = reinterpret_cast<ClassType*>(JSObjectGetPrivate(thisObj));
        int index = 0;
        auto result = (nativeObject->**method)(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, result, jsClassRef);

    }
};

template<typename ClassType, typename... Args>
struct JSCMethod<void, ClassType, Args...> {
    static JSValueRef call(void (ClassType::**method)(Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = reinterpret_cast<ClassType*>(JSObjectGetPrivate(thisObj));
        int index = 0;
        (nativeObject->**method)(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return (JSValueRef) nullptr;
    }
};

template<typename ReturnType, typename... Args>
struct JSCStaticMethod {
    static JSValueRef call(ReturnType (*f)(Args...), JSContextRef ctx, JSClassRef jsClass, const JSValueRef jsargs[]) {
        int index = 0;
        auto obj = f(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, obj, jsClass);
    }
};

template<typename InstanceType, typename MemberType>
struct JSCField {
    typedef MemberType InstanceType::*MemberPointer;

    static JSValueRef get(const MemberPointer &field, InstanceType& ptr, JSContextRef ctx, JSClassRef jsClassRef) {
        return JSCVal<MemberType>::write(ctx, ptr.*field, jsClassRef);
    }

    static void set(const MemberPointer &field, InstanceType& ptr, JSContextRef ctx, JSValueRef value) {
        ptr.*field = JSCVal<MemberType>::read(ctx, value);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCFunction {
    static JSValueRef call(ReturnType (**f)(ClassType* thisObj, Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = reinterpret_cast<ClassType*>(JSObjectGetPrivate(thisObj));

        int index = 0;
        auto result = (**f)(nativeObject, JSCVal<Args>::read(ctx, jsargs[index++])...);

        return JSCVal<ReturnType>::write(ctx, result, jsClassRef);
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
