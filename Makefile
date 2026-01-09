# COSH - C-Operating System Shell
# Professional Makefile with dynamic apps detection

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=gnu99 -DLOG_USE_COLOR
LDFLAGS := -lncurses -lvterm

# Installation Paths
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
DESTDIR ?=

# Directories
APP_DIR := apps
OBJ_DIR := obj

# Files
TARGET  := cosh

# Core source files
CORE_SRCS := $(wildcard ./*.c)

# Dynamic apps discovery (Wildcard apps/*.c)
APP_SRCS  := $(wildcard $(APP_DIR)/*.c)

# Combine all sources
SRCS      := $(CORE_SRCS) $(APP_SRCS)

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
