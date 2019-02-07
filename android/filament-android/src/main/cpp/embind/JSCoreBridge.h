//
// Created by Aleksander on 1/22/19.
//

#ifndef HELLOREACT_JSCOREBRIDGE_H
#define HELLOREACT_JSCOREBRIDGE_H

#include <android/log.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSStringRef.h>


//todo: this is temporary. Need to figure out how to manage memory between JS and C++
template<typename T>
inline T* valueToPtr(T const &value)
{
    return new T(value);
}

inline char* JSStrToCStr(JSContextRef ctx, JSValueRef v, JSValueRef *exception) {
    JSStringRef jsStr = JSValueToStringCopy(ctx, v, exception);
    size_t nBytes = JSStringGetMaximumUTF8CStringSize(jsStr);
    char *cStr = (char *) malloc(nBytes * sizeof(char));
    JSStringGetUTF8CString(jsStr, cStr, nBytes);
    JSStringRelease(jsStr);
    return cStr;
}

template<typename T>
struct JSCVal {

    static T read(JSContextRef ctx, JSValueRef jsValue) {
        T* ptr = (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, T& value, JSClassRef jsClassRef) {
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, valueToPtr(value));
    }
};

template<typename T>
struct JSCVal<T&> {

    static T& read(JSContextRef ctx, JSValueRef jsValue) {
        T* ptr = (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, T& value, JSClassRef jsClassRef) {

        return (JSValueRef) JSObjectMake(ctx, jsClassRef, valueToPtr(value));
    }
};

template<typename T>
struct JSCVal<const T&> {

    static const T& read(JSContextRef ctx, JSValueRef jsValue) {
        const T* ptr = (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, const T& value, JSClassRef jsClassRef) {
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, valueToPtr(value));
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

template<typename T>
struct JSCVal<const T&&> {

    static const T&& read(JSContextRef ctx, JSValueRef jsValue) {
        const T* ptr = (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, const T& value, JSClassRef jsClassRef) {
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, valueToPtr(value));
    }
};

template<typename T>
struct JSCVal<T&&> {

    static T&& read(JSContextRef ctx, JSValueRef jsValue) {
        T* ptr = (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));

        return *ptr;
    }

    static JSValueRef write(JSContextRef ctx, T& value, JSClassRef jsClassRef) {
        return (JSValueRef) JSObjectMake(ctx, jsClassRef, valueToPtr(value));
    }
};

template<>
struct JSCVal<int> {
    static int read(JSContextRef ctx, JSValueRef jsValue) {
        double result = JSValueToNumber(ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return (int) result;
    }

    static JSValueRef write(JSContextRef ctx, const int value, JSClassRef jsClassRef) {
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

    static JSValueRef write(JSContextRef ctx, const int value, JSClassRef jsClassRef) {
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

    static JSValueRef write(JSContextRef ctx, const double value, JSClassRef jsClassRef) {
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

    static JSValueRef write(JSContextRef ctx, const double value, JSClassRef jsClassRef) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %f", value);
        return JSValueMakeNumber(ctx, value);
    }
};

template<>
struct JSCVal<char *> {
    static char* read(JSContextRef ctx, JSValueRef jsValue) {
        return JSStrToCStr(ctx, jsValue, nullptr);
    }

    static JSValueRef write(JSContextRef ctx, char* value, JSClassRef jsClassRef) {
        auto jss = JSStringCreateWithUTF8CString(value);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %s", value);
        return JSValueMakeString(ctx, jss);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCMethod {
    static JSValueRef call(ReturnType (ClassType::**method)(Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
        int index = 0;
        auto result = (nativeObject->**method)(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, result, jsClassRef);

    }
};

template<typename ClassType, typename... Args>
struct JSCMethod<void, ClassType, Args...> {
    static JSValueRef call(void (ClassType::**method)(Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
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

template<typename... Args>
struct JSCStaticMethod<void, Args...> {
    static JSValueRef call(void (*f)(Args...), JSContextRef ctx, JSClassRef jsClass, const JSValueRef jsargs[]) {
        int index = 0;
        f(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return (JSValueRef) nullptr;
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
    static JSValueRef call(ReturnType (**f)(ClassType*, Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        int index = 0;
        auto result = (**f)(nativeObject, JSCVal<Args>::read(ctx, jsargs[index++])...);

        return JSCVal<ReturnType>::write(ctx, result, jsClassRef);
    }
};

template< typename ClassType, typename... Args>
struct JSCFunction<void, ClassType, Args...> {
    static JSValueRef call(void (**f)(ClassType* thisObj, Args...), JSContextRef ctx, JSObjectRef thisObj, JSClassRef jsClassRef, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        int index = 0;
        (**f)(nativeObject, JSCVal<Args>::read(ctx, jsargs[index++])...);
        return (JSValueRef) nullptr;
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

template<typename ReturnType, typename... Args>
struct JSCCGlobalFunction {
    static JSValueRef call(ReturnType (*f)(Args...), JSContextRef ctx, JSClassRef jsClass, const JSValueRef jsargs[]) {
        int index = 0;
        auto result = f(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return JSCVal<ReturnType>::write(ctx, result, jsClass);
    }
};

template<typename... Args>
struct JSCCGlobalFunction<void, Args...> {
    static JSValueRef call(void (*f)(Args...), JSContextRef ctx, JSClassRef jsClass, const JSValueRef jsargs[]) {
        int index = 0;
        f(JSCVal<Args>::read(ctx, jsargs[index++])...);
        return (JSValueRef) nullptr;
    }
};

#endif //HELLOREACT_JSCOREBRIDGE_H
