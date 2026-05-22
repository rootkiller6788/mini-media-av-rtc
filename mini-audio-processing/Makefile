CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2
LDFLAGS  = -lm
TARGET   =

SRCS     = pcm_wav.c fft_core.c audio_filter.c audio_codec.c resample_mix.c
OBJS     = $(SRCS:.c=.o)

EXAMPLES = example_wav_play example_spectrum example_filter
DEMOS    = demo_noise_reduction demo_audio_mixer
OUTS     = $(EXAMPLES) $(DEMOS)

.PHONY: all examples demos clean

all: $(OBJS) $(OUTS)

examples: $(EXAMPLES)
demos: $(DEMOS)

$(OBJS): %.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

example_wav_play: example_wav_play.c pcm_wav.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

example_spectrum: example_spectrum.c fft_core.o pcm_wav.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

example_filter: example_filter.c audio_filter.o pcm_wav.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

demo_noise_reduction: demo_noise_reduction.c pcm_wav.o fft_core.o audio_filter.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

demo_audio_mixer: demo_audio_mixer.c pcm_wav.o resample_mix.o audio_filter.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	del /q /f *.o 2>nul
	del /q /f $(OUTS:=.exe) 2>nul
	del /q /f output_sine.wav output_filtered.wav demo_noisy.wav demo_denoised.wav demo_mixdown.wav 2>nul
