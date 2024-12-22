rg353p: double single

double:
	$(CC) -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm
# $(CC) -DDRM_FORMAT_USE_NO_TRANSPARENCY -DRG353P -o basic_double_buffer glad.c basic_double_buffer.c -lmali -ldrm -lgbm

single:
	$(CC) -DRG353P -o basic_single_buffer glad.c basic_single_buffer.c -lmali -ldrm -lgbm
