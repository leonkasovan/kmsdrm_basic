rg353p: rg353p_double rg353p_single

rg353p_double:
	$(CC) -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm
# $(CC) -DDRM_FORMAT_USE_NO_TRANSPARENCY -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm

rg353p_single:
	$(CC) -DRG353P -o basic_single_buffer glad.c basic_single_buffer.c -lmali -ldrm -lgbm

rpi4: /usr/include/drm.h /usr/include/drm_mode.h rpi4_double rpi4_single

/usr/include/drm.h:
	sudo ln -s /usr/include/libdrm/drm.h /usr/include/drm.h

/usr/include/drm_mode.h:
	sudo ln -s /usr/include/libdrm/drm_mode.h /usr/include/drm_mode.h

rpi4_double:
	$(CC) -DRPI4 -o basic_double_buffer glad.c basic_double_buffer.c -ldrm -lgbm -lGLESv2 -lEGL

rpi4_single:
	$(CC) -DRPI4 -o basic_single_buffer glad.c basic_single_buffer.c -ldrm -lgbm -lGLESv2 -lEGL
