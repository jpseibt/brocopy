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

#define STDIN_BUF_SIZE 1048576 /* 1MB */
#define GBUF_STACK_SIZE 20480
#define MAX_PATH 260
#define MAX_KEYS 20
#define CSV_FILE_NAME "paths.csv"

// Prototypes
//int32_t set_head_dir(char *buf, int32_t size);
uint64_t set_csv_path(Str8 path_str);
int32_t set_paths_buffer(Str8 *paths_arr, Str8 *keys_arr, int32_t n_keys, Str8 stream);

  // Global general buffer (for testing!)
uint8_t gbuf[GBUF_STACK_SIZE];
uint8_t *gbuf_ptr = gbuf;

//==================================================
// TODO: Implement some Str8 formatting and tokenization.
//==================================================

// TODO: macro just for testing! No bounds check
#define str8_at_gbuf(size) (Str8){ gbuf_ptr, size }; \
  gbuf_ptr += (size)

int main(int argc, char *argv[])
{
  if (_setmode(_fileno(stdin), _O_BINARY) == -1)
  {
    printf("Cannot set stdin to binary mode. Aborting...\n");
    return 1;
  }


  //==================================================
  // TODO: Implement configuration file parsing
  //==================================================

  // Buffer up to 2KB of the stream on the stack (should be enough to hold ~20 windows paths)
  Str8 csv_stream_buf = str8_at_gbuf(2048);
  Str8 csv_path = str8_at_gbuf(MAX_PATH);
  set_csv_path(csv_path);

  FILE *fcsv = fopen((char*)csv_path.ptr, "r");
  uint64_t fcsv_bytes_read = fread(csv_stream_buf.ptr, 1, csv_stream_buf.size, fcsv);
  csv_stream_buf.size = fcsv_bytes_read;

  Str8 keys[MAX_KEYS];
  Str8 printer_paths[MAX_KEYS];

  // Fill keys array from argv[1]
  for (int32_t i = 1; i < argc; ++i)
  {
    keys[i-1] = str8_from_chptr(argv[i]);
  }

  set_paths_buffer(printer_paths, keys, argc - 1, csv_stream_buf);
  fclose(fcsv);


  // Put stdin stream into memory
  uint8_t *job_buf = malloc(sizeof(uint8_t) * STDIN_BUF_SIZE);
  int32_t bytes_read = fread(job_buf, 1, STDIN_BUF_SIZE, stdin);
  if (ferror(stdin) || !feof(stdin))
  {
    printf("Error while reading. Aborting...\n");
    return 1;
  }

  printf("Bytes read from stdin: %d\n", bytes_read);

  // Write buffered stdin to a temp file
  char *temp_job_path = "D:\\JP\\brocopy\\pjobs\\temp_job.prn";
  FILE *ftemp_job = fopen(temp_job_path, "wb");
  int32_t bytes_written = fwrite(job_buf, 1, bytes_read, ftemp_job);
  fclose(ftemp_job);

  printf("Bytes written to %s: %d\n", temp_job_path, bytes_written);

  //==================================================
  // TODO: Create log file with job sizes
  // ==================================================
  //CopyFile(temp_file_path, "\\\\xxx.xx.xx.x\\02_REST_SOBREMESA", FALSE);

  free(job_buf);

  return 0;
}


// TODO: mess function just for testing!
// Set up a null terminated Str8 with the path of the CSV_FILE_NAME
uint64_t set_csv_path(Str8 path_str)
{
  GetModuleFileName(NULL, (char*)path_str.ptr, path_str.size);

  char *file_name = CSV_FILE_NAME;
  uint64_t slash_idx = str8_index_last(path_str, '\\');

  for (uint64_t i = slash_idx + 1; i < path_str.size; ++i)
  {
    path_str.ptr[i] = *file_name++;
    if (*file_name == '\0')
    {
      path_str.ptr[++i] = *file_name;
      path_str.size = i+1;
      break;
    }
  }

  return path_str.size;
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

