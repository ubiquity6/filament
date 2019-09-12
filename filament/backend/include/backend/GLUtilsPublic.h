#ifndef GL_UTILS_PUBLIC_H
#define GL_UTILS_PUBLIC_H

namespace utils {
	namespace io {
		class ostream;
	}
}

namespace filament {
namespace GLUtils {

typedef void (*checkGLErrorFnPtrType)(utils::io::ostream& out, const char* function, size_t line);
void checkGLError(utils::io::ostream& out, const char* function, size_t line) noexcept;
static checkGLErrorFnPtrType checkGLErrorFnPtr = GLUtils::checkGLError;

void inline setGLErrorFnPtr(checkGLErrorFnPtrType fn) {
    GLUtils::checkGLErrorFnPtr = fn;
}

#define CHECK_GL_ERROR(out) { GLUtils::checkGLErrorFnPtr(out, __PRETTY_FUNCTION__, __LINE__); }

}
}

#endif
