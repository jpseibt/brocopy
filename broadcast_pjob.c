/*==============================================================================
  The idea of brocopy is to "broadcast" a file, copying it to paths defined in a
  .csv based on "keys" passed as arguments.

  Example .csv:
                key,path
                foo,C:\foo\bar\baz\bro.prn
                bar,\\123.12.1.12\Users\jose.seibt\Documents\foo\bro.prn
                baz,\\localhost\SharedPrinter

  Example call:
                broadcast_pjob.exe D:\foo\bar\baz\file.out D:\bar\paths.csv foo bar baz
                                                                             ^   ^   ^
                                                                             1   2   3
  Copies "D:\foo\bar\baz\file.out" to:
                1. "C:\foo\bar\baz\bro.prn"
                2. "\\123.12.1.12\Users\jose.seibt\Documents\foo\bro.prn"
                3. "\\localhost\SharedPrinter"

  This program aims to send a print job to n printers, using a Mfilemon port
  that is configured to create a file from the printer's driver output and pass
  its path to the program (argv[1]), the path to the CSV file that defines the
  destination paths (argv[2]), and one or more "keys" (argv[3]...) that will be
  matched against the first column of the given CSV.

  -> Mfilemon repo  - https://github.com/lomo74/mfilemon
  -> J. Paulo Seibt - https://jpseibt.github.io
  ==============================================================================*/

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define MAX_PATH 260
#else
#define _POSIX_C_SOURCE 200809L /* Exposes functions like readlink, hidden by -std=c99 */
#define MAX_PATH 4096
#include <unistd.h>
#endif

#include <time.h>
#include "src/brocopy.h"
#include "src/arena.c"
#include "src/cstring.c"
#include "src/string.c"

#define ARENA_SIZE 1048576 /* 1MB */
#define MAX_KEYS 1000
#define LOG_SEP_LINE "==================================================\n"
#define HELP_TEXT                                                            \
    "Usage: broadcast_pjob.exe <src_path> <csv_path> <key> [<key> ...]\n"    \
    "Args:\n"                                                                \
    "     <src_path>\tPath to the source file.\n"                            \
    "     <csv_path>\tPath to the .csv file defining copy destination.\n"    \
    "     <key>...  \tOne or more keys to match in the .csv first column.\n" \

// NOTE: The Str8.ptr is safe to use as a C string if constructed using
// str8_pushf or str8_snprintf -> vsnprintf always null-terminates

// Prototypes
static Str8 os_get_exe_path(Arena *arena);
static void log_date_hour(Arena *scratch, FILE *stream);
static int32_t set_paths_list(Arena *arena, Str8List *paths_list, Str8List *keys_list, Str8 stream);
static int32_t write_data_to_file(Str8 data, Str8 dest);

int main(int argc, char *argv[])
{
  Arena arena = arena_alloc(ARENA_SIZE);

  // Set log_path to /head/directory/log.txt (%:h/log.txt)
  Str8 exe_path = os_get_exe_path(&arena);
  Str8 log_path = str8_pushf(&arena, "%.*s%clog.txt", (int)str8_index_last_slash(exe_path), (char*)exe_path.ptr, OS_SLASH);

  FILE *log_stream = fopen((char*)log_path.ptr, "a");
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
    return 0;
  }

  Str8 src_path = str8_from_cstr_term(argv[1]);
  Str8 csv_path = str8_from_cstr_term(argv[2]);
  Str8List keys = {0};
  int32_t amt_keys = argc - 3;

  if (amt_keys > MAX_KEYS)
  {
    fprintf(log_stream, "Warning: Too many keys passed (%d). Truncated to MAX_KEYS=%d\n", amt_keys, MAX_KEYS);
    amt_keys = MAX_KEYS;
  }

  // Fill keys list starting from the 4th arg
  for (int32_t i = 0; i < amt_keys; ++i)
  {
    Str8Node *new_node = str8_list_push(&arena, &keys);
    if (!new_node)
    { // Highly unlikely, but still
      fprintf(log_stream, "Arena full. Aborting...\n");
      fprintf(log_stream, LOG_SEP_LINE);
      fclose(log_stream);
      arena_free(&arena);
      return 1;
    }
    new_node->str = str8_pushf(&arena, argv[i+3]);
  }

  //==================================================
  // Buffer .csv stream, parse it, and copy files
  //==================================================
  Str8 csv_stream_buf = str8_buffer_file(&arena, csv_path);

  if (csv_stream_buf.ptr == 0)
  {
    fprintf(log_stream, "Error: could not open the CSV (\"%s\")\n", (char*)csv_path.ptr);
    fprintf(log_stream, LOG_SEP_LINE);
    fclose(log_stream);
    arena_free(&arena);
    return 1;
  }

  fprintf(log_stream, "Bytes read from CSV (\"%s\"): %lu\n", (char*)csv_path.ptr, csv_stream_buf.size);

  Str8List paths = {0};
  int32_t amt_paths = set_paths_list(&arena, &paths, &keys, csv_stream_buf);
  fprintf(log_stream, "Amount of matches in CSV from arg keys: %d out of %d\n", amt_paths, amt_keys);

