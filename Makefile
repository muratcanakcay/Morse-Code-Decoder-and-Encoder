CC=$(CROSS_COMPILE)gcc
OBJS := main.o
main: $(OBJS)
	$(CC) -o main $(CFLAGS) $(LDFLAGS) $(OBJS) -l gpiod
$(OBJS) : %.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

