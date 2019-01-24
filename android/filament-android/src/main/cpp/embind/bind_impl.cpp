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
#include <string>
#include "JSCoreBridge.h"

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


typedef std::function<JSValueRef(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)> FunctionContext;
typedef std::function<JSValueRef(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)> GetterContext;
typedef std::function<JSObjectRef (JSContextRef ctx, JSClassRef jsClassRef, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)> ConstructorContext;


std::map<JSObjectRef, FunctionContext>& boundFunctions() {
    static std::map<JSObjectRef, FunctionContext> m;
    return m;
};

struct ClassDescription {
    std::string name;
    std::map<std::string, FunctionContext> methods;
    std::map<std::string, GetterContext> getters;
    ConstructorContext constructorContext;
    JSClassRef jsClassRef;
};


std::map<TYPEID, ClassDescription>& classes() {
    static std::map<TYPEID, ClassDescription> m;
    return m;
}

std::map<JSValueRef , ClassDescription>& registeredClasses() {
    static std::map<JSValueRef, ClassDescription> m;
    return m;
}


typedef void* NativeObject;
typedef int (*MethodInvokerIV)(GenericFunction function, NativeObject thisObject);
typedef JSObjectRef (*ConstructorInvoker)(GenericFunction, JSContextRef, JSClassRef, const JSValueRef jsargs[]);
typedef JSValueRef (*GenericMethodInvoker)(GenericFunction, NativeObject, JSContextRef, const JSValueRef args[]);


//Helper for string conversion
//todo: put it somewhere else

char *JStr2CStr(JSContextRef ctx, JSStringRef jsStr, JSValueRef *exception) {
    size_t nBytes = JSStringGetMaximumUTF8CStringSize(jsStr);
    char *cStr = (char *) malloc(nBytes * sizeof(char));
    JSStringGetUTF8CString(jsStr, cStr, nBytes);
    return cStr;
}

JSObjectRef GenericConstructorCall(JSContextRef ctx, JSObjectRef jsObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    __android_log_print(ANDROID_LOG_INFO, "bind", "GenericConstructorCall %u", argumentCount);
    auto prototype = JSObjectGetPrototype(ctx, jsObject);
    ClassDescription cd = registeredClasses()[prototype];

    return cd.constructorContext(ctx, cd.jsClassRef, jsObject, argumentCount, arguments, exception);
}

JSValueRef GenericGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    //todo: this lookup is really bad, make cache or something
    auto prototype = JSObjectGetPrototype(ctx, object);
    auto name = std::string(JStr2CStr(ctx, propertyName, exception));
    GetterContext gctx = registeredClasses()[prototype].getters[name];
    return gctx(ctx, object, propertyName, exception);
}

JSValueRef GenericObjectCall (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "GenericObjectCall %u", argumentCount);

    auto functionContext = boundFunctions()[function];
    return functionContext(ctx, function, thisObject, argumentCount, arguments, exception);
}

void JSRegisterClass(TYPEID classType, JSContextRef jsCtx, JSObjectRef ns)
{
    ClassDescription cd = classes()[classType];
    auto className = cd.name.c_str();

    std::vector<JSStaticFunction> staticFunctions;

    for (auto const& it : cd.methods) {

        staticFunctions.push_back({it.first.c_str(), GenericObjectCall,  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete});
    }
    staticFunctions.push_back({ 0, 0, 0 });

    std::vector<JSStaticValue> staticValues;
    for (auto const& it : cd.getters) {
        staticValues.push_back({it.first.c_str(), GenericGetter, 0, kJSPropertyAttributeDontDelete});
    }
    staticValues.push_back({0, 0, 0, 0});

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = className;
    definition.callAsConstructor = GenericConstructorCall;
    definition.staticFunctions = staticFunctions.data();
    definition.staticValues = staticValues.data();

    cd.jsClassRef = JSClassCreate(&definition);

    auto jsClassObject = JSObjectMake(jsCtx, cd.jsClassRef, nullptr);

    for (auto const& it : cd.methods) {
        JSObjectRef p = (JSObjectRef) JSObjectGetProperty(jsCtx, jsClassObject, JSStringCreateWithUTF8CString(it.first.c_str()), nullptr);
        boundFunctions()[p] = it.second;
    }

    auto prototype = JSObjectGetPrototype(jsCtx, jsClassObject);
    registeredClasses()[prototype] = cd;

    JSObjectSetProperty(jsCtx, ns, JSStringCreateWithUTF8CString(className), jsClassObject, kJSPropertyAttributeNone, nullptr);
}

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

    for (auto const& it : classes()) {
        JSRegisterClass(it.first, jsCtx, jsNamespace);
    }

    for (auto const& it : typenames()) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "Mapped type: %s ", it.second.c_str());

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
    lazy_bind().push_back([className, classType](JSGlobalContextRef jsCtx, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class %s", className);

        ClassDescription cd = {std::string(className)};
        classes()[classType] = cd;
    });
}

void _embind_register_class_constructor(
        TYPEID classType,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        GenericFunction constructor) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_constructor for %s",
                        typenames()[classType].c_str());

    ConstructorContext cc = [invoker, constructor] (JSContextRef ctx, JSClassRef jsClassRef, JSObjectRef jsConstructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {

        __android_log_print(ANDROID_LOG_INFO, "bind", " Constructor called!");

        ConstructorInvoker ci = reinterpret_cast<ConstructorInvoker>(invoker);

        return ci(constructor, ctx, jsClassRef, arguments);
    };

    classes()[classType].constructorContext = cc;
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

    auto classname = typenames()[classType];
    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_function %s::%s sig %s",
                        classname.c_str(), methodName, invokerSignature);

    FunctionContext fctx = [argCount, argTypes, invoker, context, methodName] (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "Closure called: %s", methodName);

        NativeObject no = JSObjectGetPrivate(thisObject);
        auto mi = reinterpret_cast<GenericMethodInvoker>(invoker);
        return mi(context, no, ctx, arguments);
    };

    classes()[classType].methods[std::string(methodName)] = fctx;
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
        void *setterContext)
{
    auto classname = typenames()[classType].c_str();
    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_property %s::%s", classname, fieldName);

    GetterContext gctx = [getter, getterContext](JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
        NativeObject no = JSObjectGetPrivate(object);
        MethodInvokerIV mi = reinterpret_cast<MethodInvokerIV>(getter);
        int result = mi(getterContext, no);
        return JSValueMakeNumber(ctx, result);
    };

    classes()[classType].getters[std::string(fieldName)] = gctx;
}

void _embind_register_class_class_function(
        TYPEID classType,
        const char *methodName,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        GenericFunction method) {

    __android_log_print(ANDROID_LOG_INFO, "bind",
                         "_embind_register_class_class_function %s::%s sig %s",
                         typenames()[classType].c_str(), methodName, invokerSignature);

    FunctionContext fctx = [argCount, argTypes, invoker, method, invokerSignature] (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "Static closure called %s", invokerSignature);

        return (JSObjectRef) nullptr;
    };

}

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