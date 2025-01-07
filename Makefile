steamdeck: steamdeck_basic_opengles_2buffer steamdeck_basic_opengles_1buffer steamdeck_basic_opengl_2buffer

steamdeck_basic_opengles_2buffer: basic_opengles_2buffer.c basic_opengles_2buffer.h
	$(CC) -DDEBUG -I/usr/include/libdrm -g -o steamdeck_basic_opengles_2buffer gles/glad.c basic_opengles_2buffer.c -ldrm -lgbm -lGLESv2 -lEGL
#	$(CC) -DDEBUG -o steamdeck_basic_opengles_2buffer gles/glad.c basic_opengles_2buffer.c -ldrm -lgbm -lGLESv2 -lEGL

steamdeck_basic_opengles_1buffer: basic_opengles_1buffer.c
	$(CC) -I/usr/include/libdrm -g -o steamdeck_basic_opengles_1buffer gles/glad.c basic_opengles_1buffer.c -ldrm -lgbm -lGLESv2 -lEGL

steamdeck_basic_opengl_2buffer: basic_opengl_2buffer.c basic_opengl_2buffer.h
	$(CC) -I/usr/include/libdrm -DGL_GLEXT_PROTOTYPES -g -o steamdeck_basic_opengl_2buffer gl/glad.c basic_opengl_2buffer.c -ldrm -lgbm -lGL -lEGL
#	$(CC) -I/usr/include/libdrm -DDEBUG -DGL_GLEXT_PROTOTYPES -o steamdeck_basic_opengl_2buffer gl/glad.c basic_opengl_2buffer.c -ldrm -lgbm -lGL -lEGL

rpi4: /usr/include/drm.h /usr/include/drm_mode.h rpi4_basic_opengles_2buffer rpi4_basic_opengles_1buffer rpi4_basic_opengl_2buffer

/usr/include/drm.h:
	sudo ln -s /usr/include/libdrm/drm.h /usr/include/drm.h

/usr/include/drm_mode.h:
	sudo ln -s /usr/include/libdrm/drm_mode.h /usr/include/drm_mode.h

rpi4_basic_opengles_2buffer: basic_opengles_2buffer.c basic_opengles_2buffer.h
	$(CC) -DRPI4 -o rpi4_basic_opengles_2buffer gles/glad.c basic_opengles_2buffer.c -ldrm -lgbm -lGLESv2 -lEGL
#	$(CC) -DDEBUG -DRPI4 -o rpi4_basic_opengles_2buffer gles/glad.c basic_opengles_2buffer.c -ldrm -lgbm -lGLESv2 -lEGL

rpi4_basic_opengles_1buffer: basic_opengles_1buffer.c
	$(CC) -DRPI4 -o rpi4_basic_opengles_1buffer gles/glad.c basic_opengles_1buffer.c -ldrm -lgbm -lGLESv2 -lEGL

rpi4_basic_opengl_2buffer: basic_opengl_2buffer.c basic_opengl_2buffer.h
	$(CC) -DRPI4 -DGL_GLEXT_PROTOTYPES -o rpi4_basic_opengl_2buffer gl/glad.c basic_opengl_2buffer.c -ldrm -lgbm -lGL -lEGL
#	$(CC) -DDEBUG -DRPI4 -DGL_GLEXT_PROTOTYPES -o rpi4_basic_opengl_2buffer gl/glad.c basic_opengl_2buffer.c -ldrm -lgbm -lGL -lEGL

rg353p: rg353p_basic_opengles_2buffer rg353p_basic_opengles_1buffer

rg353p_basic_opengles_2buffer: basic_opengles_2buffer.c basic_opengles_2buffer.h
	$(CC) -DRG353P -o rg353p_basic_opengles_2buffer gles/glad.c basic_opengles_2buffer.c -lmali -ldrm -lgbm
# $(CC) -DDRM_FORMAT_USE_NO_TRANSPARENCY -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm

rg353p_basic_opengles_1buffer: basic_opengles_1buffer.c
	$(CC) -DRG353P -o rg353p_basic_opengles_1buffer gles/glad.c basic_opengles_1buffer.c -lmali -ldrm -lgbm
