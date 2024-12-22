/*
Basic KMSDRM with double buffer swapped (page flipped) when vertical blank
based on kmscube
 */

#include "basic4.h"

int64_t get_time_ns(void) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_nsec + tv.tv_sec * NSEC_PER_SEC;
}

int create_program(const char* vs_src, const char* fs_src) {
    puts("common.c: create_program");
    GLuint vertex_shader, fragment_shader, program;
    GLint ret;

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    if (vertex_shader == 0) {
        printf("vertex shader creation failed!:\n");
        return -1;
    }

    glShaderSource(vertex_shader, 1, &vs_src, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        char* log;

        printf("vertex shader compilation failed!:\n");
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);
        if (ret > 1) {
            log = malloc(ret);
            glGetShaderInfoLog(vertex_shader, ret, NULL, log);
            printf("%s", log);
            free(log);
        }

        return -1;
    }

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragment_shader == 0) {
        printf("fragment shader creation failed!:\n");
        return -1;
    }
    glShaderSource(fragment_shader, 1, &fs_src, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        char* log;

        printf("fragment shader compilation failed!:\n");
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1) {
            log = malloc(ret);
            glGetShaderInfoLog(fragment_shader, ret, NULL, log);
            printf("%s", log);
            free(log);
        }

        return -1;
    }

    program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    return program;
}

int link_program(unsigned program) {
    puts("common.c: link_program");
    GLint ret;

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ret);
    if (!ret) {
        char* log;

        printf("program linking failed!:\n");
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1) {
            log = malloc(ret);
            glGetProgramInfoLog(program, ret, NULL, log);
            printf("%s", log);
            free(log);
        }

        return -1;
    }

    return 0;
}

static int get_resources(int fd, drmModeRes** resources) {
    puts("drm-common.c:get_resources:find_drm_device: drmModeGetResources");
    *resources = drmModeGetResources(fd);
    if (*resources == NULL)
        return -1;
    return 0;
}

static int find_drm_device(drmModeRes** resources) {
    drmDevicePtr devices[MAX_DRM_DEVICES] = { NULL };
    int num_devices, fd = -1;

    puts("drm-common.c:find_drm_device: drmGetDevices2");
    num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
    if (num_devices < 0) {
        printf("drmGetDevices2 failed: %s\n", strerror(-num_devices));
        return -1;
    }

    for (int i = 0; i < num_devices; i++) {
        drmDevicePtr device = devices[i];
        int ret;

        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;
        /* OK, it's a primary device. If we can get the
         * drmModeResources, it means it's also a
         * KMS-capable device.
         */
        printf("drm-common.c:find_drm_device: open %s\n", device->nodes[DRM_NODE_RENDER]);
        fd = open(device->nodes[DRM_NODE_PRIMARY], O_RDWR);
        if (fd < 0)
            continue;
        ret = get_resources(fd, resources);
        if (!ret)
            break;
        puts("drm-common.c:find_drm_device: close");
        close(fd);
        fd = -1;
    }
    puts("drm-common.c:find_drm_device: drmFreeDevices");
    drmFreeDevices(devices, num_devices);

    if (fd < 0)
        printf("no drm device found!\n");
    return fd;
}

static int32_t find_crtc_for_encoder(const drmModeRes* resources,
    const drmModeEncoder* encoder) {
    int i;

    for (i = 0; i < resources->count_crtcs; i++) {
        /* possible_crtcs is a bitmask as described here:
         * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
         */
        const uint32_t crtc_mask = 1 << i;
        const uint32_t crtc_id = resources->crtcs[i];
        if (encoder->possible_crtcs & crtc_mask) {
            return crtc_id;
        }
    }

    /* no match found */
    return -1;
}

