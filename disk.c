#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "serial_bus.h"
#include "panic.h"



#define DISK_DEVICE_FIRST 8
#define DISK_DEVICE_MAX 4

#define DISK_ENTRY_SIZE 32 /* In the D64 file! */
#define DISK_WRITE_BUFFER_SIZE 32

#define DISK_SECTOR_SIZE 256
#define DISK_BLOCKS 683
#define DISK_SIZE (DISK_SECTOR_SIZE * DISK_BLOCKS) /* 174848 bytes */

#define DISK_ENTRY_MAX 128
#define DISK_LIST_SIZE ((DISK_ENTRY_MAX + 2) * 32) /* Add 2 for head/foot. */

typedef struct disk_entry_s {
  uint8_t type;
  uint8_t track;
  uint8_t sector;
  uint8_t *filename;
  uint16_t blocks;
  uint32_t bytes;
} disk_entry_t;

typedef struct disk_s {
  bool loaded;
  uint8_t bytes[DISK_SIZE];
  disk_entry_t entry[DISK_ENTRY_MAX];
  int entries;
  uint8_t list[DISK_LIST_SIZE];
  int list_size;
  int current_entry;
  int current_file_track;
  int current_file_sector;
  int current_file_byte_in_sector;
  uint32_t current_file_bytes_read;
  int current_list_byte_no;
} disk_t;

static disk_t disk_device[DISK_DEVICE_MAX];

static uint8_t disk_write_buffer[DISK_DEVICE_MAX][DISK_WRITE_BUFFER_SIZE];
static int disk_write_index[DISK_DEVICE_MAX];



static int disk_byte_offset(int track, int sector)
{
  int offset = 0;

  while (track-- > 0) {
    if (track >= 31) {
      offset += 17;
    } else if (track >= 25) {
      offset += 18; 
    } else if (track >= 18) {
      offset += 19;
    } else if (track >= 1) {
      offset += 21;
    }
  }

  return (offset + sector) * DISK_SECTOR_SIZE;
}



static uint32_t disk_entry_bytes(disk_t *disk, uint8_t track, uint8_t sector)
{
  uint32_t bytes = 0;
  int offset;

  do {
    offset = disk_byte_offset(track, sector);
    track  = disk->bytes[offset];
    sector = disk->bytes[offset + 0x01];
    if (track == 0) {
      bytes += sector;
    } else {
      bytes += 254;
    }
  } while (track != 0);

  return bytes;
}



static void disk_entry_parse(disk_t *disk)
{
  uint8_t track, sector;
  int offset;

  disk->entries = 0;

  track = 18; /* First directory track. */
  sector = 1; /* First directory sector. */
  do {
    offset = disk_byte_offset(track, sector);
    track = disk->bytes[offset]; /* Next directory track. */
    sector = disk->bytes[offset + 0x01]; /* Next directory sector. */

    for (int i = 0; i < (DISK_SECTOR_SIZE / DISK_ENTRY_SIZE); i++) {
      if (disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x05] == 0) {
        return; /* Blank filename, end of entries? */
      }
      disk->entry[disk->entries].type =
        disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x02];
      disk->entry[disk->entries].track =
        disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x03];
      disk->entry[disk->entries].sector =
        disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x04];
      disk->entry[disk->entries].filename =
        &disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x05];
      disk->entry[disk->entries].blocks =  
         disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x1E] +
        (disk->bytes[(i * DISK_ENTRY_SIZE) + offset + 0x1F] * 256);

      disk->entry[disk->entries].bytes = disk_entry_bytes(disk,
        disk->entry[disk->entries].track,
        disk->entry[disk->entries].sector);

      disk->entries++;
      if (disk->entries >= DISK_ENTRY_MAX) {
        return;
      }
    }
  } while (track != 0);
}



