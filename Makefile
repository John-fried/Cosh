# COSH - C-Operating System Shell
# Professional Makefile with dynamic paths

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=gnu99
LDFLAGS := -lncurses

# Installation Paths (Dynamic)
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
DESTDIR ?=

# Directories
APP_DIR := apps
OBJ_DIR := obj

# Files
TARGET  := cosh
SRCS    := cosh.c wmcurses.c $(APP_DIR)/iterm.c
OBJS    := $(SRCS:%.c=$(OBJ_DIR)/%.o)

# Default rule
all: $(TARGET)

# Linking the final binary
$(TARGET): $(OBJS)
	@echo "  LD      $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compiling source files to objects
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	@echo "  CLEAN"
	@rm -rf $(OBJ_DIR) $(TARGET)

# Installation with directory creation
install: $(TARGET)
	@echo "  INSTALL $(DESTDIR)$(BINDIR)/$(TARGET)"
	@mkdir -p $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

# Uninstallation
uninstall:
	@echo "  UNINSTALL $(DESTDIR)$(BINDIR)/$(TARGET)"
	@rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
