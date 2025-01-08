steamdeck: steamdeck_basic_opengles steamdeck_basic_opengl

steamdeck_basic_opengles: basic_opengles.c basic_opengles.h
	$(CC) -DDEBUG -I/usr/include/libdrm -g -o steamdeck_basic_opengles gles/glad.c basic_opengles.c -lEGL -lgbm -ldrm

steamdeck_basic_opengl: basic_opengl.c basic_opengl.h
	$(CC) -DDEBUG -I/usr/include/libdrm-g -o steamdeck_basic_opengl gl/glad.c basic_opengl.c -ldrm -lgbm -lEGL
#	$(CC) -DDEBUG -DGL_GLEXT_PROTOTYPES -I/usr/include/libdrm-g -o steamdeck_basic_opengl gl/glad.c basic_opengl.c -ldrm -lgbm -lEGL

rpi4: /usr/include/drm.h /usr/include/drm_mode.h rpi4_basic_opengles rpi4_basic_opengles rpi4_basic_opengl

/usr/include/drm.h:
	sudo ln -s /usr/include/libdrm/drm.h /usr/include/drm.h

/usr/include/drm_mode.h:
	sudo ln -s /usr/include/libdrm/drm_mode.h /usr/include/drm_mode.h

rpi4_basic_opengles: basic_opengles.c basic_opengles.h
	$(CC) -DDEBUG -DRPI4 -o rpi4_basic_opengles gles/glad.c basic_opengles.c -ldrm -lgbm -lEGL

rpi4_basic_opengl: basic_opengl.c basic_opengl.h
	$(CC) -DDEBUG -DRPI4 -DGL_GLEXT_PROTOTYPES -o rpi4_basic_opengl gl/glad.c basic_opengl.c -ldrm -lgbm -lEGL

rg353p: basic_opengles.c basic_opengles.h
	$(CC) -DRG353P -o rg353p_basic_opengles gles/glad.c basic_opengles.c -lmali -ldrm -lgbm
