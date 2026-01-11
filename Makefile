# COSH - C-Operating System Shell

CC      := gcc
CFLAGS  := -Wall -Wextra -pedantic -O3 -march=native -std=gnu99 -DLOG_USE_COLOR
LDFLAGS := -lpanelw -lncursesw -lutil -lvterm

# Installation Paths
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
DESTDIR ?=

# Directories
APP_DIR := apps
UTIL_DIR := utils
KERNEL_DIR := kernel
OBJ_DIR := .obj

# Files
TARGET  := cosh

# Core source files
CORE_SRCS := $(wildcard ./*.c)
APP_SRCS  := $(wildcard $(APP_DIR)/*.c)
UTIL_SRCS := $(wildcard $(UTIL_DIR)/*.c)
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c)

# Combine all sources
SRCS      := $(KERNEL_SRCS) $(CORE_SRCS) $(UTIL_SRCS) $(APP_SRCS)

# Generate object paths in obj/ directory
OBJS      := $(SRCS:%.c=$(OBJ_DIR)/%.o)

# Default rule
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	@echo "  LD      $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compiling (Handles nested directory structure in obj/)
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	@echo "  CLEAN"
	@rm -rf $(OBJ_DIR) $(TARGET)

# Install
install: $(TARGET)
	@echo "  INSTALL $(DESTDIR)$(BINDIR)/$(TARGET)"
	@mkdir -p $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

# Uninstall
uninstall:
	@echo "  UNINSTALL $(DESTDIR)$(BINDIR)/$(TARGET)"
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
