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

#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define MAX_PATH 260

#include <time.h>
#include "../src/brocopy.h"
#include "../src/arena.c"
#include "../src/cstring.c"
#include "../src/string.c"

#define ARENA_SIZE 1048576 /* 1MB */
#define MAX_KEYS 1000
#define LOG_SEP_LINE "==================================================\n"
#define HELP_TEXT \
    "Usage: broadcast_pjob.exe [options] <src_path> <csv_path> <key> [<key> ...]\n" \
    "Args:\n" \
    "     <src_path>\tPath to the source file.\n" \
    "     <csv_path>\tPath to the .csv file defining copy destination.\n" \
    "     <key>...  \tOne or more keys to match in the .csv first column (ignored if --all-csv-paths option is passed).\n" \
    "Options:\n" \
    "     -h, --help          \tShow this information.\n" \
    "     -log <path>         \tPath to the log file (opened in append mode).\n" \
    "     -v, --verbose       \tWrite log messages to stdout.\n" \
    "     -a, --all-csv-paths \tCopy source file to all paths defined in the CSV.\n" \
    "     -rm, --remove-src   \tTry to remove file at <src_path>.\n"


// NOTE: The Str8.ptr is safe to use as a C string if constructed using
// str8_pushf or str8_snprintf -> vsnprintf always null-terminates

typedef struct Config Config;
struct Config
{
  Str8 src_path;
  Str8 csv_path;
  Str8 log_path;
  Str8List keys;
  int32_t verbose;
  int32_t all_csv_paths;
  int32_t remove_src;
};

// Prototypes
static Str8 os_get_exe_path(Arena *arena);
static void log_date_hour(Arena *scratch, FILE *stream);
static int32_t set_paths_list_from_keys(Arena *arena, Str8List *paths_list, Str8List *keys_list, Str8 stream);
static int32_t set_paths_list_all_csv(Arena *arena, Str8List *paths_list, Str8 stream);

