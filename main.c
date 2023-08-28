#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <pthread.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <string.h>
#include <stdbool.h>

#define fovt_p(format, args...)\
    do {\
        printf("-----> fovt [%s:%d] ", __FUNCTION__, __LINE__); \
        printf(format, ##args);\
    } while (0)

#define CHECK(cond) do {\
	if (!(cond)) {\
		printf("CHECK failed in %s() %s:%d\n", __func__, __FILE__, __LINE__);\
		return 0;\
	}\
} while(0)

typedef struct native_handle {
    int version;
    int numFds;
    int numInts;
} native_handle_t;

/* support users of drm_gralloc/gbm_gralloc */
#define gralloc_gbm_handle_t gralloc_handle_t
#define gralloc_drm_handle_t gralloc_handle_t

struct gralloc_handle_t {
	native_handle_t base;

	/* dma-buf file descriptor
	 * Must be located first since, native_handle_t is allocated
	 * using native_handle_create(), which allocates space for
	 * sizeof(native_handle_t) + sizeof(int) * (numFds + numInts)
	 * numFds = GRALLOC_HANDLE_NUM_FDS
	 * numInts = GRALLOC_HANDLE_NUM_INTS
	 * Where numFds represents the number of FDs and
	 * numInts represents the space needed for the
	 * remainder of this struct.
	 * And the FDs are expected to be found first following
	 * native_handle_t.
	 */
	int prime_fd;

	/* api variables */
	uint32_t magic; /* differentiate between allocator impls */
	uint32_t version; /* api version */

	uint32_t width; /* width of buffer in pixels */
	uint32_t height; /* height of buffer in pixels */
	uint32_t format; /* pixel format (Android) */
	uint32_t usage; /* android libhardware usage flags */

	uint32_t stride; /* the stride in bytes */
	int data_owner; /* owner of data (for validation) */
	uint64_t modifier __attribute__((aligned(8))); /* buffer modifiers */

	union {
		void *data; /* pointer to struct gralloc_gbm_bo_t */
		uint64_t reserved;
	} __attribute__((aligned(8)));
};

static const uint32_t format_list[] = {
	GBM_FORMAT_C8,
	GBM_FORMAT_RGB332,
	GBM_FORMAT_BGR233,
	GBM_FORMAT_XRGB4444,
	GBM_FORMAT_XBGR4444,
	GBM_FORMAT_RGBX4444,
	GBM_FORMAT_BGRX4444,
	GBM_FORMAT_ARGB4444,
	GBM_FORMAT_ABGR4444,
	GBM_FORMAT_RGBA4444,
	GBM_FORMAT_BGRA4444,
	GBM_FORMAT_XRGB1555,
	GBM_FORMAT_XBGR1555,
	GBM_FORMAT_RGBX5551,
	GBM_FORMAT_BGRX5551,
	GBM_FORMAT_ARGB1555,
	GBM_FORMAT_ABGR1555,
	GBM_FORMAT_RGBA5551,
	GBM_FORMAT_BGRA5551,
	GBM_FORMAT_RGB565,
	GBM_FORMAT_BGR565,
	GBM_FORMAT_RGB888,
	GBM_FORMAT_BGR888,
	GBM_FORMAT_XRGB8888,
	GBM_FORMAT_XBGR8888,
	GBM_FORMAT_RGBX8888,
	GBM_FORMAT_BGRX8888,
	GBM_FORMAT_ARGB8888,
	GBM_FORMAT_ABGR8888,
	GBM_FORMAT_RGBA8888,
	GBM_FORMAT_BGRA8888,
	GBM_FORMAT_XRGB2101010,
	GBM_FORMAT_XBGR2101010,
	GBM_FORMAT_RGBX1010102,
	GBM_FORMAT_BGRX1010102,
	GBM_FORMAT_ARGB2101010,
	GBM_FORMAT_ABGR2101010,
	GBM_FORMAT_RGBA1010102,
	GBM_FORMAT_BGRA1010102,
	GBM_FORMAT_YUYV,
	GBM_FORMAT_YVYU,
	GBM_FORMAT_UYVY,
	GBM_FORMAT_VYUY,
	GBM_FORMAT_AYUV,
	GBM_FORMAT_NV12,
	GBM_FORMAT_YVU420,
};
static const char* format_list_name[] = {
	"GBM_FORMAT_C8",
	"GBM_FORMAT_RGB332",
	"GBM_FORMAT_BGR233",
	"GBM_FORMAT_XRGB4444",
	"GBM_FORMAT_XBGR4444",
	"GBM_FORMAT_RGBX4444",
	"GBM_FORMAT_BGRX4444",
	"GBM_FORMAT_ARGB4444",
	"GBM_FORMAT_ABGR4444",
	"GBM_FORMAT_RGBA4444",
	"GBM_FORMAT_BGRA4444",
	"GBM_FORMAT_XRGB1555",
	"GBM_FORMAT_XBGR1555",
	"GBM_FORMAT_RGBX5551",
	"GBM_FORMAT_BGRX5551",
	"GBM_FORMAT_ARGB1555",
	"GBM_FORMAT_ABGR1555",
	"GBM_FORMAT_RGBA5551",
	"GBM_FORMAT_BGRA5551",
	"GBM_FORMAT_RGB565",
	"GBM_FORMAT_BGR565",
	"GBM_FORMAT_RGB888",
	"GBM_FORMAT_BGR888",
	"GBM_FORMAT_XRGB8888",
	"GBM_FORMAT_XBGR8888",
	"GBM_FORMAT_RGBX8888",
	"GBM_FORMAT_BGRX8888",
	"GBM_FORMAT_ARGB8888",
	"GBM_FORMAT_ABGR8888",
	"GBM_FORMAT_RGBA8888",
	"GBM_FORMAT_BGRA8888",
	"GBM_FORMAT_XRGB2101010",
	"GBM_FORMAT_XBGR2101010",
	"GBM_FORMAT_RGBX1010102",
	"GBM_FORMAT_BGRX1010102",
	"GBM_FORMAT_ARGB2101010",
	"GBM_FORMAT_ABGR2101010",
	"GBM_FORMAT_RGBA1010102",
	"GBM_FORMAT_BGRA1010102",
	"GBM_FORMAT_YUYV",
	"GBM_FORMAT_YVYU",
	"GBM_FORMAT_UYVY",
	"GBM_FORMAT_VYUY",
	"GBM_FORMAT_AYUV",
	"GBM_FORMAT_NV12",
	"GBM_FORMAT_YVU420",
};


