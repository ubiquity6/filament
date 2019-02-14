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

TypeRegistry& typeRegistry() {
    static TypeRegistry tr;
    return tr;
};

typedef void* NativeObject;

typedef std::function<JSValueRef(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)> FunctionContext;
typedef std::function<JSValueRef(JSContextRef ctx, JSObjectRef object, JSValueRef* exception)> GetterContext;
typedef std::function<void(JSContextRef ctx, JSObjectRef object, JSValueRef value, JSValueRef* exception)> SetterContext;
typedef std::function<JSObjectRef (JSContextRef ctx, JSClassRef jsClassRef, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)> ConstructorContext;
typedef std::function<JSValueRef(JSContextRef ctx, NativeObject no, JSValueRef* exception)> ValueArrayGetter;
typedef std::function<void(JSContextRef ctx, NativeObject no, JSValueRef value, JSValueRef* exception)> ValueArraySetter;


std::map<JSObjectRef, FunctionContext>& boundFunctions() {
    static std::map<JSObjectRef, FunctionContext> m;
    return m;
};

struct ClassFieldAccess {
    GetterContext getterContext;
    SetterContext setterContext;
};

struct ClassFieldAccessWithJSName {
    JSStringRef name;
    ClassFieldAccess access;
};

struct ClassDescription {
    std::string name;
    std::map<std::string, FunctionContext> methodsByName;
    std::map<std::string, FunctionContext> staticMethodsByName;
    std::map<std::string, ClassFieldAccess> fieldAccessorsByName;
    std::vector<ClassFieldAccessWithJSName> fieldAccessors;

    ConstructorContext constructorContext;
    JSClassRef jsClassRef;

    ClassDescription(std::string className) {
        name = className;
    }

    ClassFieldAccess findFieldAccessor(JSStringRef propName) {
        // I suspect that this might be quicker than map since number of properties is low and constant
        // We also avoid conversion from JSStringRef
        for (auto const& field : fieldAccessors) {
            if(JSStringIsEqual(field.name, propName)) {
                return field.access;
            }
        }

        //Not fount
        //todo: Emit JS exception perhaps?
        return {nullptr, nullptr};
    }
};


std::map<TYPEID, ClassDescription*>& classesByTypeId() {
    static std::map<TYPEID, ClassDescription*> m;
    return m;
}

std::map<TYPEID, JSObjectRef >& enumByTypeId() {
    static std::map<TYPEID, JSObjectRef> m;
    return m;
}

std::map<JSValueRef , ClassDescription*>& classesByPrototype() {
    static std::map<JSValueRef, ClassDescription*> m;
    return m;
}

JSClassRef getJSClassRefByTypeId(TYPEID typeId)
{
    return classesByTypeId().count(typeId) > 0
          ? classesByTypeId()[typeId]->jsClassRef
          : nullptr;
}



typedef void* (*DefaultConstructor)();
typedef JSValueRef (*GetterInvoker)(GenericFunction, NativeObject, InvokerParameters);
typedef void (*SetterInvoker)(GenericFunction, NativeObject, InvokerParameters, JSValueRef);
typedef JSObjectRef (*ConstructorInvoker)(GenericFunction, InvokerParameters, const JSValueRef jsargs[]);
typedef JSValueRef (*GenericMethodInvoker)(GenericFunction, InvokerParameters, JSObjectRef, const JSValueRef args[]);
typedef JSObjectRef (*StaticMethodInvoker)(GenericFunction, InvokerParameters, const JSValueRef jsargs[]);

JSObjectRef GenericConstructorCall(JSContextRef ctx, JSObjectRef jsObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    __android_log_print(ANDROID_LOG_INFO, "bind", "GenericConstructorCall %u", argumentCount);
    auto prototype = JSObjectGetPrototype(ctx, jsObject);
    ClassDescription* cd = classesByPrototype()[prototype];

    return cd->constructorContext(ctx, cd->jsClassRef, jsObject, argumentCount, arguments, exception);
}

