//
// Created by Nicolas Coderre on 1/7/19.
//
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

#include <android/log.h>
#include "bind_impl.h"

using namespace emscripten;
using namespace emscripten::internal;


#include <jni.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include "EXJSUtils.h"


// create a list of lamdas to lazy-bind to the javascript context once it becomes available.
std::vector<std::function<void(JSGlobalContextRef, JSObjectRef)>>& lazy_bind() {
    static std::vector<std::function<void(JSGlobalContextRef, JSObjectRef)>> lazy_bind_list;
    return lazy_bind_list;
}

std::map<TYPEID, std::string>& typenames() {
    static std::map<TYPEID, std::string> m;
    return m;
};

//typedef JSValueRef
//(*JSObjectCallAsFunctionCallback) (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);


// memory of bound functions.
typedef std::function<JSValueRef(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)> FunctionContext;

std::map<JSObjectRef, FunctionContext>& boundFunctions() {
    static std::map<JSObjectRef, FunctionContext> m;
    return m;
};

#define JNI_METHOD(return_type, method_name)                                   \
  extern "C" JNIEXPORT return_type JNICALL                                     \
      Java_com_google_android_filament_js_Bind_##method_name

JNI_METHOD(void, BindToContext)
(JNIEnv* env,
 jclass jcls,
 jlong jsCtxPtr) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "BindToContext");

    JSGlobalContextRef jsCtx = (JSGlobalContextRef) (intptr_t) jsCtxPtr;

    auto jsGlobal = JSContextGetGlobalObject(jsCtx);
    auto jsNamespace = JSObjectMake(jsCtx, nullptr, nullptr);
    EXJSObjectSetValueWithUTF8CStringName(jsCtx, jsGlobal, "Filament", jsNamespace);

    for(auto k = lazy_bind().begin(); k< lazy_bind().end(); ++k) {
        (*k)(jsCtx, jsNamespace);
    }

}


// Implemented in JavaScript.  Don't call these directly.
extern "C" {


void _embind_fatal_error(
        const char *name,
        const char *payload) {
    //throw new std::runtime_error(name);
}

void _embind_register_void(
        TYPEID voidType,
        const char *name) {
//    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_void lazy %u, %u", lazy_bind().size(), &lazy_bind());

    typenames()[voidType] = std::string(name);

    lazy_bind().push_back([voidType,name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_void %s", name);
    });
}

void _embind_register_bool(
        TYPEID boolType,
        const char *name,
        size_t size,
        bool trueValue,
        bool falseValue) {

    typenames()[boolType] = std::string(name);

    //__android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_bool lazy %u", lazy_bind().size());
    lazy_bind().push_back([boolType,name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_bool %s", name);
    });

}

void _embind_register_integer(
        TYPEID integerType,
        const char *name,
        size_t size,
        long minRange,
        unsigned long maxRange) {
    typenames()[integerType] = std::string(name);


    //__android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_integer lazy %u", lazy_bind().size());
    lazy_bind().push_back([integerType,name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_integer %s", name);
    });

}

void _embind_register_float(
        TYPEID floatType,
        const char *name,
        size_t size) {
    typenames()[floatType] = std::string(name);

    //__android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_float lazy %u", lazy_bind().size());
    lazy_bind().push_back([floatType,name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_float %s", name);
    });

}

void _embind_register_std_string(
        TYPEID stringType,
        const char *name) {
    typenames()[stringType] = std::string(name);
    //__android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_std_string lazy %u", lazy_bind().size());
    lazy_bind().push_back([stringType,name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_std_string %s", name);
    });

}

void _embind_register_std_wstring(
        TYPEID stringType,
        size_t charSize,
        const char *name) {}

void _embind_register_emval(
        TYPEID emvalType,
        const char *name) {}

void _embind_register_memory_view(
        TYPEID memoryViewType,
        unsigned typedArrayIndex,
        const char *name) {}

void _embind_register_function(
        const char *name,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *signature,
        GenericFunction invoker,
        GenericFunction function) {

    lazy_bind().push_back([name](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_function %s", name);
    });
}

void _embind_register_value_array(
        TYPEID tupleType,
        const char *name,
        const char *constructorSignature,
        GenericFunction constructor,
        const char *destructorSignature,
        GenericFunction destructor) {}

void _embind_register_value_array_element(
        TYPEID tupleType,
        TYPEID getterReturnType,
        const char *getterSignature,
        GenericFunction getter,
        void *getterContext,
        TYPEID setterArgumentType,
        const char *setterSignature,
        GenericFunction setter,
        void *setterContext) {}

void _embind_finalize_value_array(TYPEID tupleType) {}

void _embind_register_value_object(
        TYPEID structType,
        const char *fieldName,
        const char *constructorSignature,
        GenericFunction constructor,
        const char *destructorSignature,
        GenericFunction destructor) {}

void _embind_register_value_object_field(
        TYPEID structType,
        const char *fieldName,
        TYPEID getterReturnType,
        const char *getterSignature,
        GenericFunction getter,
        void *getterContext,
        TYPEID setterArgumentType,
        const char *setterSignature,
        GenericFunction setter,
        void *setterContext) {}

void _embind_finalize_value_object(TYPEID structType) {}

void _embind_register_class(
        TYPEID classType,
        TYPEID pointerType,
        TYPEID constPointerType,
        TYPEID baseClassType,
        const char *getActualTypeSignature,
        GenericFunction getActualType,
        const char *upcastSignature,
        GenericFunction upcast,
        const char *downcastSignature,
        GenericFunction downcast,
        const char *className,
        const char *destructorSignature,
        GenericFunction destructor) {
    typenames()[classType] = std::string(className);

    //__android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class lazy %u, %u", lazy_bind().size(), &lazy_bind());
    lazy_bind().push_back([className](JSGlobalContextRef jsCtx, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class %s", className);

        // todo: we shouldn't be creating a object, we should be creating a function? wait for createConstructor call?
        auto jsClassObject = JSObjectMake(jsCtx, nullptr, nullptr);

        EXJSObjectSetValueWithUTF8CStringName(jsCtx, ns, className, jsClassObject);

    });

}

void _embind_register_class_constructor(
        TYPEID classType,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        GenericFunction constructor) {
    lazy_bind().push_back([classType](JSGlobalContextRef context, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_constructor for %s",
                            typenames()[classType].c_str());


    });

}