struct gbm_module_t {
    struct gbm_device *gbm;
    struct drm_framebuffer *fb;
    struct gbm_bo *gbm_bo;
    pthread_mutex_t mutex;
};

struct drmLocalReq {
    drmModeConnector *mode_conn;
    drmModeRes *mode_res;
    drmModePlaneRes *plane_res;
    drmModeObjectProperties *object_props;
    drmModeAtomicReq *atomic_req;
    drmModeCrtc *mode_crtc;
    drmModeEncoder *mode_encoder;
    drmModeModeInfo *modes;
    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t plane_id;
    uint32_t blob_id;
    uint32_t property_crtc_id;
    uint32_t property_mode_id;
    uint32_t property_active;
    uint32_t property_fb_id;
    uint32_t property_crtc_x;
    uint32_t property_crtc_y;
    uint32_t property_crtc_w;
    uint32_t property_crtc_h;
    uint32_t property_src_x;
    uint32_t property_src_y;
    uint32_t property_src_w;
    uint32_t property_src_h;
};

#define HWC_DRM_BO_MAX_PLANES 4
typedef struct hwc_drm_bo {
  uint32_t width;
  uint32_t height;
  uint32_t format;     /* DRM_FORMAT_* from drm_fourcc.h */
  uint32_t hal_format; /* HAL_PIXEL_FORMAT_* */
  uint32_t usage;
  uint32_t pixel_stride;
  uint32_t pitches[HWC_DRM_BO_MAX_PLANES];
  uint32_t offsets[HWC_DRM_BO_MAX_PLANES];
  uint32_t prime_fds[HWC_DRM_BO_MAX_PLANES];
  uint32_t gem_handles[HWC_DRM_BO_MAX_PLANES];
  uint64_t modifiers[HWC_DRM_BO_MAX_PLANES];
  uint32_t fb_id;
  bool with_modifiers;
  int acquire_fence_fd;
  void *priv;
} hwc_drm_bo_t;


/* funcs */
// gbm
struct gbm_device *gbm_dev_create(bool master);
static int gbm_init(struct gbm_module_t *dmod, bool master);
void check_gbm_device_format(struct gbm_device *gbm);
uint32_t ConvertGbmFormatToDrm(uint32_t gbm_format);
uint32_t FormatToBitsPerPixel(uint32_t format);
int ConvertBoInfo(struct gralloc_handle_t *handle, struct hwc_drm_bo *bo);
static int test_gbm_map(struct gbm_device *gbm);
static int gbm_map(struct gbm_bo *gbm_bo, bool enable_write, void **addr, void **map_data);

