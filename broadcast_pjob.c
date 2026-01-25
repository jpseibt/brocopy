/*==============================================================================
  The idea of brocopy is to "broadcast" a file received on its standard input,
  copying it to paths defined on a .csv based on "keys" passed as arguments.

  Example .csv:
                key,path
                foo,C:\foo\
                bar,\\123.12.1.12\bar\

  This program aims to send a print job to n printers, using a RedMon redirected port
  that calls it with args that will be used as keys and forward the job to stdin.
  -> RedMon overview - https://www.ghostgum.com.au/software/redmon.htm
  ==============================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include "src/brocopy.h"

#define ARENA_SIZE 2097152 /* 2MB */
#define STDIN_BUF_SIZE 1048576 /* 1MB */
#define MAX_PATH 260
#define MAX_KEYS 20
#define CSV_FILE_NAME "paths.csv"
#define TEMP_PRN_PATH "D:\\JP\\brocopy\\pjobs\\temp_job.prn"

// Prototypes
int32_t set_paths_buffer(Str8 *paths_arr, Str8 *keys_arr, int32_t n_keys, Str8 stream);

int main(int argc, char *argv[])
{
  if (_setmode(_fileno(stdin), _O_BINARY) == -1)
  {
    printf("Cannot set stdin to binary mode. Aborting...\n");
    return 1;
  }

  Arena arena = arena_alloc(ARENA_SIZE);
  uint64_t slash_idx;
  int32_t bytes_read_from_stream, bytes_written;

  //==================================================
  // Buffer stdin stream and write a tmp file from it
  //==================================================

  Str8 stdin_buf = str8_push(&arena, STDIN_BUF_SIZE);
  bytes_read_from_stream = fread(stdin_buf.ptr, 1, stdin_buf.size, stdin);
  if (ferror(stdin) || !feof(stdin))
  {
    printf("Error while buffering standard input. Aborting...\n");
    return 1;
  }
  printf("Bytes read from stdin: %d\n", bytes_read_from_stream);

  // NOTE: The Str8.ptr is safe to use as C strings if constructed using
  // str8_pushf or str8_snprintf -> vsnprintf always null-terminates

  // Write buffered stdin to a temp file
  Str8 temp_job_path = str8_push(&arena, MAX_PATH);
  str8_snprintf(temp_job_path, "%s", TEMP_PRN_PATH);

  FILE *ftemp_job = fopen((char*)temp_job_path.ptr, "wb");
  bytes_written = fwrite(stdin_buf.ptr, 1, bytes_read_from_stream, ftemp_job);
  printf("Bytes written to %s: %d\n", temp_job_path.ptr, bytes_written);
  fclose(ftemp_job);

  //==================================================
  // Buffer CSV_FILE_NAME stream
  //==================================================

  // Set a Str8 with the .exe full path
  Str8 exe_path = str8_push(&arena, MAX_PATH);
  GetModuleFileName(NULL, (char*)exe_path.ptr, exe_path.size);

  // Set a slice with the .exe head directory
  slash_idx = str8_index_last(exe_path, '\\');
  Str8 slice_exe_dir = { exe_path.ptr, slash_idx };
  Str8 csv_path = str8_pushf(&arena, "%.*s\\%s", slice_exe_dir.size, slice_exe_dir.ptr, CSV_FILE_NAME);

  // Buffer up to 2KB of the .csv stream (should be enough to hold ~20 windows paths).
  FILE *fcsv = fopen((char*)csv_path.ptr, "r");
  Str8 csv_stream_buf = str8_push(&arena, 2048);
  bytes_read_from_stream = fread(csv_stream_buf.ptr, 1, csv_stream_buf.size, fcsv);
  printf("Bytes read from CSV file: %d\n", bytes_read_from_stream);
  fclose(fcsv);

  //==================================================
  // TODO: Create log file with job sizes
  // ==================================================

  //==================================================
  // TODO: Finish CSV file parsing to populate an array of paths using the keys received from argv
  // ==================================================

  //CopyFile(temp_file_path.ptr, paths[i], FALSE);

  arena_free(&arena);
  return 0;
}

// TODO: is an unfinished and half-refactored mess
// Return number of paths that where succesfully parsed
int32_t set_paths_buffer(Str8 *paths_arr, Str8 *keys_arr, int32_t n_keys, Str8 stream)
{
  int32_t paths_idx, keys_idx, match;
  uint64_t stream_idx, data_start_idx, comma_idx;

  paths_idx = keys_idx = match = 0;
  data_start_idx = str8_index(stream, '\n') + 1; // Skip .csv header row

  for (; keys_idx < n_keys; ++keys_idx)
  {
    stream_idx = data_start_idx;

    for (match = 0; !match && stream_idx < stream.size; )
    {
      comma_idx = stream_idx;
      while (stream.ptr[++comma_idx] != ',');

      // TODO: This call will not work: needs a new Str8 starting from stream.ptr[comma_idx + 1]
      match = str8_match(keys_arr[keys_idx], stream, comma_idx - stream_idx);

      // Advance stream_idx one element past the next '\n'
      while (stream.ptr[stream_idx++] != '\n');
    }

    if (match)
    {
      uint8_t *path_data = stream.ptr + comma_idx + 1;

      paths_arr[paths_idx].ptr = path_data;
      // Magic for now:
      // stream_idx was incremented to one char past '\n' on the last for-loop iteration,
      // so the expression in parentheses evaluates to a ptr to the last char before the '\n'.
      paths_arr[paths_idx].size = (stream.ptr + stream_idx - 2) - path_data;
      ++paths_idx;
    }
  }

  return paths_idx + 1;
}
