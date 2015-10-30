
#include "config.h"

#include "avscale.h"

unsigned avscale_version(void)
{
    return LIBAVUTIL_VERSION_INT;
}

const char *avscale_configuration(void)
{
    return LIBAV_CONFIGURATION;
}

const char *avscale_license(void)
{
#define LICENSE_PREFIX "libavutil license: "
    return LICENSE_PREFIX LIBAV_LICENSE + sizeof(LICENSE_PREFIX) - 1;
}
