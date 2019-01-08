//
// Created by Nicolas Coderre on 1/7/19.
//
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
using namespace emscripten::internal;

// Implemented in JavaScript.  Don't call these directly.
extern "C" {
void _embind_fatal_error(
        const char *name,
        const char *payload) {
    throw new std::runtime_error(name);
}

void _embind_register_void(
        TYPEID voidType,
        const char *name) {}

void _embind_register_bool(
        TYPEID boolType,
        const char *name,
        size_t size,
        bool trueValue,
        bool falseValue) {}

void _embind_register_integer(
        TYPEID integerType,
        const char *name,
        size_t size,
        long minRange,
        unsigned long maxRange) {}

void _embind_register_float(
        TYPEID floatType,
        const char *name,
        size_t size) {}

void _embind_register_std_string(
        TYPEID stringType,
        const char *name) {}

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
        GenericFunction function) {}

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
        GenericFunction destructor) {}

void _embind_register_class_constructor(
        TYPEID classType,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        GenericFunction constructor) {}

void _embind_register_class_function(
        TYPEID classType,
        const char *methodName,
        unsigned argCount,
        const TYPEID argTypes[],
        const char *invokerSignature,
        GenericFunction invoker,
        void *context,
        unsigned isPureVirtual) {}

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