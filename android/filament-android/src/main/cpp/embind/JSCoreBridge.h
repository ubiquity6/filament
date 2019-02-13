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

/////////////////////////////////////////
/// Custom type conversions
/////////////////////////////////////////

typedef void* NativeObject;
typedef std::function<NativeObject (JSContextRef, JSValueRef)> JsToNative;
typedef std::function<JSValueRef (JSContextRef, NativeObject)> NativeToJs;

struct TypeInfo
{
    std::string name;
    JsToNative js2native;
    NativeToJs native2js;
};

class TypeRegistry {
    std::map<TYPEID, TypeInfo> types;

public:
    void add(TYPEID tid, std::string name, JsToNative js2native, NativeToJs nativeToJs) {
        types[tid] = {name, js2native, nativeToJs};
    }

    std::string getName(TYPEID tid) {
        return types[tid].name;
    }

    NativeObject js2native(TYPEID tid, JSContextRef ctx, JSValueRef val) {
        if (types.count(tid) > 0) {
            return types[tid].js2native(ctx, val);
        } else {
            return nullptr;
        }
    }

    JSValueRef native2js(TYPEID tid, JSContextRef ctx, NativeObject no) {
        if (types.count(tid) > 0) {
            return types[tid].native2js(ctx, no);
        } else {
            return nullptr;
        }
    }
};


/////////////////////////////////////////
/// JS value conversions
/////////////////////////////////////////

//todo: this is temporary. Need to figure out how to manage memory between JS and C++

template<typename T>
inline T* valueToPtr(T const &value)
{
    return new T(value);
}

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

template<typename T>
struct JSCVal {

    static T& read(JSValueRef jsValue, InvokerParameters params) {
        TYPEID tid = TypeID<T>::get();

        T* ptr = (T*) params.typeRegistry.js2native(tid, params.ctx, jsValue);
        if (ptr == nullptr) {
            ptr =  (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));
        }

        return  *ptr;
    }

    static JSValueRef write(const T& value, InvokerParameters params) {
        TYPEID tid = TypeID<T>::get();

        NativeObject no = (NativeObject) &value;
        JSValueRef res = params.typeRegistry.native2js(tid, params.ctx, no);

        if (res == nullptr) {
            res = (JSValueRef) JSObjectMake(params.ctx, params.jsClassRef, valueToPtr(value));
        }

        return  res;
    }
};


template<typename T>
struct JSCVal<T*> {

    static T* read(JSValueRef jsValue, InvokerParameters params) {
        TYPEID tid = TypeID<T>::get();

        T* ptr = (T*) params.typeRegistry.js2native(tid, params.ctx, jsValue);
        if (ptr == nullptr) {
            ptr =  (T*) (JSObjectGetPrivate((JSObjectRef)jsValue));
        }

        return  ptr;
    }