JSValueRef GenericGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    auto prototype = JSObjectGetPrototype(ctx, object);
    return classesByPrototype()[prototype]->findFieldAccessor(propertyName)
            .getterContext(ctx, object, exception);
}

bool GenericSetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    auto prototype = JSObjectGetPrototype(ctx, object);
    SetterContext sctx = classesByPrototype()[prototype]->findFieldAccessor(propertyName)
            .setterContext;

    if (sctx != nullptr) {
        sctx(ctx, object, value, nullptr);
        return true;
    } else {
        return false;
    }

}

JSValueRef GenericObjectCall (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "GenericObjectCall %u", argumentCount);

    auto functionContext = boundFunctions()[function];
    return functionContext(ctx, function, thisObject, argumentCount, arguments, exception);
}



void JSRegisterClass(TYPEID classType, JSContextRef jsCtx, JSObjectRef ns)
{
    ClassDescription* cd = classesByTypeId()[classType];
    auto className = cd->name.c_str();

    std::vector<JSStaticFunction> staticFunctions;

    for (auto const& it : cd->methodsByName) {

        staticFunctions.push_back({it.first.c_str(), GenericObjectCall,  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete});
    }
    staticFunctions.push_back({ 0, 0, 0 });

    std::vector<JSStaticValue> staticValues;
    for (auto const& it : cd->fieldAccessorsByName) {
        auto fieldNameC = it.first.c_str();
        auto fieldNameJS = JSStringCreateWithUTF8CString(fieldNameC);
        cd->fieldAccessors.push_back({fieldNameJS, it.second});
        staticValues.push_back({fieldNameC, GenericGetter, GenericSetter, kJSPropertyAttributeDontDelete});
    }

    staticValues.push_back({0, 0, 0, 0});

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = className;
    definition.callAsConstructor = GenericConstructorCall;
    definition.staticFunctions = staticFunctions.data();
    definition.staticValues = staticValues.data();

    cd->jsClassRef = JSClassCreate(&definition);

    auto jsClassObject = JSObjectMake(jsCtx, cd->jsClassRef, nullptr);

    for (auto const& it : cd->methodsByName) {
        JSObjectRef p = (JSObjectRef) JSObjectGetProperty(jsCtx, jsClassObject, JSStringCreateWithUTF8CString(it.first.c_str()), nullptr);
        boundFunctions()[p] = it.second;
    }

    auto prototype = JSObjectGetPrototype(jsCtx, jsClassObject);
    classesByPrototype()[prototype] = cd;

    auto jsStringClassName = JSStringCreateWithUTF8CString(className);

    JSObjectSetProperty(jsCtx, ns, jsStringClassName, jsClassObject, kJSPropertyAttributeNone, nullptr);

    for (auto const& it : cd->staticMethodsByName) {
        auto fName = JSStringCreateWithUTF8CString(it.first.c_str());
        auto f = JSObjectMakeFunctionWithCallback(jsCtx, fName, GenericObjectCall);
        boundFunctions()[f] = it.second;
        JSObjectSetProperty(jsCtx, jsClassObject, fName, f, kJSPropertyAttributeNone, nullptr);
    }
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

    for (auto const& it : classesByTypeId()) {
        JSRegisterClass(it.first, jsCtx, jsNamespace);
    }

    for (auto const& it : typenames()) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "Mapped type: %s ", it.second.c_str());

    }

}

struct ValueArrayAccess {
    ValueArrayGetter getter;
    ValueArraySetter setter;
};

struct ValueArrayInfo {
    std::string typeName;
    GenericFunction constructor;
    GenericFunction destructor;
    std::vector<ValueArrayAccess> accessors;

    ValueArrayInfo (std::string typeName, GenericFunction constructor, GenericFunction destructor) {
        this->typeName = typeName;
        this->constructor = constructor;
        this->destructor = destructor;
    }
};