#ifndef _WIN32
  // On Linux, buffer source file data to use `write_data_to_file()`
  Str8 src_data = str8_buffer_file(&arena, src_path);
#endif

  Str8 dest_path = str8_push(&arena, MAX_PATH);
  int32_t result = 0;

  for (Str8Node *curr_node = paths.head; curr_node != NULL; curr_node = curr_node->next)
  {
    str8_snprintf(dest_path, "%.*s", (int)curr_node->str.size, (char*)curr_node->str.ptr);

#ifdef _WIN32
    result = CopyFile((char*)src_path.ptr, (char*)dest_path.ptr, FALSE);
#else
    result = write_data_to_file(src_data, dest_path);
#endif

    if (result)
    {
      fprintf(log_stream, "\"%s\" copied to \"%s\"\n", (char*)src_path.ptr, (char*)dest_path.ptr);
    }
    else
    {
      fprintf(log_stream, "Failed to copy \"%s\" to \"%s\"\n", (char*)src_path.ptr, (char*)dest_path.ptr);
    }
  }

  // Attempt to remove tmp file
  if (remove((char*)src_path.ptr) == 0) 
  {
    fprintf(log_stream, "File \"%s\" deleted successfully\n", (char*)src_path.ptr);
  }

  fprintf(log_stream, LOG_SEP_LINE);
  fclose(log_stream);
  arena_free(&arena);
  return 0;
}


static Str8
os_get_exe_path(Arena *arena)
{
  Str8 result = {0};
  char buf[MAX_PATH];

#ifdef _WIN32
  uint32_t len = GetModuleFileName(NULL, buf, sizeof(buf));
#else
  uint64_t len = readlink("/proc/self/exe", buf, sizeof(buf));
#endif

  buf[len] = '\0';
  result = str8_pushf(arena, buf);

  return result;
}

static void
log_date_hour(Arena *scratch, FILE *stream)
{
  Scratch tmp = scratch_start(scratch);

  struct tm *t = localtime(&(time_t){time(NULL)});
  Str8 time_str = str8_push(scratch, 32);
  strftime((char*)time_str.ptr, time_str.size, "%Y-%m-%d %H:%M:%S", t);

  fprintf(stream, LOG_SEP_LINE);
  fprintf(stream, "%s\n", time_str.ptr);

  scratch_end(tmp);
}

// Return number of paths that where succesfully parsed
static int32_t
set_paths_list(Arena *arena, Str8List *paths_list, Str8List *keys_list, Str8 stream)
{
  // Skip .csv header row
  Str8 cursor = str8_skip(stream, str8_index(stream, '\n') + 1);
  int32_t amt_paths = 0;

  while (cursor.size > 0)
  {
    Str8 line = str8_prefix(cursor, str8_index(cursor, '\n'));
    Str8 key_slice = str8_prefix(line, str8_index(line, ','));
    Str8 path_slice = str8_postfix(line, line.size - str8_index(line, ',') - 1);

    for (Str8Node *key_node = keys_list->head; key_node != NULL; key_node = key_node->next)
    {
      if (str8_match(key_node->str, key_slice, key_slice.size))
      {
        Str8Node *new_node = str8_list_push(arena, paths_list);
        if (!new_node) { return 0; }
        new_node->str = str8_pushf(arena, "%.*s", (int)str8_index(path_slice, '\r'), path_slice.ptr);
        ++amt_paths;
      }
    }

    cursor = str8_skip(cursor, line.size + 1);
  }

  return amt_paths;
}

static int32_t
write_data_to_file(Str8 data, Str8 dest)
{
  int32_t result = 0;
  if (data.size == 0) { return 0; }

  FILE *dest_stream = fopen((char*)dest.ptr, "wb");
  if (!dest_stream) { return 0; }

  uint64_t written = fwrite(data.ptr, 1, data.size, dest_stream);
  result = written == data.size;

  fclose(dest_stream);
  return result;
}
