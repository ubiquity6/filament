//
// Created by Aleksander on 1/22/19.
//

#ifndef HELLOREACT_JSCOREBRIDGE_H
#define HELLOREACT_JSCOREBRIDGE_H

#include <android/log.h>
#include <emscripten/wire.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSStringRef.h>

using namespace emscripten::internal;


//todo: this is temporary. Need to figure out how to manage memory between JS and C++
template<typename T>
inline T* valueToPtr(T const &value)
{
    return new T(value);
}

class TypeRegistry {
    typedef std::function<void*(JSContextRef, JSValueRef)> JS2CConverter;
    std::map<TYPEID,JS2CConverter> js2c;

public:
    void addJS2CConverter (TYPEID id, JS2CConverter f) {
        this->js2c[id] = f;
    }

    void* fromJSValue(TYPEID tid, JSContextRef ctx, JSValueRef val) {
        if (js2c.count(tid) > 0) {
            return js2c[tid](ctx, val);
        } else {
            return nullptr;
        }
    }
};

struct InvokerParameters {
    JSContextRef ctx;
    JSClassRef jsClassRef;
    TypeRegistry typeRegistry;
};

inline char* JSStrToCStr(JSContextRef ctx, JSValueRef v, JSValueRef *exception) {
    JSStringRef jsStr = JSValueToStringCopy(ctx, v, exception);
    size_t nBytes = JSStringGetMaximumUTF8CStringSize(jsStr);
    char *cStr = (char *) malloc(nBytes * sizeof(char));
    JSStringGetUTF8CString(jsStr, cStr, nBytes);
    JSStringRelease(jsStr);
    return cStr;
}

/////////////////////////////////////////
/// JS VALUE CONVERSION
/////////////////////////////////////////

template<typename T>
struct JSCVal {

    static T& read(JSValueRef jsValue, InvokerParameters params) {
        TYPEID tid = TypeID<T>::get();

        T* ptr = (T*) params.typeRegistry.fromJSValue(tid, params.ctx, jsValue);
        if (ptr == nullptr) {
            ptr =  (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));
        }

        return  *ptr;
    }

    static JSValueRef write(const T& value, InvokerParameters params) {
        return (JSValueRef) JSObjectMake(params.ctx, params.jsClassRef, valueToPtr(value));
    }
};

template<typename T>
struct JSCVal<T&> : public JSCVal<T> {};

template<typename T>
struct JSCVal<const T&> : public JSCVal<T> {};

template<typename T>
struct JSCVal<T&&> : public JSCVal<T> {};

template<typename T>
struct JSCVal<const T&&> : public JSCVal<T> {};

template<typename T>
struct JSCVal<T*> {
    static T* read(JSValueRef jsValue, InvokerParameters params) {
        return (T*) JSObjectGetPrivate((JSObjectRef)jsValue);
    }

    static JSValueRef write(T* value, InvokerParameters params) {
        return (JSValueRef) JSObjectMake(params.ctx, params.jsClassRef, value);
    }
};

template<>
struct JSCVal<int> {
    static int read(JSValueRef jsValue, InvokerParameters params) {
        double result = JSValueToNumber(params.ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return (int) result;
    }

    static JSValueRef write(const int value, InvokerParameters params) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %d", value);
        return JSValueMakeNumber(params.ctx, value);
    }
};

template<>
struct JSCVal<int&&> : public JSCVal<int> {};

template<>
struct JSCVal<double> {
    static double read(JSValueRef jsValue, InvokerParameters params) {
        double result = JSValueToNumber(params.ctx, jsValue, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue: %f", result);
        return result;
    }

    static JSValueRef write(const double value, InvokerParameters params) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %f", value);
        return JSValueMakeNumber(params.ctx, value);
    }
};

template<>
struct JSCVal<double&&> : public JSCVal<double> {};

template<>
struct JSCVal<char *> {
    static char* read(JSValueRef jsValue, InvokerParameters params) {
        return JSStrToCStr(params.ctx, jsValue, nullptr);
    }

    static JSValueRef write(char* value, InvokerParameters params) {
        auto jss = JSStringCreateWithUTF8CString(value);
        __android_log_print(ANDROID_LOG_INFO, "bind", "JSValue write: %s", value);
        return JSValueMakeString(params.ctx, jss);
    }
};


/////////////////////////////////////////
/// FUNCTION CALLERS
/////////////////////////////////////////

template<typename ReturnType, typename ClassType, typename... Args>
struct JSCMethod {
    static JSValueRef call(ReturnType (ClassType::**method)(Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
        int index = 0;
        auto result = (nativeObject->**method)(JSCVal<Args>::read(jsargs[index++], params) ...);
        return JSCVal<ReturnType>::write(result, params);

    }
};

template<typename ClassType, typename... Args>
struct JSCMethod<void, ClassType, Args...> {
    static JSValueRef call(void (ClassType::**method)(Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
        int index = 0;
        (nativeObject->**method)(JSCVal<Args>::read(jsargs[index++], params)...);
        return (JSValueRef) nullptr;
    }
};

template<typename ReturnType, typename... Args>
struct JSCStaticMethod {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        int index = 0;
        auto obj = f(JSCVal<Args>::read(jsargs[index++], params)...);
        return JSCVal<ReturnType>::write(obj, params);
    }
};

template<typename... Args>
struct JSCStaticMethod<void, Args...> {
    static JSValueRef call(void (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        int index = 0;
        f(JSCVal<Args>::read(jsargs[index++], params)...);
        return (JSValueRef) nullptr;
    }
};

template<typename InstanceType, typename MemberType>
struct JSCField {
    typedef MemberType InstanceType::*MemberPointer;

    static JSValueRef get(const MemberPointer &field, InstanceType& ptr, InvokerParameters params) {
        return JSCVal<MemberType>::write(ptr.*field, params);
    }

    static void set(const MemberPointer &field, InstanceType& ptr, InvokerParameters params, JSValueRef value) {
        ptr.*field = JSCVal<MemberType>::read(value, params);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCFunction {
    static JSValueRef call(ReturnType (**f)(ClassType*, Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        int index = 0;
        auto result = (**f)(nativeObject, JSCVal<Args>::read(jsargs[index++], params)...);

        return JSCVal<ReturnType>::write(result, params);
    }
};

template< typename ClassType, typename... Args>
struct JSCFunction<void, ClassType, Args...> {
    static JSValueRef call(void (**f)(ClassType* thisObj, Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        int index = 0;
        (**f)(nativeObject, JSCVal<Args>::read(jsargs[index++], params)...);
        return (JSValueRef) nullptr;
    }
};

template<typename ReturnType, typename... Args>
struct JSCConstructor {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        int index = 0;
        auto obj = f(JSCVal<Args>::read(jsargs[index++], params)...);
        return JSObjectMake(params.ctx, params.jsClassRef, obj);
    }
};

template<typename ReturnType, typename... Args>
struct JSCCGlobalFunction {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        int index = 0;
        auto result = f(JSCVal<Args>::read(jsargs[index++], params)...);
        return JSCVal<ReturnType>::write(result, params);
    }
};

template<typename... Args>
struct JSCCGlobalFunction<void, Args...> {
    static JSValueRef call(void (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        int index = 0;
        f(JSCVal<Args>::read(jsargs[index++], params)...);
        return (JSValueRef) nullptr;
    }
};

#endif //HELLOREACT_JSCOREBRIDGE_H
