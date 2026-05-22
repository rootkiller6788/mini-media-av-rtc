CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -O2
INCDIR  = include
SRCDIR  = src
EXADIR  = examples
DEMODIR = demos
BUILDDIR = build
OUTDIR  = output

SRCS = $(SRCDIR)/hls_segmenter.c \
       $(SRCDIR)/dash_segmenter.c \
       $(SRCDIR)/abr_engine.c    \
       $(SRCDIR)/cdn_edge.c      \
       $(SRCDIR)/drm_system.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

EXAMPLES = $(BUILDDIR)/example_hls  \
           $(BUILDDIR)/example_dash \
           $(BUILDDIR)/example_abr

DEMOS = $(BUILDDIR)/demo_media_stream \
        $(BUILDDIR)/demo_drm_stream

.PHONY: all examples demos clean dirs

all: dirs $(OBJS)
	@echo "  Object files built in $(BUILDDIR)/"

dirs:
	@if not exist "$(BUILDDIR)" mkdir "$(BUILDDIR)"
	@if not exist "$(OUTDIR)"  mkdir "$(OUTDIR)"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BUILDDIR)/example_hls: $(EXADIR)/example_hls.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(OBJS) -o $@

$(BUILDDIR)/example_dash: $(EXADIR)/example_dash.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(OBJS) -o $@

$(BUILDDIR)/example_abr: $(EXADIR)/example_abr.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(OBJS) -o $@

$(BUILDDIR)/demo_media_stream: $(DEMODIR)/demo_media_stream.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(OBJS) -o $@

$(BUILDDIR)/demo_drm_stream: $(DEMODIR)/demo_drm_stream.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(OBJS) -o $@

examples: dirs $(OBJS) $(EXAMPLES)
	@echo "  Examples built in $(BUILDDIR)/"

demos: dirs $(OBJS) $(DEMOS)
	@echo "  Demos built in $(BUILDDIR)/"

clean:
	@if exist "$(BUILDDIR)" rmdir /s /q "$(BUILDDIR)"
	@if exist "$(OUTDIR)"  rmdir /s /q "$(OUTDIR)"