// render
static uint32_t compute_color(int x, int y, int w, int h);

// gem handle
int CloseHandle(int fd, uint32_t gem_handle);

// display and atomic
static uint32_t get_property_id(int fd, drmModeObjectProperties *object_props, const char *name);
static int find_display_configuration(int fd, struct drmLocalReq *req);

int main(int argc, char **argv)
{
    struct gbm_module_t gbm_module = {0};
    struct drmLocalReq drmlocalreq;
    int ret = -1;
    int card_fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
    assert(-1 != card_fd);
    drmVersionPtr v = drmGetVersion(card_fd);
    fovt_p("v->name = %s",v->name);

    // drmLocalReq
    /* use find_display_configuration instead */
    // drm_info --> crtc 80 -> conn 255
    // drmlocalreq.mode_res = drmModeGetResources(card_fd);
    // drmlocalreq.crtc_id = drmlocalreq.mode_res->crtcs[0];
    // drmlocalreq.conn_id = drmlocalreq.mode_res->connectors[2];
    // drmlocalreq.mode_conn = drmModeGetConnector(card_fd,drmlocalreq.conn_id);
    // drmlocalreq.modes = drmlocalreq.mode_conn->modes;
    //fovt_p("crtc_id = %d conn_id = %d\n", drmlocalreq.crtc_id, drmlocalreq.conn_id);
    /* basic link */
    find_display_configuration(card_fd, &drmlocalreq);
    fovt_p("%s available mode: %d x %d\n", drmlocalreq.modes[0].name, drmlocalreq.modes[0].hdisplay, drmlocalreq.modes[0].vdisplay);

    drmSetClientCap(card_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    drmlocalreq.plane_res = drmModeGetPlaneResources(card_fd);
    drmlocalreq.plane_id = drmlocalreq.plane_res->planes[0];

    drmSetClientCap(card_fd, DRM_CLIENT_CAP_ATOMIC, 1);
    drmlocalreq.object_props = drmModeObjectGetProperties(card_fd, drmlocalreq.conn_id, DRM_MODE_OBJECT_CONNECTOR);
    drmlocalreq.property_crtc_id = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_ID");
    drmModeFreeObjectProperties(drmlocalreq.object_props);

    drmlocalreq.object_props = drmModeObjectGetProperties(card_fd, drmlocalreq.crtc_id, DRM_MODE_OBJECT_CRTC);
    drmlocalreq.property_active = get_property_id(card_fd, drmlocalreq.object_props, "ACTIVE");
    drmlocalreq.property_mode_id = get_property_id(card_fd, drmlocalreq.object_props, "MODE_ID");
    drmModeFreeObjectProperties(drmlocalreq.object_props);

    drmModeCreatePropertyBlob(card_fd, &drmlocalreq.mode_conn->modes[0], sizeof(drmlocalreq.mode_conn->modes[0]),
            &drmlocalreq.blob_id);

    /* get plane properties */
    drmlocalreq.object_props = drmModeObjectGetProperties(card_fd, drmlocalreq.plane_id, DRM_MODE_OBJECT_PLANE);
    drmlocalreq.property_crtc_id = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_ID");
    drmlocalreq.property_fb_id = get_property_id(card_fd, drmlocalreq.object_props, "FB_ID");
    drmlocalreq.property_crtc_x = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_X");
    drmlocalreq.property_crtc_y = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_Y");
    drmlocalreq.property_crtc_w = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_W");
    drmlocalreq.property_crtc_h = get_property_id(card_fd, drmlocalreq.object_props, "CRTC_H");
    drmlocalreq.property_src_x = get_property_id(card_fd, drmlocalreq.object_props, "SRC_X");
    drmlocalreq.property_src_y = get_property_id(card_fd, drmlocalreq.object_props, "SRC_Y");
    drmlocalreq.property_src_w = get_property_id(card_fd, drmlocalreq.object_props, "SRC_W");
    drmlocalreq.property_src_h = get_property_id(card_fd, drmlocalreq.object_props, "SRC_H");
    drmModeFreeObjectProperties(drmlocalreq.object_props);


    // gbm device
    struct gralloc_handle_t handle = {0};
    ret = gbm_init(&gbm_module, false);
    assert(0 == ret);
    fovt_p("hello gbm! %d\r\n", ret);
    handle.width = drmlocalreq.modes[0].hdisplay;
    handle.height = drmlocalreq.modes[0].vdisplay;
    handle.format = GBM_FORMAT_ARGB8888;
    handle.usage = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
    gbm_module.gbm_bo = gbm_bo_create(gbm_module.gbm, handle.width, handle.height, handle.format, handle.usage);
    test_gbm_map(gbm_module.gbm);
    assert(NULL != gbm_module.gbm_bo);
    handle.prime_fd = gbm_bo_get_fd(gbm_module.gbm_bo);
    handle.stride = gbm_bo_get_stride(gbm_module.gbm_bo);
    fovt_p("create BO, size=%dx%d, fmt=%d, usage=%x prime_fd = %d stride = %d\n",
            handle.width, handle.height, handle.format, handle.usage, handle.prime_fd, handle.stride);
    check_gbm_device_format(gbm_module.gbm);

    // map gbm bo
    void *map_data, * vaddr;
    vaddr = map_data = NULL;
    bool write = true;
    int err = gbm_map(gbm_module.gbm_bo, write, &vaddr, &map_data);
    if(err)
        return err;
    uint32_t *pixel, pixel_size;
    pixel = (uint32_t *)vaddr;
    pixel_size = sizeof(*pixel);
    //(0xFF << 24) | (r << 16) | (g << 8) | b;
    for (uint32_t y = 0; y < handle.height; y++) {
        for (uint32_t x = 0; x < handle.width; x++) {
            pixel[y * (handle.stride / pixel_size) + x] = 0x1 << 24 | compute_color(x, y, handle.width, handle.height);
        }
    }
    gbm_bo_unmap(gbm_module.gbm_bo, map_data);


    // commit bo
    hwc_drm_bo_t commit_bo = {0};
    ConvertBoInfo(&handle, &commit_bo);
    ret = drmPrimeFDToHandle(card_fd, commit_bo.prime_fds[0], &commit_bo.gem_handles[0]);
    fovt_p("handle = %d\n", commit_bo.gem_handles[0]);
    if (ret) {
        fovt_p("failed to import prime fd %d ret=%d\n", commit_bo.prime_fds[0], ret);
        return ret;
    }

    for (int i = 1; i < HWC_DRM_BO_MAX_PLANES; i++) {
        int fd = commit_bo.prime_fds[i];
        if (fd != 0) {
            if (fd != commit_bo.prime_fds[0]) {
                fovt_p("Multiplanar FBs are not supported by this version of composer");
                return -ENOTSUP;
            }
            commit_bo.gem_handles[i] = commit_bo.gem_handles[0];
        }
    }

    ret = drmModeAddFB2(card_fd, commit_bo.width, commit_bo.height, commit_bo.format,
            commit_bo.gem_handles, commit_bo.pitches, commit_bo.offsets, &commit_bo.fb_id,
            0);
    if (ret) {
        fovt_p("could not create drm fb %d!\n", ret);
        return ret;
    }

    /* atomic plane update */
    drmlocalreq.atomic_req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_crtc_id, drmlocalreq.crtc_id);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_fb_id, commit_bo.fb_id);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_crtc_x, 0);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_crtc_y, 0);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_crtc_w, handle.width);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_crtc_h, handle.height);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_src_x, 0);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_src_y, 0);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_src_w, handle.width << 16);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.plane_id, drmlocalreq.property_src_h, handle.height << 16);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.crtc_id, drmlocalreq.property_active, 1);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.crtc_id, drmlocalreq.property_mode_id, drmlocalreq.blob_id);
    drmModeAtomicAddProperty(drmlocalreq.atomic_req, drmlocalreq.conn_id, drmlocalreq.property_crtc_id, drmlocalreq.crtc_id);
    ret = drmModeAtomicCommit(card_fd, drmlocalreq.atomic_req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    drmModeAtomicFree(drmlocalreq.atomic_req);
    if (ret) {
        fovt_p("could commit gbm fb to screen! error = %d\n", ret);
        return ret;
    }

    fovt_p("Gbm Fb Atomic successfull!\n");
    getchar();

    // destroy commit req
    drmModeFreeEncoder(drmlocalreq.mode_encoder);
    drmModeFreeConnector(drmlocalreq.mode_conn);
    drmModeFreePlaneResources(drmlocalreq.plane_res);
    drmModeFreeResources(drmlocalreq.mode_res);
    drmModeDestroyPropertyBlob(card_fd, drmlocalreq.blob_id);

    //  release buffer
    if (commit_bo.fb_id)
        if (drmModeRmFB(card_fd, commit_bo.fb_id))
            fovt_p("Failed to rm fb");

    for (int i = 0; i < HWC_DRM_BO_MAX_PLANES; i++) {
        if (!commit_bo.gem_handles[i])
            continue;

        if (CloseHandle(card_fd, commit_bo.gem_handles[i])) {
            fovt_p("Failed to release gem handle %d", commit_bo.gem_handles[i]);
        } else {
            for (int j = i + 1; j < HWC_DRM_BO_MAX_PLANES; j++)
                if (commit_bo.gem_handles[j] == commit_bo.gem_handles[i])
                    commit_bo.gem_handles[j] = 0;
            commit_bo.gem_handles[i] = 0;
        }
    }

    gbm_bo_destroy(gbm_module.gbm_bo);
    gbm_device_destroy(gbm_module.gbm);

    close(card_fd);
    return 0;
}

