// SPDX-License-Identifier: 0BSD
#include "CmdSongToWav.hpp"
extern "C" {
  #include "json.h"
}
#include <stdio.h>
#include <string.h>
#include <stdint.h>

namespace DpcmTable {
  struct Entry {
    const uint8_t *sample;
    uint32_t size;
  };

  Entry *entries = nullptr;
}

#include "moonpak/Snd.cpp"
#include "moonpak/Snd.iwram.cpp"
#include "moonpak/SndData.cpp"

typedef uint32_t u32;
typedef uint16_t u16;

CmdSongToWav::CmdSongToWav() {
  Command::command = "songToWav";
  Command::about = "Convert a binary song into a WAV file";
}

static int printUsage(const char *err) {
  printf(
    "* songToWav -o <output.wav> -t <dpcm.json> -d <dir> <input.bin>\n\n"
    "Scans the PNG files, converts colors to RGB555, and outputs the final\n"
    "Renders the song <input.bin> into a WAV file <output.wav> using the\n"
    "DPCM table <dpcm.json> and DPCM WAV files in <dir>.\n\n"
    "  -o <output.wav>  Output file\n"
    "  -t <dpcm.json>   Input DPCM table (generated via famistudio.ts)\n"
    "  -d <dir>         Input directory of DPCM .WAV files \n"
    "  <input.bin>      The binary song file\n"
  );
  if (err) {
    fprintf(stderr, "\nError: %s\n", err);
    return 1;
  }
  return 0;
}

void CmdSongToWav::help() {
  printUsage(nullptr);
}

static void writeWAV(const char *file, int16_t *data, int size) {
  FILE *fp = fopen(file, "wb");
  #define U32(val)  do {            \
      uint32_t v = val;             \
      fputc(v & 0xff, fp);          \
      fputc((v >> 8) & 0xff, fp);   \
      fputc((v >> 16) & 0xff, fp);  \
      fputc((v >> 24) & 0xff, fp);  \
    } while (0)
  #define U16(val)  do {            \
      uint32_t v = val;             \
      fputc(v & 0xff, fp);          \
      fputc((v >> 8) & 0xff, fp);   \
    } while (0)
  U32(0x46464952);    // 'RIFF'
  U32(size * 2 + 36); // file size minus 'RIFF'
  U32(0x45564157);    // 'WAVE'
  U32(0x20746d66);    // 'fmt '
  U32(16);            // size of fmt chunk
  U16(1);             // audio format
  U16(1);             // mono
  U32(32768);         // sample rate
  U32(32768 * 2);     // bytes per second
  U16(2);             // block align
  U16(16);            // bits per sample
  U32(0x61746164);    // 'data'
  U32(size * 2);      // size of data chunk
  for (int i = 0; i < size; i++) {
    U16(data[i]);
  }
  #undef U32
  #undef U16
  fclose(fp);
}

int CmdSongToWav::main(int argc, const char **argv) {
  const char *output = NULL;
  const char *dpcmTable = NULL;
  const char *dpcmDir = NULL;
  const char *input = NULL;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      if (++i >= argc) {
        return printUsage("Missing output file after -o");
      }
      if (output) {
        return printUsage("Cannot set output file -o more than once");
      }
      output = argv[i];
    } else if (strcmp(argv[i], "-t") == 0) {
      if (++i >= argc) {
        return printUsage("Missing DPCM table file after -t");
      }
      if (dpcmTable) {
        return printUsage("Cannot set DPCM table file -t more than once");
      }
      dpcmTable = argv[i];
    } else if (strcmp(argv[i], "-d") == 0) {
      if (++i >= argc) {
        return printUsage("Missing DPCM directory after -d");
      }
      if (dpcmDir) {
        return printUsage("Cannot set DPCM directory -d more than once");
      }
      dpcmDir = argv[i];
    } else {
      if (input) {
        return printUsage("Cannot have multiple input files");
      }
      input = argv[i];
    }
  }

  if (!output) {
    return printUsage("Missing output file -o");
  }
  if (!dpcmTable) {
    return printUsage("Missing DPCM table file -t");
  }
  if (!dpcmDir) {
    return printUsage("Missing DPCM directory -d");
  }
  if (!input) {
    return printUsage("Missing input file");
  }

  // load DPCM table
  FILE *tblFp = fopen(dpcmTable, "rb");
  if (!tblFp) {
    fprintf(stderr, "Failed to read: %s\n", dpcmTable);
    return 1;
  }
  json_value *tbl = json_parsefp(tblFp);
  fclose(tblFp);
  if (tbl->kind != JSON_ARRAY) {
    fprintf(stderr, "Invalid DPCM table: %s\n", dpcmTable);
    return 1;
  }

  // construct DPCM table entries from reading WAV files off disk
  DpcmTable::entries =
    (DpcmTable::Entry *)malloc((tbl->u.array.count + 1) * sizeof(DpcmTable::Entry));
  for (int i = 0; i < tbl->u.array.count; i++) {
    json_value *ent = tbl->u.array.values[i];
    const char *name = json_string(json_objectkey(ent, "name"));
    int rate = json_number(json_objectkey(ent, "rate"));
    char path[10000];
    snprintf(path, sizeof(path), "%s/%s_%d.wav", dpcmDir, name, rate);
    FILE *fp = fopen(path, "rb");
    if (!fp) {
      fprintf(stderr, "Error: Failed to read DPCM wav file: %s\n", path);
      return 1;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *data = (uint8_t *)malloc(size);
    fread(data, size, 1, fp);
    fclose(fp);
    DpcmTable::entries[i].sample = data;
    DpcmTable::entries[i].size = size;
  }
  DpcmTable::entries[tbl->u.array.count].sample = nullptr;
  DpcmTable::entries[tbl->u.array.count].size = 0;
  json_free(tbl);

  // load the song
  uint8_t *song = nullptr;
  {
    FILE *fp = fopen(input, "rb");
    if (!fp) {
      fprintf(stderr, "Error: Failed to read input file: %s\n", input);
      return 1;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    song = (uint8_t *)malloc(size);
    fread(song, size, 1, fp);
    fclose(fp);
  }

  // render the song
  int16_t *out = nullptr;
  int outSize = 0;
  Snd snd(1, 0);
  snd.loadSong(0, song, 0);
  snd.setSongLoopsLeft(0, 0);
  while (!snd.isDone()) {
    int sampleCount = snd.tick();
    out = (int16_t *)realloc(out, sizeof(int16_t) * (outSize + sampleCount));
    for (int i = 0; i < sampleCount; i++) {
      out[outSize++] = snd.bufferTemp[i];
    }
  }

  // output WAV
  writeWAV(output, out, outSize);

  free(out);
  free(song);
  for (int i = 0; DpcmTable::entries[i].sample; i++) {
    free(const_cast<uint8_t *>(DpcmTable::entries[i].sample));
  }
  free(DpcmTable::entries);

  return 0;
}
