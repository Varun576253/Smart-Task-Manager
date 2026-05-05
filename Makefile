# ──────────────────────────────────────────────────────────────
#  Makefile – Smart Task Manager & Job Scheduler
#  Targets:  all  server  client  clean  run-server  run-client
# ──────────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11 -D_GNU_SOURCE
LDFLAGS = -lpthread

# Source files
SERVER_SRCS = server.c scheduler.c storage.c
CLIENT_SRCS = client.c

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

SERVER_BIN  = server
CLIENT_BIN  = client

# ── Default target ──────────────────────────────────────────
.PHONY: all
all: $(SERVER_BIN) $(CLIENT_BIN)
	@echo ""
	@echo "  Build complete."
	@echo "  Run server:  ./server"
	@echo "  Run client:  ./client [host] [port]"
	@echo ""

# ── Server binary ───────────────────────────────────────────
$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  Linked: $@"

# ── Client binary ───────────────────────────────────────────
$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "  Linked: $@"

# ── Generic .c → .o rule ────────────────────────────────────
%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "  Compiled: $<"

# ── Convenience targets ─────────────────────────────────────
.PHONY: run-server
run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

.PHONY: run-client
run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

# ── Clean ───────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_BIN) $(CLIENT_BIN)
	@echo "  Cleaned."

# ── Reset data files (does NOT touch users.txt) ─────────────
.PHONY: reset-data
reset-data:
	rm -f jobs.txt logs.txt
	@echo "  Data files reset (users.txt preserved)."

# ── Show phony targets ──────────────────────────────────────
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all          – build server and client (default)"
	@echo "  server       – build server only"
	@echo "  client       – build client only"
	@echo "  run-server   – build + start server"
	@echo "  run-client   – build + start client"
	@echo "  clean        – remove binaries and object files"
	@echo "  reset-data   – delete jobs.txt and logs.txt"
	@echo "  help         – show this message"