typedef void (*AFunc)(int);

JSValueRef GenericObjectCall (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "GenericObjectCall %u", argumentCount);

    auto functionContext = boundFunctions()[function];
    return functionContext(ctx, function, thisObject, argumentCount, arguments, exception);
}



void EXJSObjectSetFunctionWithUTF8CStringNameAndFunctionContext(JSContextRef ctx,
                                              JSObjectRef obj,
                                              const char *name,
                                              JSObjectCallAsFunctionCallback func,
                                                            const FunctionContext& data) {
    JSStringRef jsName = JSStringCreateWithUTF8CString(name);
    JSObjectRef jsFunction = JSObjectMakeFunctionWithCallback(ctx, jsName, func);
//    JSObjectSetPrivate(jsFunction, data);
    boundFunctions()[jsFunction] = data;
    JSObjectSetProperty(ctx, obj, jsName, jsFunction, 0, NULL);
    JSStringRelease(jsName);
}

void _embind_register_class_function(
        TYPEID classType,
        const char *methodName,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        void *context,
        unsigned isPureVirtual) {
    lazy_bind().push_back([methodName, classType, invoker, argCount, argTypes, context, invokerSignature](JSGlobalContextRef jsCtx, JSObjectRef ns) {
        auto classname = typenames()[classType].c_str();
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_function %s::%s &%u sig %s",
                            typenames()[classType].c_str(), methodName, invoker, invokerSignature);

        auto jsClassObject = (JSObjectRef) EXJSObjectGetPropertyNamed(jsCtx, ns, classname);

        FunctionContext fctx = [argCount, argTypes, invoker, context, invokerSignature] (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
            __android_log_print(ANDROID_LOG_INFO, "bind", "Closure called");

            // here you got to call
            // invoker(context, (args...))
            // the args is the hard part. you need to convert arguments, given argTypes into the right things, and use that for the args. cleverness is required.
            return (JSObjectRef)nullptr;
        };


        EXJSObjectSetFunctionWithUTF8CStringNameAndFunctionContext(jsCtx, jsClassObject, methodName, GenericObjectCall, fctx);

    });
}

void _embind_register_class_property(
        TYPEID classType,
        const char *fieldName,
        TYPEID getterReturnType,
        const char *getterSignature,
        GenericFunction getter,
        void *getterContext,
        TYPEID setterArgumentType,
        const char *setterSignature,
        GenericFunction setter,
        void *setterContext) {}

void _embind_register_class_class_function(
        TYPEID classType,
        const char *methodName,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        GenericFunction method) {}

void _embind_register_class_class_property(
        TYPEID classType,
        const char *fieldName,
        TYPEID fieldType,
        const void *fieldContext,
        const char *getterSignature,
        GenericFunction getter,
        const char *setterSignature,
        GenericFunction setter) {}

EM_VAL _embind_create_inheriting_constructor(
        const char *constructorName,
        TYPEID wrapperType,
        EM_VAL properties) {
    return 0;
}

void _embind_register_enum(
        TYPEID enumType,
        const char *name,
        size_t size,
        bool isSigned) {}

void _embind_register_smart_ptr(
        TYPEID pointerType,
        TYPEID pointeeType,
        const char *pointerName,
        sharing_policy sharingPolicy,
        const char *getPointeeSignature,
        GenericFunction getPointee,
        const char *constructorSignature,
        GenericFunction constructor,
        const char *shareSignature,
        GenericFunction share,
        const char *destructorSignature,
        GenericFunction destructor) {}

void _embind_register_enum_value(
        TYPEID enumType,
        const char *valueName,
        GenericEnumValue value) {}

void _embind_register_constant(
        const char *name,
        TYPEID constantType,
        double value) {}

}