std::map<TYPEID, ValueArrayInfo*>& valueArrayInfoByType() {
    static std::map<TYPEID, ValueArrayInfo*> m;
    return m;
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

    lazy_bind().push_back([name, argTypes, invoker, function](JSGlobalContextRef ctx, JSObjectRef ns) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_function %s", name);
        auto fName = JSStringCreateWithUTF8CString(name);
        auto f = JSObjectMakeFunctionWithCallback(ctx, fName, GenericObjectCall);

        FunctionContext fctx = [argTypes, invoker, name, function] (JSContextRef ctx, JSObjectRef jsFRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
            __android_log_print(ANDROID_LOG_INFO, "bind", "Function called %s", name);

            auto smi = reinterpret_cast<StaticMethodInvoker>(invoker);
            return smi(function, {ctx, getJSClassRefByTypeId(argTypes[0]), typeRegistry()}, arguments);
        };

        boundFunctions()[f] = fctx;
        JSObjectSetProperty(ctx, ns, fName, f, kJSPropertyAttributeNone, nullptr);

    });
}

void _embind_register_value_array(
        TYPEID tupleType,
        const char *name,
        const char *constructorSignature,
        GenericFunction constructor,
        const char *destructorSignature,
        GenericFunction destructor) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_value_array %s", name);
    valueArrayInfoByType()[tupleType] = new ValueArrayInfo(std::string(name), constructor, destructor);

}

void _embind_register_value_array_element(
        TYPEID tupleType,
        TYPEID getterReturnType,
        const char *getterSignature,
        GenericFunction getter,
        void *getterContext,
        TYPEID setterArgumentType,
        const char *setterSignature,
        GenericFunction setter,
        void *setterContext) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_value_array_element");

    ValueArrayGetter gctx = [getter, getterContext, getterReturnType](JSContextRef ctx, NativeObject no, JSValueRef* exception) {

        GetterInvoker gi = reinterpret_cast<GetterInvoker>(getter);
        return gi(getterContext, no, {ctx, getJSClassRefByTypeId(getterReturnType), typeRegistry()});
    };


    ValueArraySetter sctx = [setter, setterContext, setterArgumentType](JSContextRef ctx, NativeObject no, JSValueRef value, JSValueRef* exception) {
        SetterInvoker si = reinterpret_cast<SetterInvoker>(setter);
        return si(setterContext, no, {ctx, getJSClassRefByTypeId(setterArgumentType), typeRegistry()}, value);
    };

    valueArrayInfoByType()[tupleType]->accessors.push_back({gctx, sctx});

}

