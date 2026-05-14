# Makefile for C-Shell
# Produces shell.out in the C-Shell/ directory via `make all`.

CC      := gcc
CFLAGS  := -std=c99               \
            -D_POSIX_C_SOURCE=200809L \
            -D_XOPEN_SOURCE=700   \
            -Wall -Wextra -Werror \
            -Wno-unused-parameter \
            -fno-asm              \
            -g                    \
            -I include            \
            -MMD -MP

SRCDIR  := src
INCDIR  := include
BLDDIR  := build
TARGET  := shell.out

# Automatically discover all .c files under src/
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

# Link all object files into the final binary
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

# Compile each source file into a build artefact
$(BLDDIR)/%.o: $(SRCDIR)/%.c | $(BLDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create the build directory if it doesn't exist
$(BLDDIR):
	@mkdir -p $(BLDDIR)

# Remove compiled artefacts (keeps sources intact)
clean:
	rm -rf $(BLDDIR) $(TARGET)

# Include auto-generated dependency files
-include $(DEPS)
