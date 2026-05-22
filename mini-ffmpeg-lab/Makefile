CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2 -g
LDFLAGS  = -lm

SRC_DIR  = .
OBJ_DIR  = build
BIN_DIR  = bin

SOURCES  = $(wildcard $(SRC_DIR)/*.c)
HEADERS  = $(wildcard $(SRC_DIR)/*.h)
OBJECTS  = $(filter-out %example_%,$(filter-out %demo_%,$(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)))

EXAMPLES = example_demux example_decode example_transcode
DEMOS    = demo_player demo_transcoder

.PHONY: all clean examples demos test

all: dirs $(EXAMPLES) $(DEMOS)

dirs:
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/demuxer.o: $(SRC_DIR)/demuxer.c $(SRC_DIR)/demuxer.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/decoder_engine.o: $(SRC_DIR)/decoder_engine.c $(SRC_DIR)/decoder_engine.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/encoder_engine.o: $(SRC_DIR)/encoder_engine.c $(SRC_DIR)/encoder_engine.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/filter_graph.o: $(SRC_DIR)/filter_graph.c $(SRC_DIR)/filter_graph.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/av_sync.o: $(SRC_DIR)/av_sync.c $(SRC_DIR)/av_sync.h
	$(CC) $(CFLAGS) -c $< -o $@

LIB_OBJS = $(OBJ_DIR)/demuxer.o $(OBJ_DIR)/decoder_engine.o \
           $(OBJ_DIR)/encoder_engine.o $(OBJ_DIR)/filter_graph.o \
           $(OBJ_DIR)/av_sync.o

examples: $(EXAMPLES)

demos: $(DEMOS)

example_demux: $(LIB_OBJS) $(OBJ_DIR)/example_demux.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS)

example_decode: $(LIB_OBJS) $(OBJ_DIR)/example_decode.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS)

example_transcode: $(LIB_OBJS) $(OBJ_DIR)/example_transcode.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS)

demo_player: $(LIB_OBJS) $(OBJ_DIR)/demo_player.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS)

demo_transcoder: $(LIB_OBJS) $(OBJ_DIR)/demo_transcoder.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS)

test: $(EXAMPLES) $(DEMOS)
	@echo "=== Running Tests ==="
	$(BIN_DIR)\example_demux
	$(BIN_DIR)\example_decode
	$(BIN_DIR)\example_transcode
	$(BIN_DIR)\demo_player
	$(BIN_DIR)\demo_transcoder
	@echo "=== All Tests Passed ==="

clean:
	@if exist "$(OBJ_DIR)" rmdir /s /q "$(OBJ_DIR)"
	@if exist "$(BIN_DIR)" rmdir /s /q "$(BIN_DIR)"
