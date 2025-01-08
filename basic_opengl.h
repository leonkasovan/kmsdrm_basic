/*
 * Copyright (c) 2017 Rob Clark <rclark@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _COMMON_H
#define _COMMON_H

#include "gl/glad.h"
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libdrm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
#include <libdrm/drm_fourcc.h>
#include <stdbool.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MAX_DRM_DEVICES 8

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0
#endif

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((((__u64)0) << 56) | ((1ULL << 56) - 1))
#endif

#ifndef EGL_KHR_platform_gbm
#define EGL_KHR_platform_gbm 1
#define EGL_PLATFORM_GBM_KHR              0x31D7
#endif /* EGL_KHR_platform_gbm */

#ifndef EGL_EXT_platform_base
#define EGL_EXT_platform_base 1
typedef EGLDisplay(EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void* native_display, const EGLint* attrib_list);
typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void* native_window, const EGLint* attrib_list);
typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void* native_pixmap, const EGLint* attrib_list);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLDisplay EGLAPIENTRY eglGetPlatformDisplayEXT(EGLenum platform, void* native_display, const EGLint* attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePlatformWindowSurfaceEXT(EGLDisplay dpy, EGLConfig config, void* native_window, const EGLint* attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePlatformPixmapSurfaceEXT(EGLDisplay dpy, EGLConfig config, void* native_pixmap, const EGLint* attrib_list);
#endif
#endif /* EGL_EXT_platform_base */

#define WEAK __attribute__((weak))

struct drm {
    int fd;
    int crtc_index;
    drmModeModeInfo* mode;
    uint32_t crtc_id;
    uint32_t connector_id;
    unsigned int count;
    bool nonblocking;
    drmModeConnector *connected_connector;
};

struct drm_fb {
    struct gbm_bo* bo;
    uint32_t fb_id;
};

struct gbm {
    struct gbm_device* dev;
    struct gbm_surface* surface;
    uint32_t format;
    int width, height;
};

struct egl {
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
    bool modifiers_supported;
};

int init_egl(struct egl* egl, const struct gbm* gbm, int samples);
int create_program(const char* vs_src, const char* fs_src);
int link_program(unsigned program);

#define NSEC_PER_SEC (INT64_C(1000) * USEC_PER_SEC)
#define USEC_PER_SEC (INT64_C(1000) * MSEC_PER_SEC)
#define MSEC_PER_SEC INT64_C(1000)

int64_t get_time_ns(void);

#endif /* _COMMON_H */
