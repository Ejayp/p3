# Target library
lib := libuthread.a
objs := sem.o tps.o
staticObjs := thread.o queue.o

CC := gcc
CFLAGS := -Wall -Werror
CFLAGS += -g
PANDOC := pandoc

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs),$(staticObjs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libuthread.a: $(objs) $(staticObjs)
	ar rcs libuthread.a $(objs) $(staticObjs)

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	$(Q)rm -f $(targets) $(objs) $(deps)
