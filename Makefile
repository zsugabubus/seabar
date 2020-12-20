CFLAGS += -std=c11 -g -O0
BLOCK_CFLAGS := $(shell sed -n '/block_[a-z_]*/{s|.*block_\([a-z_]*\).*|blocks/\1.h|p}' <config.h | sort | uniq | xargs -I{} sed -n '/^\/\* *CFLAGS *+=/{s|.*+= *\(.*\) *\*/$$|\1|p}' {})
CFLAGS += $(BLOCK_CFLAGS)
LDFLAGS +=

RM ?= rm -f

TARGET := seabar

fourmat/% :
	git submodule update --init fourmat

$(TARGET) : $(TARGET).c fourmat/fourmat.c blocks/* utils.h config.h config.blocks.h Makefile
	$(CC) $(CFLAGS) -o $@ -I. $(filter %.c,$^)

config.h :
	cp config.def.h $@

config.blocks.h : config.h Makefile
	sed -n '/block_[a-z]*/{s|.*block_\([a-z]*\).*|#include "blocks/\1.h"|p}' <$< | sort | uniq >$@

run : $(TARGET)
	./$< 2>/dev/null

debug : $(TARGET)
	gdb $< -ex run

clean :
	$(RM) $(TARGET) config.blocks.h

.PHONY : debug run clean
