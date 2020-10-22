# Network library

export

LIB_OUT = $(LIB_NETWORK)

SRCS := $(shell find . -name '*.[cs]')

LIB_OBJS = $(sort $(patsubst %.c,%.o,$(SRCS))) 

.PHONY: all
.PHONY: clean

all: $(LIB_OUT)

$(LIB_OUT): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

clean:
	-rm -f $(LIB_OBJS) $(LIB_OUT)
