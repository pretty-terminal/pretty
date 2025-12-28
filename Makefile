CC ?= gcc

BUILD := .build
OUT := pretty
DEBUG_OUT := pretty.debug

SRC := $(shell find src -type f -name "*.c")
OBJS := $(SRC:%.c=$(BUILD)/%.o)

LIBS := sdl3 sdl3-ttf fontconfig
$(info $(LIBS))

DEBUG ?= 0

CFLAGS += $(shell cat warning_flags.txt)
CFLAGS += -iquote src
CFLAGS += $(shell pkg-config --cflags $(LIBS))

ifeq ($(DEBUG),1)
	OUT := $(DEBUG_OUT)
	CFLAGS += -g -O0 -fno-omit-frame-pointer
else
	CFLAGS += -O2
endif

LDLIBS += $(shell pkg-config --libs $(LIBS))

ifneq ($(RELEASE),)
CFLAGS += -DWAIT_EVENTS=1
endif

.PHONY: all
all: $(OUT)

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(COMPILE.c) -o $@ $<

$(OUT): $(OBJS)
	$(LINK.c) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(OBJS)

.PHONY: fclean
fclean: clean
	$(RM) $(OUT)

.PHONY: re
.NOTPARALLEL: re
re: fclean all

PREFIX ?= /usr
BINDIR := $(PREFIX)/bin

.PHONY: install
install:
	mkdir -p $(BINDIR)
	install -Dm577 $(OUT) -t $(BINDIR)
