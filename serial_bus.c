#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "serial_bus.h"
#include "panic.h"



#define SERIAL_BUS_DEVICE_MAX 31

#define SERIAL_BUS_ATN_OUT   3
#define SERIAL_BUS_CLOCK_OUT 4
#define SERIAL_BUS_DATA_OUT  5
#define SERIAL_BUS_CLOCK_IN  6
#define SERIAL_BUS_DATA_IN   7

#define SERIAL_BUS_EOI_RESPONSE_TIME 300
#define SERIAL_BUS_EOI_RESPONSE_HOLD_TIME 100
#define SERIAL_BUS_WORKAROUND_TIME 500

#define SERIAL_BUS_TRACE_BUFFER_SIZE 128

typedef struct serial_bus_trace_s {
  bool data;
  bool clock;
  bool atn;
  serial_bus_state_t state;
  uint32_t cycle;
  uint8_t byte;
} serial_bus_trace_t;

static serial_bus_trace_t serial_bus_trace_buffer[SERIAL_BUS_TRACE_BUFFER_SIZE];
static int serial_bus_trace_index = 0;

static serial_bus_read_t serial_bus_read[SERIAL_BUS_DEVICE_MAX];
static serial_bus_write_t serial_bus_write[SERIAL_BUS_DEVICE_MAX];



static char serial_bus_state_indicator(serial_bus_state_t state)
{
  switch (state) {
  case SERIAL_BUS_STATE_IDLE:
    return 'I';
  case SERIAL_BUS_STATE_WAIT_TALKER:
    return 'W';
  case SERIAL_BUS_STATE_READY:
    return 'R';
  case SERIAL_BUS_STATE_READ_BIT:
    return 'B';
  case SERIAL_BUS_STATE_READ_DONE:
    return 'D';
  case SERIAL_BUS_STATE_RELEASE_DATA:
    return 'L';
  case SERIAL_BUS_STATE_EOI_HANDSHAKE:
    return 'E';
  case SERIAL_BUS_STATE_WORKAROUND:
    return 'O';
  case SERIAL_BUS_STATE_TALKER_BECOME:
    return 'b';
  case SERIAL_BUS_STATE_TALKER_BECOME_ACK:
    return 'c';
  case SERIAL_BUS_STATE_TALKER_PREPARE:
    return 'p';
  case SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_READY:
    return 'r';
  case SERIAL_BUS_STATE_TALKER_WRITE_BIT_LOW:
    return 'l';
  case SERIAL_BUS_STATE_TALKER_WRITE_BIT_HIGH:
    return 'h';
  case SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_ACK:
    return 'a';
  case SERIAL_BUS_STATE_TALKER_EOI_WAIT_LOW:
    return 'e';
  case SERIAL_BUS_STATE_TALKER_EOI_WAIT_HIGH:
    return 'g';
  default:
    return '?';
  }
}



static char serial_bus_control_indicator(serial_bus_control_t control)
{
  switch (control) {
  case SERIAL_BUS_CONTROL_IDLE:
    return 'I';
  case SERIAL_BUS_CONTROL_LISTEN:
    return 'L';
  case SERIAL_BUS_CONTROL_TALK:
    return 'T';
  case SERIAL_BUS_CONTROL_WRITE:
    return 'W';
  default:
    return '?';
  }
}



static void serial_bus_trace_init(void)
{
  memset(serial_bus_trace_buffer, 0, 
    SERIAL_BUS_TRACE_BUFFER_SIZE * sizeof(serial_bus_trace_t));
  serial_bus_trace_index = 0;
}