    static JSValueRef write(T* value, InvokerParameters params) {
        TYPEID tid = TypeID<T>::get();

        NativeObject no = (NativeObject) value;
        JSValueRef res = params.typeRegistry.native2js(tid, params.ctx, no);

        if (res == nullptr) {
            res = (JSValueRef) JSObjectMake(params.ctx, params.jsClassRef, value);
        }

        return  res;
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

template<>
struct JSCVal<int> {
    static int read(JSValueRef jsValue, InvokerParameters params) {
        double result = JSValueToNumber(params.ctx, jsValue, nullptr);
        return (int) result;
    }

    static JSValueRef write(const int value, InvokerParameters params) {
        return JSValueMakeNumber(params.ctx, value);
    }
};

template<>
struct JSCVal<int&&> : public JSCVal<int> {};

template<>
struct JSCVal<double> {
    static double read(JSValueRef jsValue, InvokerParameters params) {
        double result = JSValueToNumber(params.ctx, jsValue, nullptr);
        return result;
    }

    static JSValueRef write(const double value, InvokerParameters params) {
        return JSValueMakeNumber(params.ctx, value);
    }
};

template<>
struct JSCVal<double&&> : public JSCVal<double> {};

template<>
struct JSCVal<float&&> : public JSCVal<double> {};

template<>
struct JSCVal<float> : public JSCVal<double> {};


template<>
struct JSCVal<char *> {
    static char* read(JSValueRef jsValue, InvokerParameters params) {
        return JSStrToCStr(params.ctx, jsValue, nullptr);
    }

    static JSValueRef write(char* value, InvokerParameters params) {
        auto jss = JSStringCreateWithUTF8CString(value);
        return JSValueMakeString(params.ctx, jss);
    }
};


/////////////////////////////////////////
/// Function callers
/////////////////////////////////////////

template <typename T>
struct Sequence {
    int index = 0;

    inline T next(const T seq[]){
        T res = seq[index];
        index++;
        return res;
    }
};

template<typename ReturnType, typename ClassType, typename... Args>
struct JSCMethod {
    static JSValueRef call(ReturnType (ClassType::**method)(Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
        Sequence<JSValueRef> seq;
        auto result = (nativeObject->**method)(JSCVal<Args>::read(seq.next(jsargs), params) ...);
        return JSCVal<ReturnType>::write(result, params);
    }
};

template<typename ClassType, typename... Args>
struct JSCMethod<void, ClassType, Args...> {
    static JSValueRef call(void (ClassType::**method)(Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);
        Sequence<JSValueRef> seq;
        (nativeObject->**method)(JSCVal<Args>::read(seq.next(jsargs), params)...);
        return (JSValueRef) nullptr;
    }
};

template<typename ReturnType, typename... Args>
struct JSCStaticMethod {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        Sequence<JSValueRef> seq;
        auto obj = f(JSCVal<Args>::read(seq.next(jsargs), params)...);
        return JSCVal<ReturnType>::write(obj, params);
    }
};

template<typename... Args>
struct JSCStaticMethod<void, Args...> {
    static JSValueRef call(void (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        Sequence<JSValueRef> seq;
        f(JSCVal<Args>::read(seq.next(jsargs), params)...);
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


template<typename ClassType, typename ElementType>
struct JSCArray {

    static JSValueRef get(int index, ClassType& ptr, InvokerParameters params) {
        return JSCVal<ElementType>::write(ptr[index], params);
    }

    static void set(int index, ClassType& ptr, InvokerParameters params, JSValueRef value) {
        ptr[index] = JSCVal<ElementType>::read(value, params);
    }
};


template<typename ReturnType, typename ClassType, typename... Args>
struct JSCFunction {
    static JSValueRef call(ReturnType (**f)(ClassType*, Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        Sequence<JSValueRef> seq;
        auto result = (**f)(nativeObject, JSCVal<Args>::read(seq.next(jsargs), params)...);

        return JSCVal<ReturnType>::write(result, params);
    }
};

template< typename ClassType, typename... Args>
struct JSCFunction<void, ClassType, Args...> {
    static JSValueRef call(void (**f)(ClassType* thisObj, Args...), InvokerParameters params, JSObjectRef thisObj, const JSValueRef jsargs[]) {
        ClassType* nativeObject = (ClassType*) JSObjectGetPrivate(thisObj);

        Sequence<JSValueRef> seq;
        (**f)(nativeObject, JSCVal<Args>::read(seq.next(jsargs), params)...);

        return (JSValueRef) nullptr;
    }
};

template<typename ReturnType, typename... Args>
struct JSCConstructor {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        Sequence<JSValueRef> seq;
        auto obj = f(JSCVal<Args>::read(seq.next(jsargs), params)...);
        return JSObjectMake(params.ctx, params.jsClassRef, obj);
    }
};

template<typename ReturnType, typename... Args>
struct JSCCGlobalFunction {
    static JSValueRef call(ReturnType (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        Sequence<JSValueRef> seq;
        auto result = f(JSCVal<Args>::read(seq.next(jsargs), params)...);
        return JSCVal<ReturnType>::write(result, params);
    }
};

template<typename... Args>
struct JSCCGlobalFunction<void, Args...> {
    static JSValueRef call(void (*f)(Args...), InvokerParameters params, const JSValueRef jsargs[]) {
        Sequence<JSValueRef> seq;
        f(JSCVal<Args>::read(seq.next(jsargs), params)...);
        return (JSValueRef) nullptr;
    }
};

#endif //HELLOREACT_JSCOREBRIDGE_H
