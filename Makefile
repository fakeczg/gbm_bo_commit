all:
	gcc -g -o gbm main.c  -O2 -ldrm -lgbm -lpthread -I/usr/include -I/usr/include/libdrm
clean:
	rm egl_gbm

