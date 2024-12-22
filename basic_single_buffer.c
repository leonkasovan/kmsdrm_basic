/*
Basic KMSDRM with single buffer
*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <drm/drm_mode.h>

#define sleep(x); #x

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

// Vertex Data for a Triangle
GLfloat vertices[] = {
    0.0f,  0.5f, 0.0f,  // Top vertex
   -0.5f, -0.5f, 0.0f,  // Bottom-left vertex
    0.5f, -0.5f, 0.0f   // Bottom-right vertex
};

int64_t get_time_ns(void) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_nsec + tv.tv_sec * (1000L * 1000L * 1000L);
}

// Function to Compile Shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        printf("Error compiling shader: %s\n", log);
        exit(EXIT_FAILURE);
    } else {
        printf("compiling shader ok: %d\n", shader);
    }

    return shader;
}

// Function to Create Shader Program
GLuint createProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(program, 512, NULL, log);
        fprintf(stderr, "Error linking program: %s\n", log);
        exit(EXIT_FAILURE);
    }

    return program;
}

int main(int argc, char* argv[]) {
    // Step 1: Open DRM device
    int drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        drm_fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
        if (drm_fd < 0) {
            perror("Failed to open DRM device");
            return EXIT_FAILURE;
        }
    }

    // uint64_t value;
    // int ret = drmGetCap(drm_fd, DRM_CAP_ASYNC_PAGE_FLIP, &value);
    // if (ret == 0 && value) {
    //     printf("[INFO] Asynchronous page flipping is supported.\n");
    // } else {
    //     printf("[INFO] Asynchronous page flipping is not supported.\n");
    // }

    drmModeRes* resources = drmModeGetResources(drm_fd);
    if (!resources) {
        perror("Failed to get DRM resources");
        close(drm_fd);
        return EXIT_FAILURE;
    }

    printf("[INFO] CRTC count: %d, Connector count: %d\n", resources->count_crtcs, resources->count_connectors);
    if (resources->count_crtcs == 0) {
        printf("No CRTC available; likely a legacy system.\n");
    }

    // Find a connected connector
    drmModeConnector* connector = NULL;
    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
        printf("connector[%d]: %d encoders, %d modes, %d props\n", i, connector->count_encoders, connector->count_modes, connector->count_props);
        if (connector->connection == DRM_MODE_CONNECTED) {
            printf("connector[%d] is connected\n", i);
            break;
        }
        drmModeFreeConnector(connector);
        connector = NULL;
    }
    if (!connector) {
        fprintf(stderr, "No connected connector found\n");
        drmModeFreeResources(resources);
        close(drm_fd);
        return EXIT_FAILURE;
    }

    // Get the first mode for the connector
    drmModeModeInfo mode = connector->modes[0];
    uint32_t connector_id = connector->connector_id;

    // Find a CRTC
    drmModeEncoder* encoder = drmModeGetEncoder(drm_fd, connector->encoder_id);
    uint32_t crtc_id = encoder->crtc_id;

    // Step 2: Set up GBM
    struct gbm_device* gbm = gbm_create_device(drm_fd);
    struct gbm_surface* surface = gbm_surface_create(
        gbm, mode.hdisplay, mode.vdisplay, GBM_FORMAT_ABGR8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    // Step 3: Initialize EGL
    EGLDisplay egl_display = eglGetDisplay((EGLNativeDisplayType) gbm);
    eglInitialize(egl_display, NULL, NULL);

    EGLint num_configs;
    eglGetConfigs(egl_display, NULL, 0, &num_configs);
    EGLConfig* configs = malloc(num_configs * sizeof(EGLConfig));
    eglGetConfigs(egl_display, configs, num_configs, &num_configs);
    printf("\n");
    for (int i = 0; i < num_configs; i++) {
        EGLint red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type;

        eglGetConfigAttrib(egl_display, configs[i], EGL_RED_SIZE, &red_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_GREEN_SIZE, &green_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_BLUE_SIZE, &blue_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_ALPHA_SIZE, &alpha_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_DEPTH_SIZE, &depth_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_STENCIL_SIZE, &stencil_size);
        eglGetConfigAttrib(egl_display, configs[i], EGL_SURFACE_TYPE, &surface_type);
        eglGetConfigAttrib(egl_display, configs[i], EGL_RENDERABLE_TYPE, &render_type);

        if (red_size == 8 && green_size == 8 && blue_size == 8 && alpha_size == 8 && depth_size == 24 && stencil_size == 8) {
            printf("Config %d: R:%d G:%d B:%d A:%d Depth:%d Stencil:%d Surface=0x%08X Render=0x%08X\n", i, red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type);
        }
    }
    free(configs);
    printf("Needed EGL_SURFACE_TYPE=0x%08X, EGL_RENDERABLE_TYPE=0x%08X\n", EGL_WINDOW_BIT, EGL_OPENGL_ES3_BIT);


    EGLConfig config;
    EGLint matched = 0;
    EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, // Required for rendering to a GBM surface
#ifdef RG353P
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, // OpenGL ES 3.0
#elif defined(RPI4)
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // OpenGL ES 2.0
#endif
    EGL_RED_SIZE, 8,                 // Red component size
    EGL_GREEN_SIZE, 8,               // Green component size
    EGL_BLUE_SIZE, 8,                // Blue component size
    EGL_ALPHA_SIZE, 8,               // Alpha component size (if needed)
    EGL_DEPTH_SIZE, 24,              // Depth buffer size
    EGL_STENCIL_SIZE, 8,             // Stencil buffer size
    EGL_NONE                         // End of attributes
    };
    if (!eglChooseConfig(egl_display, config_attribs, &config, 1, &matched) || !matched) {
        printf("No EGL configs with appropriate attributes.\n");
        return EXIT_FAILURE;
    }

    EGLint red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type;
    eglGetConfigAttrib(egl_display, config, EGL_RED_SIZE, &red_size);
    eglGetConfigAttrib(egl_display, config, EGL_GREEN_SIZE, &green_size);
    eglGetConfigAttrib(egl_display, config, EGL_BLUE_SIZE, &blue_size);
    eglGetConfigAttrib(egl_display, config, EGL_ALPHA_SIZE, &alpha_size);
    eglGetConfigAttrib(egl_display, config, EGL_DEPTH_SIZE, &depth_size);
    eglGetConfigAttrib(egl_display, config, EGL_STENCIL_SIZE, &stencil_size);
    eglGetConfigAttrib(egl_display, config, EGL_SURFACE_TYPE, &surface_type);
    eglGetConfigAttrib(egl_display, config, EGL_RENDERABLE_TYPE, &render_type);
    printf("Chosen Config R:%d G:%d B:%d A:%d Depth:%d Stencil:%d Surface=0x%08X Render=0x%08X\n", red_size, green_size, blue_size, alpha_size, depth_size, stencil_size, surface_type, render_type);

    static const EGLint context_attribs[] = {
#ifdef RG353P        
        EGL_CONTEXT_CLIENT_VERSION, 3,
#endif        
        EGL_NONE
    };
    EGLContext egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (egl_context == EGL_NO_CONTEXT) {
        printf("Failed to create EGL context\n");
        return -1;
    }

    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, config, (EGLNativeWindowType) surface, NULL);
    if (egl_surface == EGL_NO_SURFACE) {
        printf("Failed to create EGL surface\n");
        return -1;
    }

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

    const char* gl_exts = (char*) glGetString(GL_EXTENSIONS);
    printf("OpenGL ES information:\n");
    printf("  version: \"%s\"\n", glGetString(GL_VERSION));
    printf("  shading language version: \"%s\"\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("  vendor: \"%s\"\n", glGetString(GL_VENDOR));
    printf("  renderer: \"%s\"\n", glGetString(GL_RENDERER));
    printf("  extensions: \"%s\"\n", gl_exts);
    printf("===================================\n");

    // Step 4: Set up OpenGL
    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (vertex_shader == 0) {
        printf("Vertex shader creation failed!:\n");
        return -1;
    }

    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (fragment_shader == 0) {
        printf("Fragment shader creation failed!:\n");
        return -1;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    // GLint ret;
    glGetProgramiv(program, GL_LINK_STATUS, &ret);
    if (!ret) {
        char* log;

        printf("Program linking failed!:\n");
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1) {
            log = malloc(ret);
            glGetProgramInfoLog(program, ret, NULL, log);
            printf("%s", log);
            free(log);
        }

        return -1;
    } else {
        printf("Program linking succeed!\n");
    }

    glUseProgram(program);

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

    // Step 5: Render Loop
    struct gbm_bo* bo = NULL;
    struct gbm_bo* next_bo = NULL;
    uint32_t fb_id = 0, next_fb_id = 0;
    drmEventContext evctx = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler = NULL, // Custom handler will not be needed for a simple loop
    };
    glViewport(0, 0, mode.hdisplay, mode.vdisplay);
    bo = gbm_surface_lock_front_buffer(surface); // Initial buffer
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);
    drmModeAddFB(drm_fd, mode.hdisplay, mode.vdisplay, 24, 32, stride, handle, &fb_id);
    drmModeSetCrtc(drm_fd, crtc_id, fb_id, 0, 0, &connector_id, 1, &mode);

    // Check drmModePageFlip is supported?
    // ret = drmModePageFlip(drm_fd, crtc_id, fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
    // if (ret < 0) {
    //     if (errno == EINVAL) {
    //         printf("[INFO] Page flipping not supported on this system.\n");
    //     } else {
    //         perror("drmModePageFlip failed");
    //     }
    // } else {
    //     printf("[INFO] Page flipping supported on this system.\n");
    // }

    int frame = 0;
    int64_t start_time = get_time_ns();
    while (frame < 300) {
        // Render
        glClearColor(0.0f, 0.5f, 1.0f, 1.0f); // Blue background
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Get the next buffer
        next_bo = gbm_surface_lock_front_buffer(surface);
        uint32_t next_handle = gbm_bo_get_handle(next_bo).u32;
        uint32_t next_stride = gbm_bo_get_stride(next_bo);
        drmModeAddFB(drm_fd, mode.hdisplay, mode.vdisplay, 24, 32, next_stride, next_handle, &next_fb_id);
        // printf("fb_id=%d next_fb_id=%d\n", fb_id, next_fb_id);

        // // Page flip
        // if (drmModePageFlip(drm_fd, crtc_id, next_fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL) < 0) {
        //     fprintf(stderr, "Failed to queue page flip\n");
        //     printf("%d\n", __LINE__);
        //     return -1;
        //     printf("%d\n", __LINE__);
        // }
        // printf("%d\n", __LINE__);
        // // Wait for the page flip to complete
        // struct pollfd fds[] = {
        //     {.fd = drm_fd, .events = POLLIN }
        // };
        // while (poll(fds, 1, -1) <= 0) {
        //     if (errno == EINTR) continue;
        //     perror("Poll error");
        //     break;

        //     // Handle DRM events
        //     drmHandleEvent(drm_fd, &evctx);
        // }
        // printf("%d\n", __LINE__);

        drmModeSetCrtc(drm_fd, crtc_id, next_fb_id, 0, 0, &connector_id, 1, &mode);

        // Release the previous framebuffer
        if (bo) {
            drmModeRmFB(drm_fd, fb_id);
            gbm_surface_release_buffer(surface, bo);
        }

        bo = next_bo;
        fb_id = next_fb_id;

        // Swap buffers
        eglSwapBuffers(egl_display, egl_surface);
        frame++;
    }

    int64_t cur_time = get_time_ns();
    double elapsed_time = cur_time - start_time;
    double secs = elapsed_time / (double) (1000L * 1000L * 1000L);
    unsigned frames = frame - 1;  /* first frame ignored */
    printf("Rendered %u frames in %f sec (%f fps)\n", frames, secs, (double) frames / secs);

    // Cleanup
    // printf("%d\n", __LINE__);
    if (encoder) drmModeFreeEncoder(encoder);
    // printf("%d\n", __LINE__);
    if (connector) drmModeFreeConnector(connector);
    // printf("%d\n", __LINE__);
    if (resources) drmModeFreeResources(resources);
    // printf("%d\n", __LINE__);
    if (surface) gbm_surface_destroy(surface);
    // printf("%d\n", __LINE__);
    if (gbm) gbm_device_destroy(gbm);
    // printf("%d drm_fd=%d\n", __LINE__, drm_fd);
    if (drm_fd > 0) close(drm_fd);
    return 0;
}
