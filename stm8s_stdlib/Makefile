C_FILES    = $(shell find . -name "*.c")
OBJECTS    = $(patsubst %.c, %.rel, $(C_FILES))
CFLAGS     = -mstm8 -I . -DSTM8S003 -DF_CPU=16000000 --std-c99
#-Wa,-l
#--debug 

%.rel: %.c
	sdcc $(INCLUDES) $(CFLAGS) -c -o $@ $<

all: stm8s_stdlib.lib

clean:
	rm -rf $(OBJECTS) stm8s_stdlib.lib

stm8s_stdlib.lib: $(OBJECTS)
	sdcclib a $@ $(OBJECTS)

