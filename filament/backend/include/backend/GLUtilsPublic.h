#ifndef GL_UTILS_PUBLIC_H
#define GL_UTILS_PUBLIC_H

namespace utils {
	namespace io {
		class ostream;
	}
}

// necessary gl error info
typedef unsigned int GLenum;

#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506

namespace filament {
namespace GLUtils {

typedef void (*checkGLErrorFnPtrType)(utils::io::ostream& out, const char* function, size_t line);
void checkGLError(utils::io::ostream& out, const char* function, size_t line) noexcept;
static checkGLErrorFnPtrType checkGLErrorFnPtr = GLUtils::checkGLError;

void inline setGLErrorFnPtr(checkGLErrorFnPtrType fn) {
    GLUtils::checkGLErrorFnPtr = fn;
}

GLenum glGetErrorPassthrough();

#define CHECK_GL_ERROR(out) { GLUtils::checkGLErrorFnPtr(out, __PRETTY_FUNCTION__, __LINE__); }

}
}

#endif
