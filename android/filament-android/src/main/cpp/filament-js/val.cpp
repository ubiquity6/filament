#include <emscripten/emscripten.h>
#include <emscripten/val.h>

using namespace emscripten;
using namespace emscripten::internal;

extern "C" {

void _emval_register_symbol(const char *) {}

void _emval_incref(EM_VAL value) {}
void _emval_decref(EM_VAL value) {}

void _emval_run_destructors(EM_DESTRUCTORS handle) {}

EM_VAL _emval_new_array() { return 0; }
EM_VAL _emval_new_object() { return 0; }
EM_VAL _emval_new_cstring(const char *) { return 0; }

EM_VAL _emval_take_value(TYPEID type, EM_VAR_ARGS argv) { return 0; }

EM_VAL _emval_new(
        EM_VAL value,
        unsigned argCount,
        const TYPEID argTypes[],
        EM_VAR_ARGS argv) { return 0; }

EM_VAL _emval_get_global(const char *name) { return 0; }
EM_VAL _emval_get_module_property(const char *name) { return 0; }
EM_VAL _emval_get_property(EM_VAL object, EM_VAL key) { return 0; }
void _emval_set_property(EM_VAL object, EM_VAL key, EM_VAL value) {}
EM_GENERIC_WIRE_TYPE
_emval_as(EM_VAL value, TYPEID returnType, EM_DESTRUCTORS *destructors) { return 0; }

bool _emval_equals(EM_VAL first, EM_VAL second) { return false; }
bool _emval_strictly_equals(EM_VAL first, EM_VAL second) { return false; }
bool _emval_greater_than(EM_VAL first, EM_VAL second) { return false; }
bool _emval_less_than(EM_VAL first, EM_VAL second) { return false; }

EM_VAL _emval_call(
        EM_VAL value,
        unsigned argCount,
        const TYPEID argTypes[],
        EM_VAR_ARGS argv) { return 0; }

// DO NOT call this more than once per signature. It will
// leak generated function objects!
EM_METHOD_CALLER _emval_get_method_caller(
        unsigned argCount, // including return value
        const TYPEID argTypes[]) { return 0; }
EM_GENERIC_WIRE_TYPE _emval_call_method(
        EM_METHOD_CALLER caller,
        EM_VAL handle,
        const char *methodName,
        EM_DESTRUCTORS *destructors,
        EM_VAR_ARGS argv) { return 0; }
void _emval_call_void_method(
        EM_METHOD_CALLER caller,
        EM_VAL handle,
        const char *methodName,
        EM_VAR_ARGS argv) {}
EM_VAL _emval_typeof(EM_VAL value) { return 0; }
bool _emval_instanceof(EM_VAL object, EM_VAL constructor) { return false; }
bool _emval_in(EM_VAL item, EM_VAL object) { return false; }
bool _emval_delete(EM_VAL object, EM_VAL property) { return false; }

}