int main(int argc, char *argv[])
{
  Arena arena = arena_alloc(ARENA_SIZE);
  Config config = {0};
  FILE *log_stream = 0;
  int32_t amt_keys = 0;
  int32_t amt_paths = 0;

  //==================================================
  // Process args
  //==================================================
  if (argc < 2)
  {
    fprintf(stderr, "Not enough arguments provided (argc=%d)...\n", argc);
    fprintf(stderr, HELP_TEXT);
    return 0;
  }

  for (int32_t i = 1; i < argc; ++i)
  {
    Str8 curr_arg = str8_from_cstr_term(argv[i]);
    /*
       Obs: str8_from_cstr_term() macro is used to ensure null termination primarily for path arguments,
            but if changing the approach for curr_arg Str8 construction, remember to change this line at
            config.keys node insertion:
    */

    if (str8_equals(str8_from_lit_term("-h"), curr_arg) || str8_equals(str8_from_lit_term("--help"), curr_arg))
    {
      fprintf(stdout, HELP_TEXT);
      arena_free(&arena);
      return 1;
    }
    else if (str8_equals(str8_from_lit_term("-log"), curr_arg))
    {
      if (++i >= argc)
      {
        fprintf(stderr, "Error: -log requires a path.\n");
        arena_free(&arena);
        return 1;
      }
      else
      {
        config.log_path = str8_pushf(&arena, argv[i]);
        str8_normalize_slash(config.log_path);
      }
    }
    else if (str8_equals(str8_from_lit_term("-v"), curr_arg) || str8_equals(str8_from_lit_term("--verbose"), curr_arg))
    {
      config.verbose = 1;
    }
    else if (str8_equals(str8_from_lit_term("-a"), curr_arg) || str8_equals(str8_from_lit_term("--all-csv-paths"), curr_arg))
    {
      config.all_csv_paths = 1;
    }
    else if (str8_equals(str8_from_lit_term("-rm"), curr_arg) || str8_equals(str8_from_lit_term("--remove-src"), curr_arg))
    {
      config.remove_src = 1;
    }
    else
    { // Positional args
      if (config.src_path.ptr == 0)
      { // <src_path>
        config.src_path = str8_push_copy(&arena, curr_arg);
        str8_normalize_slash(config.src_path);
      }
      else if (config.csv_path.ptr == 0)
      { // <csv_path>
        config.csv_path = str8_push_copy(&arena, curr_arg);
        str8_normalize_slash(config.csv_path);
      }
      else if (config.all_csv_paths != 0)
      { // argument keys are ignored
        amt_keys = -1;
        break;
      }
      else
      { // <key>
        Str8Node *new_node = str8_list_push(&arena, &config.keys);
        if (!new_node)
        { // Highly unlikely, but still
          fprintf(stderr, "Error: Arena full. Aborting...\n");
          arena_free(&arena);
          return 1;
        }
        new_node->str = curr_arg;
        new_node->str.size -= 1; // "Forget" null terminator from str8_from_cstr_term() macro
        ++amt_keys;
      }
    }
  }

  // Check src and csv paths
  if (config.src_path.ptr == 0 || config.csv_path.ptr == 0)
  {
    fprintf(stderr, "Error: Missing <src_path> or <csv_path>.\n");
    fprintf(stderr, HELP_TEXT);
    arena_free(&arena);
    return 1;
  }

  { // Check if src and csv paths are accessible
    FILE *src_check = fopen((char*)config.src_path.ptr, "r");
    FILE *csv_check = fopen((char*)config.csv_path.ptr, "r");
    if (!src_check || !csv_check)
    {
      if (src_check) { fclose(src_check); }
      if (csv_check) { fclose(csv_check); }
      fprintf(stderr, "Error: \"%s\" or \"%s\" are inaccessible.\n", (char*)config.src_path.ptr, (char*)config.csv_path.ptr);
      arena_free(&arena);
      return 1;
    }
    fclose(src_check);
    fclose(csv_check);
  }

  // Check log file
  if (config.log_path.ptr)
  {
    log_stream = fopen((char*)config.log_path.ptr, "a");
    if (!log_stream)
    {
      fprintf(stderr, "Warning: Could not open log file at \"%s\". Fallback to default (at executable dir).\n", config.log_path.ptr);

      // Set log_path to exe /head/brolog.txt (%:h/brolog.txt).
      Str8 exe_path = os_get_exe_path(&arena);
      config.log_path = str8_pushf(&arena, "%.*s%cbrolog.txt",
                                   (int)str8_index_last_slash(exe_path), (char*)exe_path.ptr, OS_SLASH);
      log_stream = fopen((char*)config.log_path.ptr, "a");
    }
  }

  if (config.verbose) { fprintf(stdout, "Logging at \"%s\".\n", config.log_path.ptr); }

  // Init logging
  log_date_hour(&arena, log_stream);
  fprintf(log_stream, "Args:");
  for (int32_t i = 1; i < argc; ++i)
  {
    fprintf(log_stream, " %s", argv[i]);
  }
  fprintf(log_stream, "\n");

  if (amt_keys > MAX_KEYS)
  {
    fprintf(log_stream, "Warning: Too many keys passed (%d). Truncated to MAX_KEYS=%d\n", amt_keys, MAX_KEYS);
    if (config.verbose) { fprintf(stdout, "Warning: Too many keys passed (%d). Truncated to MAX_KEYS=%d\n", amt_keys, MAX_KEYS); }
    amt_keys = MAX_KEYS;
  }

  //==================================================
  // Buffer and parse .csv stream
  //==================================================
  Str8 csv_stream_buf = str8_buffer_file(&arena, config.csv_path);
  if (csv_stream_buf.ptr == 0)
  {
    fprintf(log_stream, "Error: could not buffer the CSV. Aborting...\n");
    if (config.verbose) { fprintf(stdout, "Error: could not buffer the CSV. Aborting...\n"); }
    fprintf(log_stream, LOG_SEP_LINE);
    fclose(log_stream);
    arena_free(&arena);
    return 1;
  }

  fprintf(log_stream, "Bytes read from CSV (\"%s\"): %lu\n", (char*)config.csv_path.ptr, csv_stream_buf.size);
  if (config.verbose) { fprintf(stdout, "Bytes read from CSV (\"%s\"): %lu\n", (char*)config.csv_path.ptr, csv_stream_buf.size); }

  Str8List paths = {0};
  if (amt_keys > 0)
  {
    amt_paths = set_paths_list_from_keys(&arena, &paths, &config.keys, csv_stream_buf);
    fprintf(log_stream, "Amount of matches in CSV from arg keys: %d out of %d\n", amt_paths, amt_keys);
    if (config.verbose) { fprintf(stdout, "Amount of matches in CSV from arg keys: %d out of %d\n", amt_paths, amt_keys); }
  }
  else
  {
    amt_paths = set_paths_list_all_csv(&arena, &paths, csv_stream_buf);
    fprintf(log_stream, "Amount of paths parsed in CSV: %d\n", amt_paths);
    if (config.verbose) { fprintf(stdout, "Amount of paths parsed in CSV: %d\n", amt_paths); }
  }

  //==================================================
  // Copy files in paths list
  //==================================================
  Str8 dest_path = str8_push(&arena, MAX_PATH);
  int32_t result = 0;

  for (Str8Node *curr_node = paths.head; curr_node != NULL; curr_node = curr_node->next)
  {
    str8_snprintf(dest_path, "%.*s", (int)curr_node->str.size, (char*)curr_node->str.ptr);
    str8_normalize_slash(dest_path);

    result = CopyFile((char*)config.src_path.ptr, (char*)config.csv_path.ptr, FALSE);

    if (result)
    {
      fprintf(log_stream, "\"%s\" copied to \"%s\"\n", (char*)config.src_path.ptr, (char*)dest_path.ptr);
      if (config.verbose) { fprintf(stdout, "\"%s\" copied to \"%s\"\n", (char*)config.src_path.ptr, (char*)dest_path.ptr); }
    }
    else
    {
      fprintf(log_stream, "Failed to copy \"%s\" to \"%s\"\n", (char*)config.src_path.ptr, (char*)dest_path.ptr);
      if (config.verbose) { fprintf(stdout, "Failed to copy \"%s\" to \"%s\"\n", (char*)config.src_path.ptr, (char*)dest_path.ptr); }
    }
  }

  // Attempt to remove tmp file
  if (config.remove_src)
  {
    int32_t result = remove((char*)config.src_path.ptr);
    if (result == 0)
    {
      fprintf(log_stream, "File \"%s\" removed successfully.\n", (char*)config.src_path.ptr);
      if (config.verbose) { fprintf(stdout, "File \"%s\" removed successfully.\n", (char*)config.src_path.ptr); }
    }
    else
    {
      fprintf(log_stream, "Could not remove \"%s\".\n", (char*)config.src_path.ptr);
      if (config.verbose) { fprintf(stdout, "Could not remove \"%s\".\n", (char*)config.src_path.ptr); }
    }
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

  uint32_t len = GetModuleFileName(NULL, buf, sizeof(buf));

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

// Return number of paths that where succesfully matched from keys list
static int32_t
set_paths_list_from_keys(Arena *arena, Str8List *paths_list, Str8List *keys_list, Str8 stream)
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
      if (str8_equals_insensitive(key_node->str, key_slice))
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

// Return number of paths that where succesfully parsed from stream
static int32_t
set_paths_list_all_csv(Arena *arena, Str8List *paths_list, Str8 stream)
{
  // Skip .csv header row
  Str8 cursor = str8_skip(stream, str8_index(stream, '\n') + 1);
  int32_t amt_paths = 0;

  while (cursor.size > 0)
  {
    Str8 line = str8_prefix(cursor, str8_index(cursor, '\n'));
    Str8 path_slice = str8_postfix(line, line.size - str8_index(line, ',') - 1);

    Str8Node *new_node = str8_list_push(arena, paths_list);
    if (!new_node) { return 0; }
    new_node->str = str8_pushf(arena, "%.*s", (int)str8_index(path_slice, '\r'), path_slice.ptr);

    if (++amt_paths > MAX_KEYS) { break; }
    cursor = str8_skip(cursor, line.size + 1);
  }

  return amt_paths;
}
