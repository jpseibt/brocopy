/*==============================================================================
  The idea of brocopy is to "broadcast" a file received on its standard input,
  copying it to paths defined in a .csv based on "keys" passed as arguments.

  Example .csv:
                key,path
                foo,C:\foo\
                bar,\\123.12.1.12\bar\

  This program aims to send a print job to n printers, using a Mfilemon port
  that is configured to create a file from the printer's driver output and pass
  its path to the program (argv[1]), the path to the CSV file that defines the
  destination paths (argv[2]), and one or more "keys" (argv[3]...) that will be
  matched against the first column of the given CSV.

  -> Mfilemon repo - https://github.com/lomo74/mfilemon

  TODO: Explore implementation with Mfilemon port, as it can also be configured
  to `Run as user` and with a `Domain` configuration for the launched program,
  which would help with network file transfers and integration with Windows Server.
  ==============================================================================*/

#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <time.h>

#include "src/brocopy.h"
#include "src/arena.c"
#include "src/cstring.c"
#include "src/string.c"

#define ARENA_SIZE 2097152 /* 2MB */
#define MAX_PATH 260
#define LOGS_PATH "D:\\JP\\brocopy\\logs\\broadlog.txt"
#define LOG_SEP_LINE "==================================================\n"
#define DATE_HOUR_FSTR "%Y-%m-%d %H:%M:%S"
#define HELP_TEXT \
    "Usage: broadcast_pjob.exe <src_path> <csv_path> <key> [<key> ...]\n"    \
    "Args:\n"                                                                \
    "     <src_path>\tPath to the source file.\n"                            \
    "     <csv_path>\tPath to the .csv file defining copy destination.\n"    \
    "     <key>...  \tOne or more keys to match in the .csv first column.\n" \

// NOTE: The Str8.ptr is safe to use as C strings if constructed using
// str8_pushf or str8_snprintf -> vsnprintf always null-terminates

// Prototypes
static void log_date_hour(Arena *scratch, FILE *stream);
static int32_t set_paths_arr(Arena *arena, Str8 *paths_arr, Str8 *keys_arr, int32_t amt_keys, Str8 stream);

int main(int argc, char *argv[])
{
  Arena arena = arena_alloc(ARENA_SIZE);
  FILE *log_stream = fopen(LOGS_PATH, "a");

  log_date_hour(&arena, log_stream);

  //==================================================
  // Process args
  //==================================================
  if (argc < 4)
  {
    fprintf(stderr, HELP_TEXT);
    fprintf(log_stream, "Not enough arguments provided (argc=%d)...\n", argc);
    fprintf(log_stream, LOG_SEP_LINE);
    fclose(log_stream);
    arena_free(&arena);
    return 1;
  }

  int32_t amt_keys = argc - 3;
  Str8 src_path = str8_from_cstr_term(argv[1]);
  Str8 csv_path = str8_from_cstr_term(argv[2]);
  Str8 keys[amt_keys];

  // Fill keys array starting from the 4th arg
  for (int32_t i = 3; i < argc; ++i)
  {
    keys[i - 3] = str8_from_cstr(argv[i]);
  }

  //==================================================
  // Buffer .csv stream, parse it, and copy files
  //==================================================
  Str8 csv_stream_buf = str8_buffer_file(&arena, csv_path);

  if (csv_stream_buf.ptr == 0)
  {
    fprintf(log_stream, "Error while opening the CSV (\"%s\")\n", (char*)csv_path.ptr);
    fprintf(log_stream, LOG_SEP_LINE);
    fclose(log_stream);
    arena_free(&arena);
    return 1;
  }

  fprintf(log_stream, "Bytes read from CSV (\"%s\"): %llu\n", (char*)csv_path.ptr, csv_stream_buf.size);

  // TODO: Improve CSV file parsing to populate an array of paths (Str8 Linked List)
  Str8 paths[amt_keys];
  int32_t amt_paths = set_paths_arr(&arena, paths, keys, amt_keys, csv_stream_buf);
  fprintf(log_stream, "Amount of matches in CSV from arg keys: %d out of %d\n", amt_paths, amt_keys);

  Str8 dest_path = str8_push(&arena, MAX_PATH);

  for (int32_t i = 0; i < amt_paths; ++i)
  {
    str8_snprintf(dest_path, "%.*s", (int)paths[i].size, paths[i].ptr);
    if (CopyFile((char*)src_path.ptr, (char*)dest_path.ptr, FALSE))
    {
      fprintf(log_stream, "\"%s\" file copied to \"%s\"\n", (char*)src_path.ptr, (char*)paths[i].ptr);
    }
    else
    {
      fprintf(log_stream, "Failed to copy \"%s\" to \"%s\"\n", (char*)src_path.ptr, (char*)paths[i].ptr);
    }
  }

  fprintf(log_stream, LOG_SEP_LINE);
  fclose(log_stream);
  arena_free(&arena);
  return 0;
}

static void
log_date_hour(Arena *scratch, FILE *stream)
{
  Scratch tmp = scratch_start(scratch);

  struct tm *t = localtime(&(time_t){time(NULL)});
  Str8 time_str = str8_push(scratch, 32);
  strftime((char*)time_str.ptr, time_str.size, DATE_HOUR_FSTR, t);

  fprintf(stream, LOG_SEP_LINE);
  fprintf(stream, "%s\n", time_str.ptr);

  scratch_end(tmp);
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