static void serial_bus_trace_add(bool data, bool clock, bool atn,
  serial_bus_state_t state, uint8_t byte)
{
  static uint32_t cycle = 0;
  cycle++;

  if ((serial_bus_trace_buffer[serial_bus_trace_index].data  == data) &&
      (serial_bus_trace_buffer[serial_bus_trace_index].clock == clock) &&
      (serial_bus_trace_buffer[serial_bus_trace_index].atn   == atn)) {
    return;
  }

  serial_bus_trace_index++;
  if (serial_bus_trace_index >= SERIAL_BUS_TRACE_BUFFER_SIZE) {
    serial_bus_trace_index = 0;
  }

  serial_bus_trace_buffer[serial_bus_trace_index].data  = data;
  serial_bus_trace_buffer[serial_bus_trace_index].clock = clock;
  serial_bus_trace_buffer[serial_bus_trace_index].atn   = atn;
  serial_bus_trace_buffer[serial_bus_trace_index].state = state;
  serial_bus_trace_buffer[serial_bus_trace_index].cycle = cycle;
  serial_bus_trace_buffer[serial_bus_trace_index].byte  = byte;
}



static void serial_bus_trace_dump(FILE *fh)
{
  int i;
  uint32_t next_cycle;

  fprintf(fh, "Cycles: D C A  State:\n");
  for (i = 0; i < SERIAL_BUS_TRACE_BUFFER_SIZE; i++) {
    serial_bus_trace_index++;
    if (serial_bus_trace_index >= SERIAL_BUS_TRACE_BUFFER_SIZE) {
      serial_bus_trace_index = 0;
    }

    if (serial_bus_trace_index == SERIAL_BUS_TRACE_BUFFER_SIZE - 1) {
      next_cycle = serial_bus_trace_buffer[0].cycle;
    } else {
      next_cycle = serial_bus_trace_buffer[serial_bus_trace_index + 1].cycle;
    }

    fprintf(fh, "%-6u  %d %d %d  [%c] 0x%02x\n",
      next_cycle - serial_bus_trace_buffer[serial_bus_trace_index].cycle,
      serial_bus_trace_buffer[serial_bus_trace_index].data,
      serial_bus_trace_buffer[serial_bus_trace_index].clock,
      serial_bus_trace_buffer[serial_bus_trace_index].atn,
      serial_bus_state_indicator(
        serial_bus_trace_buffer[serial_bus_trace_index].state),
      serial_bus_trace_buffer[serial_bus_trace_index].byte);
  }
}



void serial_bus_init(serial_bus_t *serial_bus)
{
  serial_bus->state = SERIAL_BUS_STATE_IDLE;
  serial_bus->control = SERIAL_BUS_CONTROL_IDLE;
  serial_bus->device_no = 0;
  serial_bus->channel_no = 0;
  serial_bus->bit_count = 0;
  serial_bus->byte = 0;
  serial_bus->listener_hold_data_line = true;
  serial_bus->listener_hold_clock_line = false;
  serial_bus->wait_cycles = 0;
  serial_bus->eoi_flag = false;
  serial_bus->file_not_found_error = false;

  for (int i = 0; i < SERIAL_BUS_DEVICE_MAX; i++) {
    serial_bus_read[i] = NULL;
    serial_bus_write[i] = NULL;
  }

  serial_bus_trace_init();
}



