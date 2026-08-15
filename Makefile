_CFLAGS := -O2 -std=gnu23 -pipe \
           -Wall -Wextra -Wformat    \
           -Werror=implicit-function-declaration -Werror=int-conversion
CFLAGS_DBG := -g -O0 -std=gnu23 -pipe \
           -Wall -Wextra -Wformat    \
           -Werror=implicit-function-declaration -Werror=int-conversion

override CFLAGS := $(_CFLAGS) $(CFLAGS)

LDFLAGS = -Llibs -L/usr/local/lib -ljson-c -lqlibc

all:
	$(CC) $(CFLAGS) -o spotify-playing src/*.c $(LDFLAGS)

alldbg:
	$(CC) $(CFLAGS_DBG) -o spotify-playing src/*.c $(LDFLAGS)
