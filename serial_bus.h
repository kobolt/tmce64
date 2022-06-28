#ifndef _SERIAL_BUS_H
#define _SERIAL_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
  SERIAL_BUS_STATE_IDLE,
  SERIAL_BUS_STATE_WAIT_TALKER,
  SERIAL_BUS_STATE_READY,
  SERIAL_BUS_STATE_READ_BIT,
  SERIAL_BUS_STATE_READ_DONE,
  SERIAL_BUS_STATE_RELEASE_DATA,
  SERIAL_BUS_STATE_EOI_HANDSHAKE,
  SERIAL_BUS_STATE_WORKAROUND,
  SERIAL_BUS_STATE_TALKER_BECOME,
  SERIAL_BUS_STATE_TALKER_BECOME_ACK,
  SERIAL_BUS_STATE_TALKER_PREPARE,
  SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_READY,
  SERIAL_BUS_STATE_TALKER_WRITE_BIT_LOW,
  SERIAL_BUS_STATE_TALKER_WRITE_BIT_HIGH,
  SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_ACK,
  SERIAL_BUS_STATE_TALKER_EOI_WAIT_LOW,
  SERIAL_BUS_STATE_TALKER_EOI_WAIT_HIGH,
} serial_bus_state_t;

typedef enum {
  SERIAL_BUS_CONTROL_IDLE,
  SERIAL_BUS_CONTROL_LISTEN,
  SERIAL_BUS_CONTROL_TALK,
  SERIAL_BUS_CONTROL_WRITE,
} serial_bus_control_t;

typedef struct serial_bus_s {
  serial_bus_state_t state;
  serial_bus_control_t control;
  uint8_t device_no;
  uint8_t channel_no;
  int bit_count;
  uint8_t byte;
  bool listener_hold_data_line;
  bool listener_hold_clock_line;
  int wait_cycles;
  bool eoi_flag;
  bool file_not_found_error;
} serial_bus_t;

void serial_bus_init(serial_bus_t *serial_bus);
void serial_bus_execute(serial_bus_t *serial_bus, uint8_t *cia_data_port);
void serial_bus_dump(FILE *fh, serial_bus_t *serial_bus);

typedef uint8_t (*serial_bus_read_t)(uint8_t, bool *);
typedef int (*serial_bus_write_t)(uint8_t, uint8_t, uint8_t);
void serial_bus_attach(uint8_t device_no,
  serial_bus_read_t read, serial_bus_write_t write);

#endif /* _SERIAL_BUS_H */
