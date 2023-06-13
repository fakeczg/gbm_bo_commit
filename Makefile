all:
	gcc -g -o gbm main.c  -O2 -ldrm -I/usr/include/libdrm
clean:
	rm egl_gbm

