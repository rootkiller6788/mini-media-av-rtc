CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
LDFLAGS = -lm

SRCS = face_landmark.c body_pose.c expression_bs.c lip_sync.c avatar_drive.c
HDRS = face_landmark.h body_pose.h expression_bs.h lip_sync.h avatar_drive.h
OBJS = $(SRCS:.c=.o)

EXAMPLES = example_face_detect example_lip_sync example_avatar_drive
DEMOS = demo_face_landmark demo_avatar_full
TARGETS = $(DEMOS) $(EXAMPLES)

.PHONY: all clean examples demos

all: $(TARGETS)

demos: $(DEMOS)

examples: $(EXAMPLES)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

demo_face_landmark: demo_face_landmark.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

demo_avatar_full: demo_avatar_full.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

example_face_detect: example_face_detect.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

example_lip_sync: example_lip_sync.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

example_avatar_drive: example_avatar_drive.c $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	del /f /q *.o *.exe 2>nul || rm -f *.o $(TARGETS)

run-demo-face: demo_face_landmark
	./demo_face_landmark

run-demo-avatar: demo_avatar_full
	./demo_avatar_full

run-face: example_face_detect
	./example_face_detect

run-lip: example_lip_sync
	./example_lip_sync

run-avatar: example_avatar_drive
	./example_avatar_drive
