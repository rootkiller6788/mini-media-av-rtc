CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lm
BUILD_DIR = build

SRCS = src/yuv_rgb_conv.c src/h264_nal.c src/macroblock.c src/motion_est.c src/rate_control.c
OBJS = $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

EXAMPLES = $(BUILD_DIR)/01_yuv_convert $(BUILD_DIR)/02_nal_parse $(BUILD_DIR)/03_encode_frame
DEMOS = $(BUILD_DIR)/yuv_to_rgb $(BUILD_DIR)/h264_encoder

.PHONY: all clean

all: $(BUILD_DIR) $(EXAMPLES) $(DEMOS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/01_yuv_convert: examples/01_yuv_convert.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/02_nal_parse: examples/02_nal_parse.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/03_encode_frame: examples/03_encode_frame.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/yuv_to_rgb: demo/yuv_to_rgb.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/h264_encoder: demo/h264_encoder.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
