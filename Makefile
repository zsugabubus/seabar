CFLAGS += -std=c11 -g -O0
BLOCK_CFLAGS := $(shell sed -n '/block_[a-z_]*/{s|.*block_\([a-z_]*\).*|blocks/\1.h|p}' <config.h | sort | uniq | xargs -I{} sed -n '/^\/\* *CFLAGS *+=/{s|.*+= *\(.*\) *\*/$$|\1|p}' {})
CFLAGS += $(BLOCK_CFLAGS)
LDFLAGS +=

TARGET := seabar

$(TARGET) : $(TARGET).c blocks/* config.h config.blocks.h Makefile
	$(CC) $(CFLAGS) -o $@ $< -I. fourmat/fourmat.c

config.h :
	cp config.def.h $@

config.blocks.h : config.h Makefile
	sed -n '/block_[a-z]*/{s|.*block_\([a-z]*\).*|#include "blocks/\1.h"|p}' <$< | sort | uniq >$@

run : $(TARGET)
	./$< 2>/dev/null

debug : $(TARGET)
	gdb $< -ex run

.PHONY : debug run
