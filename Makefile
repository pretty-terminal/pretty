CC ?= gcc

BUILD := .build
OUT := pretty

SRC := $(shell find src -type f -name "*.c")
OBJS := $(SRC:%.c=$(BUILD)/%.o)

LIBS := sdl3 sdl3-ttf fontconfig
$(info $(LIBS))

CFLAGS += $(shell cat warning_flags.txt)
CFLAGS += -O2
CFLAGS += -iquote src
CFLAGS += $(shell pkg-config --cflags $(LIBS))

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