static void disk_list_create(disk_t *disk)
{
  int offset, n, space_tab;

  offset = disk_byte_offset(18, 0); /* Directory header. */

  /* Title: */
  disk->list[0x00] = 0x01;
  disk->list[0x01] = 0x04;
  disk->list[0x02] = 0x01;
  disk->list[0x03] = 0x01;
  disk->list[0x04] = 0x00;
  disk->list[0x05] = 0x00;
  disk->list[0x06] = 0x12;
  disk->list[0x07] = '"';
  for (int i = 0; i < 16; i++) {
    disk->list[0x08 + i] = disk->bytes[offset + 0x90 + i]; /* Disk Name */
    if (disk->list[0x08 + i] == 0xA0) {
      disk->list[0x08 + i] = 0x20; /* Convert shifted space to normal space. */
    }
  }
  disk->list[0x18] = '"';
  disk->list[0x19] = ' ';
  disk->list[0x1A] = disk->bytes[offset + 0xA2]; /* Disk ID */
  disk->list[0x1B] = disk->bytes[offset + 0xA3]; /* Disk ID */
  disk->list[0x1C] = ' ';
  disk->list[0x1D] = '2';
  disk->list[0x1E] = 'A';
  disk->list[0x1F] = 0x00;

  /* Entries: */
  for (int i = 0; i < disk->entries; i++) {
    if (((i + 3) * 32) > DISK_LIST_SIZE) {
      panic("Disk list size overflow!");
      return;
    }

    n = 0x20 + (i * 32);
    disk->list[n++] = 0x01;
    disk->list[n++] = 0x01;
    disk->list[n++] = disk->entry[i].blocks % 256;
    disk->list[n++] = disk->entry[i].blocks / 256;

    if (disk->entry[i].blocks > 99) {
      disk->list[n++] = ' ';
      space_tab = 0x18;
    } else if (disk->entry[i].blocks > 9) {
      disk->list[n++] = ' ';
      disk->list[n++] = ' ';
      space_tab = 0x19;
    } else {
      disk->list[n++] = ' ';
      disk->list[n++] = ' ';
      disk->list[n++] = ' ';
      space_tab = 0x1A;
    }

    disk->list[n++] = '"';
    for (int j = 0; j < 16; j++) {
      if (disk->entry[i].filename[j] == 0xA0) {
        break;
      }
      disk->list[n] = disk->entry[i].filename[j];
      n++;
    }
    disk->list[n++] = '"';

    while ((n % 0x20) < space_tab) {
      disk->list[n++] = ' ';
    }

    switch (disk->entry[i].type & 0x7F) {
    case 0:
      disk->list[n++] = 'D';
      disk->list[n++] = 'E';
      disk->list[n++] = 'L';
      break;
    case 1:
      disk->list[n++] = 'S';
      disk->list[n++] = 'E';
      disk->list[n++] = 'Q';
      break;
    case 2:
      disk->list[n++] = 'P';
      disk->list[n++] = 'R';
      disk->list[n++] = 'G';
      break;
    case 3:
      disk->list[n++] = 'U';
      disk->list[n++] = 'S';
      disk->list[n++] = 'R';
      break;
    case 4:
      disk->list[n++] = 'R';
      disk->list[n++] = 'E';
      disk->list[n++] = 'L';
      break;
    default:
      disk->list[n++] = '?';
      disk->list[n++] = '?';
      disk->list[n++] = '?';
      break;
    }

    while ((n % 0x20) < 0x1F) {
      disk->list[n++] = ' ';
    }
    disk->list[n++] = 0x00;
  }

  /* Footer: */
  n = 0x20 + (disk->entries * 32);
  disk->list[n++] = 0x01;
  disk->list[n++] = 0x01;
  disk->list[n++] = 0; /* NOTE: Just shows zero blocks free. */
  disk->list[n++] = 0;

  disk->list[n++] = 'B';
  disk->list[n++] = 'L';
  disk->list[n++] = 'O';
  disk->list[n++] = 'C';
  disk->list[n++] = 'K';
  disk->list[n++] = 'S';
  disk->list[n++] = ' ';
  disk->list[n++] = 'F';
  disk->list[n++] = 'R';
  disk->list[n++] = 'E';
  disk->list[n++] = 'E';
  disk->list[n++] = '.';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = ' ';
  disk->list[n++] = 0x00;
  disk->list[n++] = 0x00;
  disk->list[n++] = 0x00;

  disk->list_size = n;
}



int disk_load_d64(uint8_t device_no, const char *filename)
{
  FILE *fh;
  int c, n;
  disk_t *disk;

  if (device_no >= DISK_DEVICE_FIRST &&
      device_no < (DISK_DEVICE_FIRST + DISK_DEVICE_MAX)) {
    disk = &disk_device[device_no - DISK_DEVICE_FIRST];
  } else {
    return -1;
  }

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  n = 0;
  while ((c = fgetc(fh)) != EOF) {
    if (n >= DISK_SIZE) {
      break;
    }
    disk->bytes[n] = c;
    n++;
  }

  fclose(fh);

  disk_entry_parse(disk);
  disk_list_create(disk);

  disk->current_entry               = -1;
  disk->current_file_track          = 0;
  disk->current_file_sector         = 0;
  disk->current_file_byte_in_sector = 0;
  disk->current_file_bytes_read     = 0;
  disk->current_list_byte_no        = 0;

  disk->loaded = true;
  return 0;
}



static int disk_open(uint8_t device_no, uint8_t *filename)
{
  disk_t *disk;
  disk = &disk_device[device_no - DISK_DEVICE_FIRST];

  if (! disk->loaded) {
    return -1; /* No disk loaded! */
  }

  if (filename[0] == '$' && filename[1] == '\0') { /* List */
    disk->current_list_byte_no = 0;
    disk->current_entry = -1; /* Special value for listing. */

  } else if (filename[0] == '*' && filename[1] == '\0') { /* First */
    if (disk->entries <= 0) {
      return -1; /* No files! */
    }
    disk->current_file_byte_in_sector = 0;
    disk->current_file_bytes_read = 0;
    disk->current_entry = 0; /* First entry. */

  } else { /* Regular */
    disk->current_file_byte_in_sector = 0;
    disk->current_file_bytes_read = 0;

    /* Search and compare: */
    for (int i = 0; i < disk->entries; i++) {
      size_t match = 0;
      for (int j = 0; j < 16; j++) {
        if (disk->entry[i].filename[j] == 0xA0) {
          if (match == strlen((const char *)filename)) {
            disk->current_entry = i;
            return 0; /* Found a match. */
          }
          break;
        }
        if (filename[j] == '\0') {
          break;
        }
        if (disk->entry[i].filename[j] != filename[j]) {
          break;
        }
        match++;
      }
    }

    return -1; /* Filename not found. */
  }

  return 0;
}