void serial_bus_execute(serial_bus_t *serial_bus, uint8_t *cia_data_port)
{
  bool data;
  bool clock;
  bool atn;
  bool last_byte;

  /* Determine states of Data, Clock and ATN signals. */
  if (serial_bus->listener_hold_data_line) {
    data = true;
  } else {
    data = ((*cia_data_port >> SERIAL_BUS_DATA_OUT) & 0x1);
  }
  if (serial_bus->listener_hold_clock_line) {
    clock = true;
  } else {
    clock = ((*cia_data_port >> SERIAL_BUS_CLOCK_OUT) & 0x1);
  }
  atn = ((*cia_data_port >> SERIAL_BUS_ATN_OUT) & 0x1);

  serial_bus_trace_add(data, clock, atn, serial_bus->state, serial_bus->byte);

  /* Loopback inverted Data and Clock signals. */
  if (data) {
    *cia_data_port &= ~(1 << SERIAL_BUS_DATA_IN);
  } else {
    *cia_data_port |=  (1 << SERIAL_BUS_DATA_IN);
  }
  if (clock) {
    *cia_data_port &= ~(1 << SERIAL_BUS_CLOCK_IN);
  } else {
    *cia_data_port |=  (1 << SERIAL_BUS_CLOCK_IN);
  }

  /* Let listener statemachine act on signal states. */
  switch (serial_bus->state) {
  case SERIAL_BUS_STATE_IDLE:
    if (atn) {
      serial_bus->listener_hold_data_line = true; /* I'm here. */
      /* Handle protocol violation where ATN is set before Clock is released. */
      if (! clock) {
        serial_bus->wait_cycles = 0;
        serial_bus->state = SERIAL_BUS_STATE_WORKAROUND;
      } else {
        serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
      }
    }
    break;

  case SERIAL_BUS_STATE_WORKAROUND:
    serial_bus->wait_cycles++;
    if (serial_bus->wait_cycles > SERIAL_BUS_WORKAROUND_TIME) {
      serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
    }
    break;

  case SERIAL_BUS_STATE_WAIT_TALKER:
    if (! clock) { /* Talker ready to send a character. */
      serial_bus->bit_count = 0;
      serial_bus->byte = 0;
      serial_bus->wait_cycles = 0;
      serial_bus->eoi_flag = false;
      serial_bus->listener_hold_data_line = false; /* Ready to receive. */
      serial_bus->state = SERIAL_BUS_STATE_READY;
    }
    break;

  case SERIAL_BUS_STATE_READY:
    if (clock) {
      serial_bus->state = SERIAL_BUS_STATE_READ_BIT;
    } else {
      if ((! serial_bus->eoi_flag) && serial_bus->bit_count == 0) {
        serial_bus->wait_cycles++;
        if (serial_bus->wait_cycles > SERIAL_BUS_EOI_RESPONSE_TIME) {
          serial_bus->eoi_flag = true;
          serial_bus->listener_hold_data_line = true; /* I noticed the EOI. */
          serial_bus->state = SERIAL_BUS_STATE_EOI_HANDSHAKE;
        }
      }
    }
    break;

  case SERIAL_BUS_STATE_READ_BIT:
    if (! clock) { /* Data ready. */
      if (! data) {
        serial_bus->byte += (1 << serial_bus->bit_count);
      }
      serial_bus->bit_count++;
      if (serial_bus->bit_count >= 8) {
        serial_bus->state = SERIAL_BUS_STATE_READ_DONE;
      } else {
        serial_bus->state = SERIAL_BUS_STATE_READY;
      }
    }
    break;

  case SERIAL_BUS_STATE_READ_DONE:
    if (clock) { /* Data no longer ready. */
      serial_bus->listener_hold_data_line = true; /* Acknowledge. */

      switch (serial_bus->control) {
      case SERIAL_BUS_CONTROL_IDLE:
        if (serial_bus->byte >= 0x20 &&
            serial_bus->byte <= 0x3E) { /* LISTEN */
          serial_bus->device_no = serial_bus->byte - 0x20;
          serial_bus->control = SERIAL_BUS_CONTROL_LISTEN;

        } else if (serial_bus->byte >= 0x40 &&
                   serial_bus->byte <= 0x5E) { /* TALK */
          serial_bus->device_no = serial_bus->byte - 0x40;
          serial_bus->control = SERIAL_BUS_CONTROL_TALK;
        }
        serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
        break;

      case SERIAL_BUS_CONTROL_LISTEN:
        if (serial_bus->byte == 0x3F) { /* UNLISTEN */
          serial_bus->device_no = 0;
          serial_bus->control = SERIAL_BUS_CONTROL_IDLE;
          serial_bus->state = SERIAL_BUS_STATE_RELEASE_DATA;

        } else if (serial_bus->byte >= 0xE0 &&
                   serial_bus->byte <= 0xEF) { /* CLOSE */
          serial_bus->channel_no = 0;
          serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;

        } else if (serial_bus->byte >= 0xF0) { /* OPEN */
          serial_bus->channel_no = serial_bus->byte - 0xF0;
          serial_bus->control = SERIAL_BUS_CONTROL_WRITE;
          serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;

        } else {
          serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
        }
        break;

      case SERIAL_BUS_CONTROL_TALK:
        if (serial_bus->byte == 0x5F) { /* UNTALK */
          serial_bus->device_no = 0;
          serial_bus->control = SERIAL_BUS_CONTROL_IDLE;
          serial_bus->state = SERIAL_BUS_STATE_RELEASE_DATA;

        } else if (serial_bus->byte >= 0x60 &&
                   serial_bus->byte <= 0x6F) { /* OPEN CHANNEL / DATA */
          serial_bus->channel_no = serial_bus->byte - 0x60;
          serial_bus->state = SERIAL_BUS_STATE_TALKER_BECOME;

        } else {
          serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
        }
        break;

      case SERIAL_BUS_CONTROL_WRITE:
        if (serial_bus_write[serial_bus->device_no] == NULL) {
          panic("Device %d not attached!\n", serial_bus->device_no);
        }

        if (atn && serial_bus->byte == 0x3F) { /* UNLISTEN (w/ATN) */
          /* Write a '\0' to indicate the end. */
          if ((serial_bus_write[serial_bus->device_no])
            (serial_bus->device_no,
             serial_bus->channel_no, '\0') != 0) {
            serial_bus->file_not_found_error = true;
          }
          serial_bus->device_no = 0;
          serial_bus->control = SERIAL_BUS_CONTROL_IDLE;
          serial_bus->state = SERIAL_BUS_STATE_RELEASE_DATA;

        } else {
          /* Write the byte to the channel on the device. */
          if ((serial_bus_write[serial_bus->device_no])
            (serial_bus->device_no,
             serial_bus->channel_no,
             serial_bus->byte) != 0) {
            serial_bus->file_not_found_error = true;
          }
          serial_bus->state = SERIAL_BUS_STATE_WAIT_TALKER;
        }
        break;
      }
    }
    break;

  case SERIAL_BUS_STATE_RELEASE_DATA:
    if (! clock) {
      serial_bus->listener_hold_data_line = false;
      serial_bus->state = SERIAL_BUS_STATE_IDLE;
    }
    break;

  case SERIAL_BUS_STATE_EOI_HANDSHAKE:
    serial_bus->wait_cycles++;
    if (serial_bus->wait_cycles > 
      (SERIAL_BUS_EOI_RESPONSE_TIME + SERIAL_BUS_EOI_RESPONSE_HOLD_TIME)) {
      serial_bus->listener_hold_data_line = false; /* Handshake complete. */
      serial_bus->state = SERIAL_BUS_STATE_READY;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_BECOME:
    if (! clock) {
      serial_bus->wait_cycles = 0;
      serial_bus->listener_hold_data_line = false;
      serial_bus->listener_hold_clock_line = true; /* Become talker. */
      serial_bus->state = SERIAL_BUS_STATE_TALKER_BECOME_ACK;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_BECOME_ACK:
    serial_bus->wait_cycles++;
    if (serial_bus->wait_cycles > 100) {
      serial_bus->listener_hold_clock_line = false;
      serial_bus->state = SERIAL_BUS_STATE_TALKER_PREPARE;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_PREPARE:
    if (serial_bus->file_not_found_error) {
      serial_bus->listener_hold_data_line = false;
      serial_bus->listener_hold_clock_line = false;
      serial_bus->control = SERIAL_BUS_CONTROL_IDLE;
      serial_bus->state = SERIAL_BUS_STATE_IDLE;
      serial_bus->file_not_found_error = false;

    } else {
      serial_bus->bit_count = 0;
      if (serial_bus_read[serial_bus->device_no] != NULL) {
        serial_bus->byte = (serial_bus_read[serial_bus->device_no])
          (serial_bus->device_no, &last_byte);
      } else {
        panic("Device %d not attached!\n", serial_bus->device_no);
      }
      serial_bus->wait_cycles = 0;
      if (last_byte) {
        serial_bus->eoi_flag = true;
        serial_bus->state = SERIAL_BUS_STATE_TALKER_EOI_WAIT_LOW;
      } else {
        serial_bus->state = SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_READY;
      }
    }
    break;

  case SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_READY:
    if (! data) {
      serial_bus->listener_hold_data_line = true;
      serial_bus->listener_hold_clock_line = true;
      serial_bus->state = SERIAL_BUS_STATE_TALKER_WRITE_BIT_LOW;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_WRITE_BIT_LOW:
    serial_bus->wait_cycles++;
    if (serial_bus->wait_cycles > 30) {
      if (serial_bus->bit_count >= 8) {
        serial_bus->listener_hold_data_line = false;
        serial_bus->listener_hold_clock_line = true;
        serial_bus->state = SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_ACK;
      } else {
        if ((serial_bus->byte >> serial_bus->bit_count) & 0x1) {
          serial_bus->listener_hold_data_line = false;
        } else {
          serial_bus->listener_hold_data_line = true;
        }
        serial_bus->bit_count++;
        serial_bus->wait_cycles = 0;
        serial_bus->listener_hold_clock_line = false;
        serial_bus->state = SERIAL_BUS_STATE_TALKER_WRITE_BIT_HIGH;
      }
    }
    break;

  case SERIAL_BUS_STATE_TALKER_WRITE_BIT_HIGH:
    serial_bus->wait_cycles++;
    if (serial_bus->wait_cycles > 30) {
      serial_bus->listener_hold_clock_line = true;
      serial_bus->wait_cycles = 0;
      serial_bus->state = SERIAL_BUS_STATE_TALKER_WRITE_BIT_LOW;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_ACK:
    if (data) {
      serial_bus->listener_hold_clock_line = false;
      if (serial_bus->eoi_flag) {
        serial_bus->state = SERIAL_BUS_STATE_IDLE;
      } else {
        serial_bus->state = SERIAL_BUS_STATE_TALKER_PREPARE;
      }
    }
    break;

  case SERIAL_BUS_STATE_TALKER_EOI_WAIT_LOW:
    if (! data) {
      serial_bus->state = SERIAL_BUS_STATE_TALKER_EOI_WAIT_HIGH;
    }
    break;

  case SERIAL_BUS_STATE_TALKER_EOI_WAIT_HIGH:
    if (data) {
      serial_bus->state = SERIAL_BUS_STATE_TALKER_WAIT_LISTENER_READY;
    }
    break;
  }
}



void serial_bus_attach(uint8_t device_no,
  serial_bus_read_t read, serial_bus_write_t write)
{
  if (device_no < SERIAL_BUS_DEVICE_MAX) {
    serial_bus_read[device_no] = read;
    serial_bus_write[device_no] = write;
  }
}



void serial_bus_dump(FILE *fh, serial_bus_t *serial_bus)
{
  fprintf(fh, "State          : %c\n",
    serial_bus_state_indicator(serial_bus->state));
  fprintf(fh, "Control        : %c\n",
    serial_bus_control_indicator(serial_bus->control));
  fprintf(fh, "Device No      : %d\n", serial_bus->device_no);
  fprintf(fh, "Channel No     : %d\n", serial_bus->channel_no);
  fprintf(fh, "Hold Data Line : %d\n", serial_bus->listener_hold_data_line);
  fprintf(fh, "Hold Clock Line: %d\n", serial_bus->listener_hold_clock_line);
  fprintf(fh, "Byte           : 0x%02x\n", serial_bus->byte);
  fprintf(fh, "Bit Count      : %d\n", serial_bus->bit_count);
  fprintf(fh, "Wait Cycles    : %d\n", serial_bus->wait_cycles);
  fprintf(fh, "EOI Flag       : %d\n", serial_bus->eoi_flag);
  fprintf(fh, "File Not Found : %d\n", serial_bus->file_not_found_error);

  fprintf(fh, "----------------\n");
  serial_bus_trace_dump(fh);
}