static int32_t find_crtc_for_connector(const struct drm* drm, const drmModeRes* resources,
    const drmModeConnector* connector) {
    int i;

    for (i = 0; i < connector->count_encoders; i++) {
        const uint32_t encoder_id = connector->encoders[i];
        puts("drm-common.c: drmModeGetEncoder");
        drmModeEncoder* encoder = drmModeGetEncoder(drm->fd, encoder_id);

        if (encoder) {
            const int32_t crtc_id = find_crtc_for_encoder(resources, encoder);

            puts("drm-common.c: drmModeFreeEncoder");
            drmModeFreeEncoder(encoder);
            if (crtc_id != 0) {
                return crtc_id;
            }
        }
    }

    /* no match found */
    return -1;
}

static drmModeConnector* find_drm_connector(int fd, drmModeRes* resources,
    int connector_id) {
    drmModeConnector* connector = NULL;
    int i;

    if (connector_id >= 0) {
        if (connector_id >= resources->count_connectors)
            return NULL;

        puts("drm-common.c:find_drm_connector: drmModeGetConnector");
        connector = drmModeGetConnector(fd, resources->connectors[connector_id]);
        if (connector && connector->connection == DRM_MODE_CONNECTED)
            return connector;

        puts("drm-common.c:find_drm_connector: drmModeFreeConnector");
        drmModeFreeConnector(connector);
        return NULL;
    }

    for (i = 0; i < resources->count_connectors; i++) {
        puts("drm-common.c:find_drm_connector: drmModeGetConnector");
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector && connector->connection == DRM_MODE_CONNECTED) {
            /* it's connected, let's use this! */
            break;
        }
        puts("drm-common.c:find_drm_connector: drmModeFreeConnector");
        drmModeFreeConnector(connector);
        connector = NULL;
    }

    return connector;
}

int init_drm(struct drm* drm, const char* device, const char* mode_str,
    int connector_id, unsigned int vrefresh, unsigned int count, bool nonblocking) {
    drmModeRes* resources;
    drmModeConnector* connector = NULL;
    drmModeEncoder* encoder = NULL;
    int i, ret, area;

    if (device) {
        puts("drm-common.c:init_drm: open");
        drm->fd = open(device, O_RDWR);
        ret = get_resources(drm->fd, &resources);
        if (ret < 0 && errno == EOPNOTSUPP)
            printf("%s does not look like a modeset device\n", device);
    } else {
        drm->fd = find_drm_device(&resources);
    }

    if (drm->fd < 0) {
        printf("could not open drm device\n");
        return -1;
    }

    if (!resources) {
        printf("drmModeGetResources failed: %s\n", strerror(errno));
        return -1;
    }

    /* find a connected connector: */
    connector = find_drm_connector(drm->fd, resources, connector_id);

    if (!connector) {
        /* we could be fancy and listen for hotplug events and wait for
         * a connector..
         */
        printf("no connected connector!\n");
        return -1;
    }

    /* find user requested mode: */
    if (mode_str && *mode_str) {
        for (i = 0; i < connector->count_modes; i++) {
            drmModeModeInfo* current_mode = &connector->modes[i];

            if (strcmp(current_mode->name, mode_str) == 0) {
                if (vrefresh == 0 || current_mode->vrefresh == vrefresh) {
                    drm->mode = current_mode;
                    break;
                }
            }
        }
        if (!drm->mode)
            printf("requested mode not found, using default mode!\n");
    }

    /* find preferred mode or the highest resolution mode: */
    if (!drm->mode) {
        for (i = 0, area = 0; i < connector->count_modes; i++) {
            drmModeModeInfo* current_mode = &connector->modes[i];

            if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
                drm->mode = current_mode;
                break;
            }

            int current_area = current_mode->hdisplay * current_mode->vdisplay;
            if (current_area > area) {
                drm->mode = current_mode;
                area = current_area;
            }
        }
    }

    if (!drm->mode) {
        printf("could not find mode!\n");
        return -1;
    }

    /* find encoder: */
    for (i = 0; i < resources->count_encoders; i++) {
        puts("drm-common.c:init_drm: drmModeGetEncoder");
        encoder = drmModeGetEncoder(drm->fd, resources->encoders[i]);
        if (encoder->encoder_id == connector->encoder_id)
            break;
        puts("drm-common.c:init_drm: drmModeFreeEncoder");
        drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (encoder) {
        drm->crtc_id = encoder->crtc_id;
    } else {
        int32_t crtc_id = find_crtc_for_connector(drm, resources, connector);
        if (crtc_id == -1) {
            printf("no crtc found!\n");
            return -1;
        }

        drm->crtc_id = crtc_id;
    }

    for (i = 0; i < resources->count_crtcs; i++) {
        if (resources->crtcs[i] == drm->crtc_id) {
            drm->crtc_index = i;
            break;
        }
    }

    puts("drm-common.c:init_drm: drmModeFreeResources");
    drmModeFreeResources(resources);

    drm->connector_id = connector->connector_id;
    drm->count = count;
    drm->nonblocking = nonblocking;

    return 0;
}