static uint8_t disk_read_file(disk_t *disk)
{
  uint8_t byte;
  int offset;

  if (disk->current_file_byte_in_sector == 0) { /* First */
    disk->current_file_track  = disk->entry[disk->current_entry].track;
    disk->current_file_sector = disk->entry[disk->current_entry].sector;
    disk->current_file_byte_in_sector = 2;

  } else if (disk->current_file_byte_in_sector >= 256) { /* Next */
    offset = disk_byte_offset(disk->current_file_track,
                              disk->current_file_sector);
    disk->current_file_track  = disk->bytes[offset];
    disk->current_file_sector = disk->bytes[offset + 0x01];
    disk->current_file_byte_in_sector = 2;
  }

  offset = disk_byte_offset(disk->current_file_track,
                            disk->current_file_sector);
  byte = disk->bytes[offset + disk->current_file_byte_in_sector];
  disk->current_file_byte_in_sector++;
  disk->current_file_bytes_read++;

  return byte;
}



static uint8_t disk_read_byte(disk_t *disk, bool *last_byte)
{
  uint8_t byte;

  if (disk->current_entry == -1) {
    byte = disk->list[disk->current_list_byte_no];
    if (disk->current_list_byte_no >= (disk->list_size - 1)) {
      *last_byte = true;
    } else {
      *last_byte = false;
    }
    disk->current_list_byte_no++;
    return byte;

  } else {
    byte = disk_read_file(disk);
    if (disk->current_file_bytes_read >= 
      disk->entry[disk->current_entry].bytes) {
      *last_byte = true;
    } else {
      *last_byte = false;
    }
    return byte;
  }
}



static uint8_t disk_read(uint8_t device_no, bool *last_byte)
{
  return disk_read_byte(&disk_device[device_no - DISK_DEVICE_FIRST], last_byte);
}



static int disk_write(uint8_t device_no, uint8_t channel_no, uint8_t byte)
{
  int result = 0;

  if (disk_write_index[device_no - DISK_DEVICE_FIRST]
    >= DISK_WRITE_BUFFER_SIZE) {
    panic("Disk write buffer overflow!");
    return -1;
  }

  disk_write_buffer[device_no - DISK_DEVICE_FIRST]
    [disk_write_index[device_no - DISK_DEVICE_FIRST]] = byte;
  disk_write_index[device_no - DISK_DEVICE_FIRST]++;

  if (channel_no == 0 && byte == '\0') {
    result = disk_open(device_no,
      disk_write_buffer[device_no - DISK_DEVICE_FIRST]);
    disk_write_index[device_no - DISK_DEVICE_FIRST] = 0;
  }

  return result;
}



void disk_init(void)
{
  for (int i = 0; i < DISK_DEVICE_MAX; i++) {
    serial_bus_attach(i + DISK_DEVICE_FIRST, disk_read, disk_write);
    disk_device[i].loaded = false;
    disk_write_index[i] = 0;
  }
}



void disk_dump(FILE *fh)
{
  for (int i = 0; i < DISK_DEVICE_MAX; i++) {
    fprintf(fh, "Disk Device #%d\n", i + DISK_DEVICE_FIRST);

    if (disk_device[i].loaded) {
      fprintf(fh, "  Entries:\n");
      for (int j = 0; j < disk_device[i].entries; j++) {
        fprintf(fh, "    T%02d:S%02d '%s' Type: 0x%02x Blocks: %d, Bytes: %d\n",
          disk_device[i].entry[j].track,
          disk_device[i].entry[j].sector,
          disk_device[i].entry[j].filename,
          disk_device[i].entry[j].type,
          disk_device[i].entry[j].blocks,
          disk_device[i].entry[j].bytes);
      }

      fprintf(fh, "  Current:\n");
      fprintf(fh, "    Entry           : %d\n",
        disk_device[i].current_entry);
      fprintf(fh, "    Track           : %d\n",
        disk_device[i].current_file_track);
      fprintf(fh, "    Sector          : %d\n",
        disk_device[i].current_file_sector);
      fprintf(fh, "    Byte in Sector  : %d\n",
        disk_device[i].current_file_byte_in_sector);
      fprintf(fh, "    Total Bytes Read: %d\n",
        disk_device[i].current_file_bytes_read);

    } else {
      fprintf(fh, "  Not Loaded\n");
    }
  }
}



