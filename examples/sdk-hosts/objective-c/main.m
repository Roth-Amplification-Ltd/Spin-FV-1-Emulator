#include <fv1/sdk.h>
#include <fv1/sdk_debug.h>

__attribute__((objc_root_class))
@interface FV1SDKProbe
+ (int)exerciseHeader:(fv1_sdk_engine *)engine;
@end

@implementation FV1SDKProbe
+ (int)exerciseHeader:(fv1_sdk_engine *)engine {
    fv1_sdk_version_info_v1 info;
    fv1_sdk_version_info_v1_init(&info);
    if (fv1_sdk_get_version_info_v1(&info) != FV1_SDK_OK) return 1;
    if (engine != 0 && fv1_sdk_engine_set_pot(engine, 3u, 0.5f) != FV1_SDK_ERROR_INVALID_ARGUMENT) return 2;
    return fv1_sdk_register_name(FV1_SDK_REG_POT0) == 0 ? 3 : 0;
}
@end