int init_surface(struct gbm* gbm, uint64_t modifier) {
    if (gbm_surface_create_with_modifiers) {
        puts("common.c:init_gbm:init_surface: gbm_surface_create_with_modifiers");
        gbm->surface = gbm_surface_create_with_modifiers(gbm->dev, gbm->width, gbm->height, gbm->format, &modifier, 1);
    }

    if (!gbm->surface) {
        if (modifier != DRM_FORMAT_MOD_LINEAR) {
            printf("Modifiers requested but support isn't available\n");
            return -2;
        }
        puts("common.c:init_gbm:init_surface: gbm_surface_create");
        gbm->surface = gbm_surface_create(gbm->dev, gbm->width, gbm->height, gbm->format, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    }

    if (!gbm->surface) {
        printf("failed to create gbm surface\n");
        return -3;
    }

    return 0;
}

int init_gbm(struct gbm* gbm, int drm_fd, int w, int h, uint32_t format,
    uint64_t modifier) {
    puts("common.c:init_gbm: gbm_create_device");
    gbm->dev = gbm_create_device(drm_fd);
    if (!gbm->dev)
        return -1;

    gbm->format = format;
    gbm->surface = NULL;

    gbm->width = w;
    gbm->height = h;

    return init_surface(gbm, modifier);
}

static bool has_ext(const char* extension_list, const char* ext) {
    const char* ptr = extension_list;
    int len = strlen(ext);

    if (ptr == NULL || *ptr == '\0')
        return false;

    while (true) {
        ptr = strstr(ptr, ext);
        if (!ptr)
            return false;

        if (ptr[len] == ' ' || ptr[len] == '\0')
            return true;

        ptr += len;
    }
}

static int match_config_to_visual(EGLDisplay egl_display, EGLint visual_id, EGLConfig* configs, int count) {
    int i;

    for (i = 0; i < count; ++i) {
        EGLint id;

        if (!eglGetConfigAttrib(egl_display,
            configs[i], EGL_NATIVE_VISUAL_ID,
            &id))
            continue;

        printf("common.c:init_egl:match_config_to_visual: eglGetConfigAttrib id=%d visual_id=%d\n", id, visual_id);
        if (id == visual_id)
            return i;
    }
    puts("common.c:init_egl:match_config_to_visual: Visual ID NOT FOUND");
    return -1;
}

static bool egl_choose_config(EGLDisplay egl_display, const EGLint* attribs,
    EGLint visual_id, EGLConfig* config_out) {
    EGLint count = 0;
    EGLint matched = 0;
    EGLConfig* configs;
    int config_index = -1;

    printf("common.c:init_egl:egl_choose_config: eglGetConfigs visual_id=%d\n", visual_id);
    if (!eglGetConfigs(egl_display, NULL, 0, &count) || count < 1) {
        printf("No EGL configs to choose from.\n");
        return false;
    }
    configs = malloc(count * sizeof * configs);
    if (!configs)
        return false;

    puts("common.c:init_egl:egl_choose_config: eglChooseConfig");
    if (!eglChooseConfig(egl_display, attribs, configs,
        count, &matched) || !matched) {
        printf("No EGL configs with appropriate attributes.\n");
        goto out;
    }
    printf("common.c:init_egl:egl_choose_config: Found %d matched configs\n", matched);

    if (!visual_id) {
        config_index = 0;
        printf("common.c:init_egl:egl_choose_config: Use first[0] matched configs\n");
    }


    if (config_index == -1)
        config_index = match_config_to_visual(egl_display, visual_id, configs, matched);

    if (config_index != -1) {
        *config_out = configs[config_index];
        printf("common.c:init_egl:egl_choose_config: Use index [%d] matched configs\n", config_index);
    }

out:
    free(configs);
    if (config_index == -1)
        return false;

    return true;
}

int init_egl(struct egl* egl, const struct gbm* gbm, int samples) {
    EGLint major, minor;

    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

#ifdef DRM_FORMAT_USE_NO_TRANSPARENCY
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SAMPLES, samples,
        EGL_NONE
    };
#else
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,               // Alpha component size (if needed)
        EGL_DEPTH_SIZE, 24,              // Depth buffer size
        EGL_STENCIL_SIZE, 8,             // Stencil buffer size
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SAMPLES, samples,
        EGL_NONE
    };