void _embind_finalize_value_array(TYPEID tupleType) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_finalize_value_array ");

    auto info = valueArrayInfoByType()[tupleType];

    JsToNative js2c = [tupleType, info](JSContextRef ctx, JSValueRef val) {
        DefaultConstructor dc = (DefaultConstructor) info->constructor;
        NativeObject no = dc();
        for (int i = 0; i<info->accessors.size(); i++) {
            JSValueRef v = JSObjectGetPropertyAtIndex(ctx, (JSObjectRef) val, i, nullptr);
            info->accessors[i].setter(ctx, no, v, nullptr);
        }

        return no;
    };

    NativeToJs c2js = [tupleType, info](JSContextRef ctx, NativeObject no) {
        size_t argCount = info->accessors.size();
        JSValueRef arguments [argCount];
        for (int i =0; i < argCount; i++) {
            arguments[i] = info->accessors[i].getter(ctx, no, nullptr);
        }

        return JSObjectMakeArray(ctx, argCount, arguments, nullptr);
    };

    typeRegistry().add(tupleType, info->typeName, js2c, c2js);

}

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

    auto cName = std::string(className);
    typenames()[classType] = cName;
    typenames()[pointerType] = cName + "*";
    typenames()[constPointerType] = "const " + cName + "*";


    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class %s", className);
    ClassDescription* cd = new ClassDescription(std::string(className));
    classesByTypeId()[classType] = cd;
    classesByTypeId()[pointerType] = cd;
    classesByTypeId()[constPointerType] = cd;

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

        return ci(constructor, {ctx, jsClassRef, typeRegistry()}, arguments);
    };

    classesByTypeId()[classType]->constructorContext = cc;
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

    auto returnType = argTypes[0];

    for (int i = 0; i< argCount; i++) {
        auto typeId = argTypes[i];
        auto info = reinterpret_cast<const std::type_info *>(typeId);

        __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_class_function %s arg %d Type name: %s Raw type: %s", methodName, i, typenames()[typeId].c_str(), info->name());
    }

    FunctionContext fctx = [argCount, argTypes, invoker, context, methodName, returnType] (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
        __android_log_print(ANDROID_LOG_INFO, "bind", "Closure called: %s", methodName);

        auto mi = reinterpret_cast<GenericMethodInvoker>(invoker);
        return mi(context, {ctx, getJSClassRefByTypeId(returnType), typeRegistry()}, thisObject, arguments);
    };

    auto mName = std::string(methodName);
    if(mName[0] == '_') {
        mName.erase(0,1);
    }

    classesByTypeId()[classType]->methodsByName[mName] = fctx;
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

    GetterContext gctx = [getter, getterContext, getterReturnType, classname, fieldName](JSContextRef ctx, JSObjectRef object, JSValueRef* exception) {
        NativeObject no = JSObjectGetPrivate(object);

        __android_log_print(ANDROID_LOG_INFO, "bind", "Getter called %s::%s", classname, fieldName);

        GetterInvoker gi = reinterpret_cast<GetterInvoker>(getter);
        return gi(getterContext, no, {ctx, getJSClassRefByTypeId(getterReturnType), typeRegistry()});
    };


    SetterContext sctx = [setter, setterContext, setterArgumentType](JSContextRef ctx, JSObjectRef object, JSValueRef value, JSValueRef* exception) {
        NativeObject no = JSObjectGetPrivate(object);
        SetterInvoker si = reinterpret_cast<SetterInvoker>(setter);
        return si(setterContext, no, {ctx, getJSClassRefByTypeId(setterArgumentType), typeRegistry()}, value);
    };

    classesByTypeId()[classType]->fieldAccessorsByName[std::string(fieldName)] = {gctx, sctx};
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

        auto smi = reinterpret_cast<StaticMethodInvoker>(invoker);
        return smi(method, {ctx, getJSClassRefByTypeId(argTypes[0]), typeRegistry()}, arguments);
    };

    auto mName = std::string(methodName);
    if(mName[0] == '_') {
        mName.erase(0,1);
    }

    classesByTypeId()[classType]->staticMethodsByName[mName] = fctx;

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
        bool isSigned) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_enum %s", name);

    lazy_bind().push_back([name, enumType](JSGlobalContextRef ctx, JSObjectRef ns) {

        auto container = JSObjectMake(ctx, nullptr, nullptr);
        auto name_str = std::string(name);
        auto targetObject = name_str.find('$') != std::string::npos
                ? ns
                : JSContextGetGlobalObject(ctx);

        EXJSObjectSetValueWithUTF8CStringName(ctx, targetObject, name, container);

        enumByTypeId()[enumType] = container;

    });

}

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
        GenericFunction destructor) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_smart_ptr %s", pointerName);

}

void _embind_register_enum_value(
        TYPEID enumType,
        const char *valueName,
        GenericEnumValue value) {

    __android_log_print(ANDROID_LOG_INFO, "bind", "_embind_register_enum_value %s", valueName);

    lazy_bind().push_back([valueName, value, enumType](JSGlobalContextRef ctx, JSObjectRef ns) {
        auto container = enumByTypeId()[enumType];
        auto jsValue = JSValueMakeNumber(ctx, value);
        EXJSObjectSetValueWithUTF8CStringName(ctx, container, valueName, jsValue);
    });

}

void _embind_register_constant(
        const char *name,
        TYPEID constantType,
        double value) {}

}