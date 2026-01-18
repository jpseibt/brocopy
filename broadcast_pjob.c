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
void set_paths_buffer(char **paths_buf, char **path_keys, int32_t num_keys, char *stream, int32_t stream_len);

// Global general buffer (for testing!)
uint8_t gbuf[GBUF_STACK_SIZE];
uint8_t *gbuf_ptr = gbuf;

/* TODO: macro just for testing! No bounds check */
#define str8_at_gbuf(size) (Str8){ gbuf_ptr, size }; \
  gbuf_ptr += (size)

int main(int argc, char *argv[])
{
  if (_setmode(_fileno(stdin), _O_BINARY) == -1)
  {
    printf("Cannot set stdin to binary mode. Aborting...\n");
    return 1;
  }


  /*==================================================
    TODO: Implement configuration file parsing
    ==================================================*/

  // Get path of the .exe head to buffer the config file
  // Buffer up to 2KB of the stream on the stack (should be enough to hold ~20 windows paths)
  Str8 printer_paths[MAX_KEYS];
  Str8 csv_stream_buf = str8_at_gbuf(2048);
  Str8 csv_path = str8_at_gbuf(MAX_PATH);

  set_csv_path(csv_path);

  FILE *fconfig = fopen((char*)csv_path.ptr, "r");
  uint64_t fcsv_bytes_read = fread(csv_stream_buf.ptr, 1, csv_stream_buf.size, fconfig);

  set_paths_buffer(printer_paths, argv[1], argc - 1, csv_stream_buf.ptr, fcsv_bytes_read);
  fclose(fconfig);


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

  /*==================================================
    TODO: Create log file with job sizes
    ==================================================*/
  //CopyFile(temp_file_path, "\\\\xxx.xx.xx.x\\02_REST_SOBREMESA", FALSE);

  free(job_buf);
  return 0;
}


// TODO: mess function just for testing!
// Set up a null terminated Str8 with the path of the CSV_FILE_NAME
uint64_t set_csv_path(Str8 path_str)
{
  GetModuleFileName(NULL, path_str.ptr, path_str.size);

  char *file_name = CSV_FILE_NAME;
  uint64_t slash_idx = str8_index(path_str, '\\');

  for (uint64_t i = slash_idx + 1; i < path_str.size; ++i)
  {
    path_str.ptr[i] = *file_name++;
    if (*file_name == '\0')
    {
      path_str.ptr[++i] = *file_name;
      path_str.size = i;
      break;
    }
  }

  return path_str.size;
}

// TODO: is an unfinished and half-refactored mess
void set_paths_buffer(StrSlice *paths_buf, char **path_keys, int32_t num_keys, char *stream, int32_t stream_len)
{
  // skip csv header and newline char
  char *head_p = index(stream, '\n');
  if (!head_p || !*(head_p + 1)) return null;
  ++head_p; // Start of data

  StrSlice csv_key;
  StrSlice csv_path;

  for (int32_t i = 0; i < num_keys; ++i)
  {
    char *trav_p = head_p;
    int32_t match;
    do
    {
      csv_key.ptr = trav_p;

      trav_p = index(csv_key.ptr, ',');
      if (!trav_p) return null;
      csv_key.len = (int32_t)(trav_p - csv_key.ptr);

      csv_path.ptr = trav_p + 1;

      trav_p = index(csv_path.ptr, '\n');
      if (!trav_p) return null;
      csv_path.len = (int32_t)(trav_p - csv_path.ptr);

      match = str_match(path_keys[i], csv_key.ptr, str_len(path_keys[i]) 1);
      if (match)
      {
        paths_buf[i] = csv_path;
      }
    } while (!match && *++trav_p)
  }
}
