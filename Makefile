# Auto-organizing Makefile for shell project

CC := gcc
SRCDIR := src
INCDIR := include
BUILDDIR := build
TARGET := shell.out

# Ensure directory structure exists and migrate flat sources/headers once
INIT := $(shell sh -c 'mkdir -p $(SRCDIR) $(INCDIR) $(BUILDDIR); for f in *.c; do [ -f "$$f" ] && mv "$$f" $(SRCDIR)/; done; for f in *.h; do [ -f "$$f" ] && mv "$$f" $(INCDIR)/; done')

CFLAGS := -std=c99 \
	-D_POSIX_C_SOURCE=200809L \
	-D_XOPEN_SOURCE=700 \
	-Wall -Wextra -Werror \
	-Wno-unused-parameter \
	-fno-asm \
	-g -I$(INCDIR) -MMD -MP
LDFLAGS :=

# Discover sources after prepare step
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean distclean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
