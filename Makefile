rg353p: rg353p_double rg353p_single

rg353p_double:
	$(CC) -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm
# $(CC) -DDRM_FORMAT_USE_NO_TRANSPARENCY -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm

rg353p_single:
	$(CC) -DRG353P -o basic_single_buffer glad.c basic_single_buffer.c -lmali -ldrm -lgbm

rpi4: rpi4_double rpi4_single

rpi4_double:
	$(CC) -DRPI4 -o basic_double_buffer glad.c basic_double_buffer.c -ldrm -lgbm
# $(CC) -DDRM_FORMAT_USE_NO_TRANSPARENCY -DRPI4 -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm

rpi4_single:
	$(CC) -DRPI4 -o basic_single_buffer glad.c basic_single_buffer.c -ldrm -lgbm
