OBJECTS    = main.rel
LDFLAGS    = -mstm8 -lstm8 --code-loc 0x8000 --data-loc 0
#LDFLAGS    = -mstm8 -lstm8 --code-loc 0x0000 --data-loc 933
CFLAGS     = -mstm8 -I. -DSTM8S003 -DF_CPU=16000000 --std-c99 

all: shakeit.bin

clean:
	rm -rf $(OBJECTS) shakeit.ihx shakeit.bin

shakeit.ihx: $(OBJECTS)
	sdcc $(OBJECTS) -o $@ $(LDFLAGS)

%.bin: %.ihx
	objcopy -I ihex -O binary $< $@

%.rel: %.c
	sdcc $(INCLUDES) $(CFLAGS) -c -o $@ $<