static drmModeConnector *find_connector(int fd, drmModeRes *resources) {
    // iterate the connectors
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *connector =
            drmModeGetConnector(fd, resources->connectors[i]);
        // pick the first connected connector
        if (connector->connection == DRM_MODE_CONNECTED) {
            return connector;
        }
        drmModeFreeConnector(connector);
    }
    // no connector found
    return NULL;
}

static drmModeEncoder *find_encoder(int fd, drmModeRes *resources,
                                    drmModeConnector *connector) {
  if (connector->encoder_id) {
    return drmModeGetEncoder(fd, connector->encoder_id);
  }
  // no encoder found
  return NULL;
}

static int find_display_configuration(int fd, struct drmLocalReq *req)
{
    req->mode_res = drmModeGetResources(fd);
    // find a connector
    req->mode_conn = find_connector(fd, req->mode_res);
    if (!req->mode_conn)
    {
        fovt_p("No Connector Found!\r\n");
        goto no_conn;
    }

    // save the connector_id
    req->conn_id = req->mode_conn->connector_id;
    // save the mode
    req->modes = req->mode_conn->modes;
    // find an encoder
    req->mode_encoder = find_encoder(fd, req->mode_res, req->mode_conn);
    if (!req->mode_encoder)
    {
        fovt_p("No Encoder Found!\r\n");
        goto no_encoder;
    }

    // find a CRTC
    if (req->mode_encoder->crtc_id) {
        req->mode_crtc = drmModeGetCrtc(fd, req->mode_encoder->crtc_id);
    }

    // save the crtc_id
    req->crtc_id = req->mode_crtc->crtc_id;

    return 0;
no_encoder:
    drmModeFreeEncoder(req->mode_encoder);
    drmModeFreeConnector(req->mode_conn);
no_conn:
    drmModeFreeResources(req->mode_res);
    return -1;



}


