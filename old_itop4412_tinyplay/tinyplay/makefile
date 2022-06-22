
GCC := /home/zcq/Arm_tool_x86_64_linux/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc -static 

all : tinyplay tinypcminfo tinycap tinymix 

tinyplay:tinyplay.o pcm.o
	$(GCC) -o tinyplay tinyplay.o pcm.o

tinypcminfo:tinypcminfo.o pcm.o
	$(GCC) -o tinypcminfo tinypcminfo.o pcm.o 

tinycap:tinycap.o pcm.o
	$(GCC) -o tinycap tinycap.o pcm.o

tinymix:tinymix.o mixer.o
	$(GCC) -o tinymix tinymix.o mixer.o

tinyplay.o:tinyplay.c
	$(GCC) -c tinyplay.c

tinypcminfo.o:tinypcminfo.c
	$(GCC) -c tinypcminfo.c

tinycap.o:tinycap.c
	$(GCC) -c tinycap.c

tinymix.o:tinymix.c
	$(GCC) -c tinymix.c

pcm.o:pcm.c
	$(GCC) -c pcm.c

mixer.o:mixer.c
	$(GCC) -c mixer.c

.PHONY : clean

clean:
	rm -f mixer.o pcm.o tinymix.o tinycap.o tinypcminfo.o tinyplay.o