#endif

    const char* egl_exts_client, * egl_exts_dpy, * gl_exts;

#define get_proc_client(ext, name) do { \
		if (has_ext(egl_exts_client, #ext)) { \
			puts("common.c:init_egl: eglGetProcAddress " #name); \
			egl->name = (void *)eglGetProcAddress(#name); \
		} \
		} while (0)
#define get_proc_dpy(ext, name) do { \
		if (has_ext(egl_exts_dpy, #ext)) { \
			puts("common.c:init_egl: eglGetProcAddress " #name); \
			egl->name = (void*) eglGetProcAddress(#name); \
		} \
		} while (0)

#define get_proc_gl(ext, name) do { \
		if (has_ext(gl_exts, #ext)) {\
			puts("common.c:init_egl: eglGetProcAddress " #name); \
			egl->name = (void*) eglGetProcAddress(#name); \
		} \
		} while (0)

    puts("common.c:init_egl: eglQueryString");
    egl_exts_client = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    get_proc_client(EGL_EXT_platform_base, eglGetPlatformDisplayEXT);

    if (egl->eglGetPlatformDisplayEXT) {
        puts("common.c:init_egl: eglGetPlatformDisplayEXT");
        egl->display = egl->eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR,
            gbm->dev, NULL);
    } else {
        puts("common.c:init_egl: eglGetDisplay");
        egl->display = eglGetDisplay((EGLNativeDisplayType) gbm->dev);
    }

    puts("common.c:init_egl: eglInitialize");
    if (!eglInitialize(egl->display, &major, &minor)) {
        printf("failed to initialize\n");
        return -1;
    }

    puts("common.c:init_egl: eglQueryString");
    egl_exts_dpy = eglQueryString(egl->display, EGL_EXTENSIONS);
    // get_proc_dpy(EGL_KHR_image_base, eglCreateImageKHR);
    // get_proc_dpy(EGL_KHR_image_base, eglDestroyImageKHR);
    // get_proc_dpy(EGL_KHR_fence_sync, eglCreateSyncKHR);
    // get_proc_dpy(EGL_KHR_fence_sync, eglDestroySyncKHR);
    // get_proc_dpy(EGL_KHR_fence_sync, eglWaitSyncKHR);
    // get_proc_dpy(EGL_KHR_fence_sync, eglClientWaitSyncKHR);
    // get_proc_dpy(EGL_ANDROID_native_fence_sync, eglDupNativeFenceFDANDROID);

    egl->modifiers_supported = has_ext(egl_exts_dpy,
        "EGL_EXT_image_dma_buf_import_modifiers");

    printf("init_egl: Using display %p with EGL version %d.%d\n",
        egl->display, major, minor);

    printf("===================================\n");
    printf("EGL information:\n");
    printf("  version: \"%s\"\n", eglQueryString(egl->display, EGL_VERSION));
    printf("  vendor: \"%s\"\n", eglQueryString(egl->display, EGL_VENDOR));
    printf("  client extensions: \"%s\"\n", egl_exts_client);
    printf("  display extensions: \"%s\"\n", egl_exts_dpy);
    printf("===================================\n");

    puts("common.c:init_egl: eglBindAPI");
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("failed to bind api EGL_OPENGL_ES_API\n");
        return -1;
    }

    if (!egl_choose_config(egl->display, config_attribs, gbm->format,
        &egl->config)) {
        printf("failed to choose config\n");
        return -1;
    }

    EGLint red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type;
    eglGetConfigAttrib(egl->display, egl->config, EGL_RED_SIZE, &red_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_GREEN_SIZE, &green_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_BLUE_SIZE, &blue_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_ALPHA_SIZE, &alpha_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_DEPTH_SIZE, &depth_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_STENCIL_SIZE, &stencil_size);
    eglGetConfigAttrib(egl->display, egl->config, EGL_SURFACE_TYPE, &surface_type);
    eglGetConfigAttrib(egl->display, egl->config, EGL_RENDERABLE_TYPE, &render_type);
    printf("Chosen Config R:%d G:%d B:%d A:%d Depth:%d Stencil:%d Surface=0x%08X Render=0x%08X\n", red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type);

    puts("common.c:init_egl: eglCreateContext");
    egl->context = eglCreateContext(egl->display, egl->config,
        EGL_NO_CONTEXT, context_attribs);
    if (egl->context == EGL_NO_CONTEXT) {
        printf("failed to create context\n");
        return -1;
    }

    if (!gbm->surface) {
        egl->surface = EGL_NO_SURFACE;
    } else {
        puts("common.c:init_egl: eglCreateWindowSurface");
        egl->surface = eglCreateWindowSurface(egl->display, egl->config,
            (EGLNativeWindowType) gbm->surface, NULL);
        if (egl->surface == EGL_NO_SURFACE) {
            printf("failed to create egl surface\n");
            return -1;
        }
    }

    /* connect the context to the surface */
    puts("common.c:init_egl: eglMakeCurrent");
    eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);

    gl_exts = (char*) glGetString(GL_EXTENSIONS);
    printf("OpenGL ES 2.x information:\n");
    printf("  version: \"%s\"\n", glGetString(GL_VERSION));
    printf("  shading language version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
    printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
    printf("  extensions: \"%s\"\n", gl_exts);
    printf("===================================\n");

    return 0;
}

static void drm_fb_destroy_callback(struct gbm_bo* bo, void* data) {
    puts("drm-common.c: gbm_bo_get_device");
    puts("drm-common.c: gbm_device_get_fd");
    int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
    struct drm_fb* fb = data;

    if (fb->fb_id) {
        puts("drm-common.c: drmModeRmFB");
        drmModeRmFB(drm_fd, fb->fb_id);
    }

    free(fb);
}

struct drm_fb* drm_fb_get_from_bo(struct gbm_bo* bo) {
    // puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_device");
    // puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_device_get_fd");
    int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
    // puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_user_data");
    struct drm_fb* fb = gbm_bo_get_user_data(bo);
    uint32_t width, height, format,
        strides[4] = { 0 }, handles[4] = { 0 },
        offsets[4] = { 0 }, flags = 0;
    int ret = -1;

    if (fb)
        return fb;

    fb = calloc(1, sizeof * fb);
    fb->bo = bo;

    puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_width");
    width = gbm_bo_get_width(bo);
    puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_height");
    height = gbm_bo_get_height(bo);
    puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_format");
    format = gbm_bo_get_format(bo);

    if (gbm_bo_get_handle_for_plane && gbm_bo_get_modifier &&
        gbm_bo_get_plane_count && gbm_bo_get_stride_for_plane &&
        gbm_bo_get_offset) {

        uint64_t modifiers[4] = { 0 };
        puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_modifier");
        modifiers[0] = gbm_bo_get_modifier(bo);
        const int num_planes = gbm_bo_get_plane_count(bo);
        printf("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_plane_count => num_planes=%d\n", num_planes);
        for (int i = 0; i < num_planes; i++) {
            printf("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_handle_for_plane plane_no=%d\n", i);
            handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
            printf("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_stride_for_plane plane_no=%d\n", i);
            strides[i] = gbm_bo_get_stride_for_plane(bo, i);
            printf("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_get_offset plane_no=%d\n", i);
            offsets[i] = gbm_bo_get_offset(bo, i);
            modifiers[i] = modifiers[0];
        }

        if (modifiers[0] && modifiers[0] != DRM_FORMAT_MOD_INVALID) {
            flags = DRM_MODE_FB_MODIFIERS;
            printf("drm-common.c:legacy_run:drm_fb_get_from_bo: Using modifier %" PRIx64 "\n", modifiers[0]);
        }

        puts("drm-common.c:legacy_run:drm_fb_get_from_bo: drmModeAddFB2WithModifiers");
        ret = drmModeAddFB2WithModifiers(drm_fd, width, height,
            format, handles, strides, offsets,
            modifiers, &fb->fb_id, flags);
    }

    if (ret) {
        if (flags)
            fprintf(stderr, "Modifiers failed!\n");

        memcpy(handles, (uint32_t[4]) { gbm_bo_get_handle(bo).u32, 0, 0, 0 }, 16);
        memcpy(strides, (uint32_t[4]) { gbm_bo_get_stride(bo), 0, 0, 0 }, 16);
        memset(offsets, 0, 16);
        puts("drm-common.c:legacy_run:drm_fb_get_from_bo: drmModeAddFB2");
        ret = drmModeAddFB2(drm_fd, width, height, format, handles, strides, offsets, &fb->fb_id, 0);
    }

    if (ret) {
        printf("failed to create fb: %s\n", strerror(errno));
        free(fb);
        return NULL;
    }

    puts("drm-common.c:legacy_run:drm_fb_get_from_bo: gbm_bo_set_user_data");
    gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

    return fb;
}

static void page_flip_handler(int fd, unsigned int frame,
    unsigned int sec, unsigned int usec, void* data) {
    /* suppress 'unused parameter' warnings */
    (void) fd, (void) frame, (void) sec, (void) usec;

    int* waiting_for_flip = data;
    // printf("drm-legacy.c: page_flip_handler BEGIN waiting_for_flip=%d\n", *waiting_for_flip);
    *waiting_for_flip = 0;
    // printf("drm-legacy.c: page_flip_handler END waiting_for_flip=%d\n", *waiting_for_flip);
}

static int run_gl_loop(const struct gbm* gbm, const struct egl* egl, struct drm* drm) {
    fd_set fds;
    drmEventContext evctx = {
            .version = 2,
            .page_flip_handler = page_flip_handler,
    };
    struct gbm_bo* bo;
    struct drm_fb* fb;
    uint32_t i = 0;
    int64_t start_time, report_time, cur_time;
    int ret;

    if (gbm->surface) {
        puts("drm-legacy.c:legacy_run: eglSwapBuffers");
        eglSwapBuffers(egl->display, egl->surface);
        puts("drm-legacy.c:legacy_run: gbm_surface_lock_front_buffer");
        bo = gbm_surface_lock_front_buffer(gbm->surface);
    } else {
        printf("FATAL ERROR, gbm->surface is NULL\n");
        return -1;
    }
    fb = drm_fb_get_from_bo(bo);
    if (!fb) {
        printf("Failed to get a new framebuffer BO\n");
        return -1;
    }

    /* set mode: */
    puts("drm-legacy.c:legacy_run: drmModeSetCrtc");
    ret = drmModeSetCrtc(drm->fd, drm->crtc_id, fb->fb_id, 0, 0, &drm->connector_id, 1, drm->mode);
    if (ret) {
        printf("failed to set mode: %s\n", strerror(errno));
        return ret;
    }

    start_time = report_time = get_time_ns();

    // drm->count = 500;
    while (i < drm->count) {
        // printf("\ndrm-legacy.c:legacy_run: i=%d drm.count=%d\n", i, drm->count);
        // unsigned frame = i;
        struct gbm_bo* next_bo;
        int waiting_for_flip = 1;

        /* Start fps measuring on second frame, to remove the time spent
         * compiling shader, etc, from the fps:
         */
        if (i == 1) {
            start_time = report_time = get_time_ns();
        }

        // if (!gbm->surface) {
        //     puts("drm-legacy.c: glBindFramebuffer");
        //     glBindFramebuffer(GL_FRAMEBUFFER, egl->fbs[frame % NUM_BUFFERS].fb);
        // }

        // puts("drm-legacy.c:legacy_run: egl->draw");
        // egl->draw(i++);
        glClearColor(0.0f, 0.5f, 1.0f, 1.0f); // Blue background
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (gbm->surface) {
            // puts("drm-legacy.c:legacy_run: eglSwapBuffers");
            eglSwapBuffers(egl->display, egl->surface);
            // printf("common.c:legacy_run: gbm_surface_lock_front_buffer => next_bo=%p\n", next_bo);
            next_bo = gbm_surface_lock_front_buffer(gbm->surface);
        }
        fb = drm_fb_get_from_bo(next_bo);
        if (!fb) {
            printf("Failed to get a new framebuffer BO\n");
            return -1;
        }

        /*
         * Here you could also update drm plane layers if you want
         * hw composition
         */

         // printf("drm-legacy.c:legacy_run: drmModePageFlip waiting_for_flip=%d\n", waiting_for_flip);
        ret = drmModePageFlip(drm->fd, drm->crtc_id, fb->fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
        if (ret) {
            printf("failed to queue page flip: %s\n", strerror(errno));
            return -1;
        }

        while (waiting_for_flip) {
            FD_ZERO(&fds);
            FD_SET(0, &fds);
            FD_SET(drm->fd, &fds);

            // puts("drm-legacy.c:legacy_run: select");
            ret = select(drm->fd + 1, &fds, NULL, NULL, NULL);
            if (ret < 0) {
                printf("select err: %s\n", strerror(errno));
                return ret;
            } else if (ret == 0) {
                printf("select timeout!\n");
                return -1;
            } else if (FD_ISSET(0, &fds) && !drm->nonblocking) {
                printf("user interrupted!\n");
                return 0;
            }
            // puts("drm-legacy.c:legacy_run: drmHandleEvent");
            drmHandleEvent(drm->fd, &evctx);
        }

        cur_time = get_time_ns();
        if (cur_time > (report_time + 2 * NSEC_PER_SEC)) {
            double elapsed_time = cur_time - start_time;
            double secs = elapsed_time / (double) NSEC_PER_SEC;
            unsigned frames = i - 1;  /* first frame ignored */
            printf("Rendered %u frames in %f sec (%f fps)\n", frames, secs, (double) frames / secs);
            report_time = cur_time;
        }

        /* release last buffer to render on again: */
        if (gbm->surface) {
            // puts("drm-legacy.c:legacy_run: gbm_surface_release_buffer");
            gbm_surface_release_buffer(gbm->surface, bo);
        }
        bo = next_bo;
        i++;
    }

    // finish_perfcntrs();

    cur_time = get_time_ns();
    double elapsed_time = cur_time - start_time;
    double secs = elapsed_time / (double) NSEC_PER_SEC;
    unsigned frames = i - 1;  /* first frame ignored */
    printf("Rendered %u frames in %f sec (%f fps)\n", frames, secs, (double) frames / secs);

    // dump_perfcntrs(frames, elapsed_time);

    return 0;
}

static struct gbm gbm;
static struct drm drm;
static struct egl egl;

// Vertex Shader Source Code
const char* vertexShaderSource =
"#version 320 es\n"
"precision mediump float;\n"
"#if __VERSION__ >= 130\n"
"#define COMPAT_VARYING out\n"
"#define COMPAT_ATTRIBUTE in\n"
"#define COMPAT_TEXTURE texture\n"
"#else\n"
"#define COMPAT_VARYING varying \n"
"#define COMPAT_ATTRIBUTE attribute \n"
"#define COMPAT_TEXTURE texture2D\n"
"#endif\n"
"COMPAT_ATTRIBUTE vec4 a_Position;\n"
"void main() {\n"
"    gl_Position = a_Position;\n"
"}";

// Fragment Shader Source Code
const char* fragmentShaderSource =
"#version 320 es\n"
"precision mediump float;\n"
"#if __VERSION__ >= 130\n"
"#define COMPAT_VARYING in\n"
"#define COMPAT_TEXTURE texture\n"
"out vec4 FragColor;\n"
"#else\n"
"#define COMPAT_VARYING varying\n"
"#define FragColor gl_FragColor\n"
"#define COMPAT_TEXTURE texture2D\n"
"#endif\n"
"uniform vec4 u_Color;\n"
"void main(void) {\n"
"    FragColor = u_Color;\n"
"}\n";

int main(int argc, char* argv[]) {
    const char* device = NULL;
    // const char* video = NULL;
    // const char* shadertoy = NULL;
    // const char* perfcntr = NULL;
    char mode_str[DRM_DISPLAY_MODE_LEN] = "";
    char* p;
    // enum mode mode = SMOOTH;
#ifdef DRM_FORMAT_USE_NO_TRANSPARENCY
    uint32_t format = DRM_FORMAT_XRGB8888;
#else
    uint32_t format = DRM_FORMAT_ARGB8888;
#endif
    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    int samples = 0;
    // int atomic = 0;
    // int gears = 0;
    // int offscreen = 0;
    int connector_id = -1;
    // int opt;
    // unsigned int len;
    unsigned int vrefresh = 0;
    unsigned int count = 500;
    bool nonblocking = false;
    int ret;

    ret = init_drm(&drm, device, mode_str, connector_id, vrefresh, count, nonblocking);
    if (ret) {
        printf("failed to initialize DRM. Code %d\n", ret);
        return ret;
    } else {
        printf("initialize DRM [OK]\n");
    }

    ret = init_gbm(&gbm, drm.fd, drm.mode->hdisplay, drm.mode->vdisplay, format, modifier);
    if (ret) {
        printf("failed to initialize GBM. Code %d\n", ret);
        return ret;
    } else {
        printf("initialize GBM gbm.surface=%p [OK]\n", gbm.surface);
    }

    ret = init_egl(&egl, &gbm, samples);
    if (ret) {
        printf("failed to initialize EGL. Code %d\n", ret);
        return ret;
    } else {
        printf("initialize EGL [OK]\n");
    }

    GLuint program = create_program(vertexShaderSource, fragmentShaderSource);
    if (program < 0) {  // return program negative = ERROR
        printf("failed to compile shader. Code %d\n", program);
        return program;
    } else {
        printf("Compile shader [OK]\n");
    }
    ret = link_program(program);
    if (ret) {
        printf("failed to link shader. Code %d\n", ret);
        return ret;
    } else {
        printf("Link shader [OK]\n");
    }
    glUseProgram(program);

    // ============================================================================================
    // GL init setup
    // ============================================================================================
    GLfloat vertices[] = {
        0.0f,  0.5f,  // Top
       -0.5f, -0.5f,  // Bottom left
        0.5f, -0.5f   // Bottom right
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint position = glGetAttribLocation(program, "a_Position");
    glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*) 0);
    glEnableVertexAttribArray(position);
    glViewport(0, 0, gbm.width, gbm.height);
    printf("Windows created %dx%d [OK]\n", gbm.width, gbm.height);

    // ============================================================================================
    // GL drawing loop
    // ============================================================================================
    run_gl_loop(&gbm, &egl, &drm);

    return 0;
}