struct gbm_device *gbm_dev_create(bool master)
{
    struct gbm_device *gbm;
    char card_path[128] = "/dev/dri/card1";
    char render_path[128] = "/dev/dri/renderD128";
    char *path = NULL;
    int fd;

    fovt_p("%s \n", master ? card_path : render_path);
    fd = open(master ? card_path : render_path, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		fovt_p("failed to open %s", path);
		return NULL;
	}

	gbm = gbm_create_device(fd);
	if (!gbm) {
		fovt_p("failed to create gbm device");
		close(fd);
	}

	return gbm;
}

static int gbm_init(struct gbm_module_t *dmod, bool master)
{
    int err = 0;

    pthread_mutex_lock(&dmod->mutex);
    if (!dmod->gbm) {
        dmod->gbm = gbm_dev_create(master);
        if (!dmod->gbm)
            err = -EINVAL;
    }
    pthread_mutex_unlock(&dmod->mutex);

    return err;
}

static uint32_t compute_color(int x, int y, int w, int h)
{
	uint32_t pixel = 0x00000000;
	float xratio = (x - w / 2) / ((float)(w / 2));
	float yratio = (y - h / 2) / ((float)(h / 2));
	// If a point is on or inside an ellipse, num <= 1.
	float num = xratio * xratio + yratio * yratio;
	uint32_t g = 255 * num;
	if (g < 256)
		pixel = 0x0FF0000 | (g << 8);
	return pixel;
}

