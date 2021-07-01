#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"

#define TIMEOUT_MS 1500

void cell_input(struct cell *cell, unsigned char c) {
  if (cell->len + 2 > cell->size) cell->len = 0;  // Silent flush
  cell->buf[cell->len++] = c;                     // Append to recv buffer
  cell->buf[cell->len] = '\0';                    // For printf()
  // cell->dbg("IN: %s\n", cell->buf);
}

static int cmdlen(struct cell *c) {
  int n = c->cmd;
  while (c->cmds && c->cmds[n] != '\0' && c->cmds[n] != ';') n++;
  return n - c->cmd;
}

static void next_at_command(struct cell *c) {
  c->cmd += cmdlen(c);
  while (c->cmds[c->cmd] == ';') c->cmd++;
}

static void handle_response(struct cell *cell, unsigned char *buf, size_t len) {
#define ISSPACE(x) ((x) == '\r' || (x) == '\n' || (x) == ' ')
#define ISPRINT(x) ((x) >= ' ' && (x) <= '~')
  if (len > 0 && ISSPACE(buf[0])) buf[0] = '\0';
  for (size_t i = len - 1; i > 0 && !ISPRINT(buf[i]); i--) buf[i] = '\0';
  cell->dbg("  RESP: (%d) [%s]\n", len, buf);
  if ((len >= 2 && memcmp(buf, "OK", 2) == 0) ||
      (len >= 7 && memcmp(buf, "CONNECT", 7) == 0)) {
    next_at_command(cell);  // Switch to the next command
    cell->expire = 0;       // Schedule it to be sent
  } else if (len >= 5 && memcmp(buf, "ERROR", 5) == 0) {
    cell->state = CELL_START;
  }
}

static void cmd_exec(struct cell *cell, unsigned long now) {
  int n = cmdlen(cell);
  const char *cmd = &cell->cmds[cell->cmd];
  // DEBUG(("EXEC [%.*s]\n", n, cmd));
  if (cell->expire == 0) {
    cell->dbg("Sending [%.*s]\n", n, cmd);
    cell->expire = now + TIMEOUT_MS;  // Set the expire
    cell->len = 0;                    // Clear off receive buffer
    cell->buf[0] = '\0';
    cell->tx(cmd, (size_t) n);  // Send command
    cell->tx("\r\n", 2);        // CR LF after
  } else if (now > cell->expire) {
    cell->dbg("Expired: [%.*s], buf %d [%s]\n", n, cmd, cell->len, cell->buf);
    cell->state = CELL_START;
  } else {
    size_t discard = 0;
    for (size_t i = 0; i < cell->len; i++) {
      if (cell->buf[i] != '\n') continue;
      handle_response(cell, &cell->buf[discard], i + 1 - discard);
      discard = i + 1;
    }
    memmove(cell->buf, &cell->buf[discard], (size_t)(cell->len - discard));
    cell->len -= discard;
  }
}

void cell_poll(struct cell *cell, unsigned long now) {
  switch (cell->state) {
    case CELL_START:
      cell->tx("+++", 3);
      cell->expire = now + 1200;
      cell->state = CELL_WAIT;
      break;
    case CELL_WAIT:
      if (now < cell->expire) break;
      cell->expire = 0;
      cell->cmd = 0;
      cell->state = CELL_AT;
      break;
    case CELL_AT:
      if (cell->cmds == NULL) break;
      cmd_exec(cell, now);
      if (cell->state != CELL_AT) break;
      if (cmdlen(cell) == 0) cell->len = 0, cell->state = CELL_OK;
      break;
    case CELL_PPP:
    case CELL_OK:
      break;
  }
}
