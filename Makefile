# COSH - C-Operating System Shell

CC      := gcc
CFLAGS  := -Wall -Wextra -pedantic -O3 -march=native -std=gnu99 -ILibs -IInclude -DLOG_USE_COLOR
CFLAGS += -Wshadow -Wpointer-arith -Wstrict-prototypes
LDFLAGS := -lpanelw -lncursesw -lutil -lvterm 
# Installation Paths
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
DESTDIR ?=

# Directories
APP_DIR := Apps
UTIL_DIR := Utils
KERNEL_DIR := Kernel
LIBS_DIR := Libs
BUILD_DIR := Build
OBJ_DIR := $(BUILD_DIR)/obj

# Files
TARGETNAME  := cosh
TARGETPATH := $(BUILD_DIR)/$(TARGETNAME)

# Core source files
CORE_SRCS := $(wildcard ./*.c)
APP_SRCS  := $(wildcard $(APP_DIR)/*.c)
UTIL_SRCS := $(wildcard $(UTIL_DIR)/*.c)
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c)
LIBS_SRCS   := $(shell find $(LIBS_DIR) -name "*.c")

# Combine all sources
SRCS      := $(LIBS_SRCS) $(KERNEL_SRCS) $(CORE_SRCS) $(UTIL_SRCS) $(APP_SRCS)

# Generate object paths in object directory
OBJS      := $(SRCS:%.c=$(OBJ_DIR)/%.o)

# Default rule
all: $(TARGETPATH)

# Linking
$(TARGETPATH): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "  LD      $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compiling
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR)

# Install
install: $(TARGETPATH)
	@echo "  INSTALL $(DESTDIR)$(BINDIR)/$(TARGETNAME)"
	@mkdir -p $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGETPATH) $(DESTDIR)$(BINDIR)/$(TARGETNAME)

# Uninstall
uninstall:
	@echo "  UNINSTALL $(DESTDIR)$(BINDIR)/$(TARGETNAME)"
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGETNAME)

size: $(TARGETPATH)
	@echo "  SIZE..."
	@size $(TARGETPATH)

.PHONY: all clean install uninstall
