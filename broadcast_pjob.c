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

#include <io.h>
#include <fcntl.h>
#include <windows.h>

#include "src/brocopy.h"
#include "src/arena.c"
#include "src/cstring.c"
#include "src/string.c"

#define ARENA_SIZE 2097152 /* 2MB */
#define STDIN_BUF_SIZE 1048576 /* 1MB */
#define MAX_PATH 260
#define MAX_KEYS 20
#define CSV_FILE_NAME "paths.csv"
#define TEMP_PRN_PATH "C:\\Git\\brocopy\\pjobs\\temp_job.prn"

// Prototypes
int32_t set_paths_arr(Arena *arena, Str8 *paths_arr, Str8 *keys_arr, int32_t amt_keys, Str8 stream);

int main(int argc, char *argv[])
{
  Arena arena = arena_alloc(ARENA_SIZE);
  uint64_t slash_idx;
  int32_t bytes_read_from_stream, bytes_written;


  //==================================================
  // Buffer stdin stream and write a tmp file from it
  //==================================================

  if (_setmode(_fileno(stdin), _O_BINARY) == -1)
  {
    printf("Cannot set stdin to binary mode. Aborting...\n");
    arena_free(&arena);
    return 1;
  }

  Str8 stdin_buf = str8_push(&arena, STDIN_BUF_SIZE);
  bytes_read_from_stream = fread(stdin_buf.ptr, 1, stdin_buf.size, stdin);
  if (ferror(stdin) || !feof(stdin))
  {
    printf("Error while buffering standard input. Aborting...\n");
    arena_free(&arena);
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
  // Buffer .csv stream
  //==================================================

  // Set a Str8 with the .exe full path
  Str8 exe_path = str8_push(&arena, MAX_PATH);
  GetModuleFileName(NULL, (char*)exe_path.ptr, exe_path.size);

  // Set a slice with the .exe head directory
  slash_idx = str8_index_last(exe_path, '\\');
  Str8 slice_exe_dir = { exe_path.ptr, slash_idx };
  Str8 csv_path = str8_pushf(&arena, "%.*s\\%s", slice_exe_dir.size, slice_exe_dir.ptr, CSV_FILE_NAME);

  Str8 csv_stream_buf = str8_buffer_file(&arena, csv_path);
  printf("Bytes read from CSV file: %llu\n", csv_stream_buf.size);

  //==================================================
  // TODO: Create log file with job sizes
  // ==================================================

  //==================================================
  // TODO: Improve CSV file parsing to populate an array of paths using the keys received from argv
  // ==================================================
  Str8 paths[argc - 1];
  Str8 keys[argc - 1];
  for (int i = 1; i < argc; ++i) 
  {
    keys[i-1] = str8_from_cstr(argv[i]);
  }

  int32_t amt_paths = set_paths_arr(&arena, paths, keys, argc - 1, csv_stream_buf);

  Scratch scratch = scratch_start(&arena);
  Str8 dest_path = str8_push(&arena, MAX_PATH);

  for (int32_t i = 0; i < amt_paths; ++i)
  {
    str8_snprintf(dest_path, "%.*s\\%s", (int)paths[i].size, paths[i].ptr, "test.txt");
    CopyFile((char*)temp_job_path.ptr, (char*)dest_path.ptr, FALSE);
    printf("\"%s\" file copied to \"%s\"\n", temp_job_path.ptr, paths[i].ptr);
  }

  scratch_end(scratch);

  arena_free(&arena);
  return 0;
}

// TODO: Maybe passing the paths and keys arrays as a linked list would be the best approach
// Return number of paths that where succesfully parsed
int32_t set_paths_arr(Arena *arena, Str8 *paths_arr, Str8 *keys_arr, int32_t amt_keys, Str8 stream)
{
  // Skip .csv header row
  int32_t paths_idx = 0;
  uint64_t data_start_idx = str8_index(stream, '\n') + 1;
  Str8 stream_cursor, line;

  for (int32_t i = 0; i < amt_keys; ++i)
  {
    stream_cursor = str8_skip(stream, data_start_idx);

    do
    {
      line = str8_prefix(stream_cursor, str8_index(stream_cursor, '\n'));
      Str8 key_slice = str8_prefix(line, str8_index(line, ','));
      Str8 path_slice = str8_postfix(line, line.size - str8_index(line, ',') - 1);

      if (str8_match(keys_arr[i], key_slice, keys_arr[i].size))
      {
        paths_arr[paths_idx] = str8_pushf(arena, "%.*s", (int)str8_index(path_slice, '\r'), path_slice.ptr);
        ++paths_idx;
      }

      stream_cursor = str8_skip(stream_cursor, line.size + 1);
    }
    while (stream_cursor.size > 0);
  }

  return paths_idx;
}
