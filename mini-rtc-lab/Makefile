CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
LDFLAGS =

SRCS = sdp_signaling.c ice_stun_turn.c dtls_srtp.c media_track.c sfu_mcu.c
OBJS = $(SRCS:.c=.o)

EXAMPLES = example_webrtc_call example_sfu_call example_ice_turn
DEMOS = demo_webrtc_endpoint demo_sfu_server
TARGETS = $(EXAMPLES) $(DEMOS)

.PHONY: all clean examples demos help

all: $(TARGETS)

examples: $(EXAMPLES)

demos: $(DEMOS)

example_webrtc_call: example_webrtc_call.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example_sfu_call: example_sfu_call.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example_ice_turn: example_ice_turn.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_webrtc_endpoint: demo_webrtc_endpoint.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_sfu_server: demo_sfu_server.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

sdp_signaling.o: sdp_signaling.c sdp_signaling.h
	$(CC) $(CFLAGS) -c -o $@ $<

ice_stun_turn.o: ice_stun_turn.c ice_stun_turn.h sdp_signaling.h
	$(CC) $(CFLAGS) -c -o $@ $<

dtls_srtp.o: dtls_srtp.c dtls_srtp.h sdp_signaling.h
	$(CC) $(CFLAGS) -c -o $@ $<

media_track.o: media_track.c media_track.h
	$(CC) $(CFLAGS) -c -o $@ $<

sfu_mcu.o: sfu_mcu.c sfu_mcu.h media_track.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	del /q *.o $(TARGETS).exe 2>nul || rm -f *.o $(TARGETS) 2>/dev/null

help:
	@echo "make all      - Build all examples and demos"
	@echo "make examples - Build examples only"
	@echo "make demos    - Build demos only"
	@echo "make clean    - Remove built files"
	@echo "make help     - Show this help"