static int test_gbm_map(struct gbm_device *gbm)
{
	uint32_t *pixel, pixel_size;
	struct gbm_bo *bo;
	void *map_data, *addr;

	uint32_t stride = 0;
	const int width = 1920;
	const int height = 1080;

	addr = map_data = NULL;

	bo = gbm_bo_create(gbm, width, height, GBM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR);

    uint32_t flags = 0;
	addr = gbm_bo_map(bo, 0, 0, width, height, 0, &stride, &map_data);
	fovt_p("width = %d height = %d flags = %d stride = %d\n",gbm_bo_get_width(bo), gbm_bo_get_height(bo), flags, stride);
	fovt_p("mapped bo %p at %p\n", bo,addr);


	gbm_bo_unmap(bo, map_data);
	gbm_bo_destroy(bo);
    return 0;
}

static int gbm_map(struct gbm_bo *gbm_bo, bool enable_write, void **addr, void **map_data)
{
    int err = 0;
    int flags = GBM_BO_TRANSFER_READ;
    struct gbm_bo *bo = gbm_bo;
    uint32_t stride;

    if (*map_data)
        return -EINVAL;

    if (enable_write)
        flags |= GBM_BO_TRANSFER_WRITE;

    *addr = gbm_bo_map(bo, 0, 0, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
            flags, &stride, map_data);
    fovt_p("width = %d height = %d flags = %d stride = %d\n",gbm_bo_get_width(bo), gbm_bo_get_height(bo), flags, stride);
    fovt_p("mapped bo %p at %p\n", bo, *addr);
    if (*addr == NULL)
        return -ENOMEM;

    assert(stride == gbm_bo_get_stride(bo));

    return err;
}

void check_gbm_device_format(struct gbm_device *gbm)
{
    int i = 0;
    for(i = 0; i < sizeof(format_list) / sizeof(*format_list); i++)
    {

    if (gbm_device_is_format_supported(gbm, format_list[i], 0))
        fovt_p("gbm_device support format : %s\n", format_list_name[i]);
    }
}

uint32_t ConvertGbmFormatToDrm(uint32_t gbm_format)
{
    switch (gbm_format) {
        case GBM_FORMAT_RGB888:
            return DRM_FORMAT_BGR888;
        case GBM_FORMAT_ARGB8888:
            return DRM_FORMAT_ARGB8888;
        case GBM_FORMAT_XBGR8888:
            return DRM_FORMAT_XBGR8888;
        case GBM_FORMAT_ABGR8888:
            return DRM_FORMAT_ABGR8888;
        case GBM_FORMAT_RGB565:
            return DRM_FORMAT_BGR565;
        case GBM_FORMAT_GR88:
            return DRM_FORMAT_YVU420;
        default:
            fovt_p("Cannot convert gbm format to drm format %u", gbm_format);
            return DRM_FORMAT_INVALID;
    }
}

uint32_t FormatToBitsPerPixel(uint32_t format)
{
  switch (format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
      return 32;
    case DRM_FORMAT_BGR888:
    case GBM_FORMAT_RGB888:
      return 24;
    case DRM_FORMAT_BGR565:
    case GBM_FORMAT_RGB565:
      return 16;
    case DRM_FORMAT_YVU420:
    case GBM_FORMAT_GR88:
      return 12;
    default:
      fovt_p("Cannot convert hal format %u to bpp (returning 32)", format);
      return 32;
  }
}

int ConvertBoInfo(struct gralloc_handle_t *handle, struct hwc_drm_bo *bo)
{
    bo->width = handle->width;
    bo->height = handle->height;
    bo->format = ConvertGbmFormatToDrm(handle->format);
    if (bo->format == DRM_FORMAT_INVALID)
        return -EINVAL;
    bo->usage = handle->usage;
    bo->pixel_stride = (handle->stride * 8) / FormatToBitsPerPixel(bo->format);
    bo->prime_fds[0] = handle->prime_fd;
    bo->pitches[0] = handle->stride;
    bo->offsets[0] = 0;
    return 0;
}

static uint32_t get_property_id(int fd, drmModeObjectProperties *object_props,
                                const char *name)
{
    drmModePropertyPtr property;
    uint32_t i, id = 0;

    for (i = 0; i < object_props->count_props; i++) {
        property = drmModeGetProperty(fd, object_props->props[i]);
        if (!strcmp(property->name, name))
            id = property->prop_id;
        drmModeFreeProperty(property);

        if (id)
            break;
    }
    return id;
}

int CloseHandle(int fd, uint32_t gem_handle) {
  struct drm_gem_close gem_close;

  memset(&gem_close, 0, sizeof(gem_close));

  gem_close.handle = gem_handle;
  int ret = drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
  if (ret)
    fovt_p("Failed to close gem handle %d %d", gem_handle, ret);

  return ret;
}

