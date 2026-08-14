/* miniaudio is kept in its own C translation unit so our strict C++ warning
   policy does not flood builds with diagnostics from third-party code. */
#if defined(FV1_HAVE_MINIAUDIO)
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#else
int fv1_miniaudio_backend_not_built = 0;
#endif
