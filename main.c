#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#define fovt_p(format, args...)\
    do {\
        printf("-----> fovt [%s:%d] ", __FUNCTION__, __LINE__); \
        printf(format, ##args);\
    } while (0)

int main(int argc, char **argv)
{
    struct gbm_device *gbm_device;
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    assert(-1 != fd);
    drmVersionPtr v = drmGetVersion(fd);
    fovt_p("v->name = %s",v->name);

    fovt_p("hello gbm!\r\n");
    return 0;
}
