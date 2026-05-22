CC       := gcc
CFLAGS   := -Wall -Wextra -O2 -I include
LDFLAGS  := -lm
OUTDIR   := bin
AR       := ar
ARFLAGS  := rcs

SRCDIR   := src
INCDIR   := include
EXAMPLEDIR := examples

SRCS     := $(wildcard $(SRCDIR)/*.c)
OBJS     := $(patsubst $(SRCDIR)/%.c,$(OUTDIR)/%.o,$(SRCS))
LIBNAME  := libmini_image.a
TARGET   := $(OUTDIR)/$(LIBNAME)

EXAMPLES := $(basename $(notdir $(wildcard $(EXAMPLEDIR)/*.c)))
EXE      := $(addprefix $(OUTDIR)/,$(EXAMPLES))

.PHONY: all lib examples run_examples clean

all: lib examples

lib: $(TARGET)

examples: $(EXE)

$(OUTDIR):
	mkdir -p "$(OUTDIR)"

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) | $(OUTDIR)
	$(AR) $(ARFLAGS) $@ $^

$(OUTDIR)/01_basic_filter: $(OUTDIR)/pixel_format.o $(OUTDIR)/image_filter.o
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/01_basic_filter.c $^ $(LDFLAGS)

$(OUTDIR)/02_jpeg_codec: $(OUTDIR)/pixel_format.o $(OUTDIR)/transforms.o $(OUTDIR)/jpeg_codec.o
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/02_jpeg_codec.c $^ $(LDFLAGS)

$(OUTDIR)/03_image_io: $(OUTDIR)/pixel_format.o $(OUTDIR)/image_filter.o $(OUTDIR)/image_io.o
	$(CC) $(CFLAGS) -o $@ $(EXAMPLEDIR)/03_image_io.c $^ $(LDFLAGS)

run_examples: examples
	$(foreach exe,$(EXE),$(exe)${\n})

clean:
	rm -rf "$(OUTDIR)"
