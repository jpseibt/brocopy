#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include "brocopy.h"

#define STDIN_BUF_SIZE 1048576 /* 1MB */
#define MAX_PATH 260
#define MAX_KEYS 20
#define CSV_PATHS_FILE "paths.csv"


// Prototypes
static inline uint32_t str_len(char *s);
int32_t buf_stdin(uint8_t *buf);
int32_t get_head_dir(char *buf, int32_t size);
int32_t str_append(char *buf, char *s, int32_t size);
void set_paths_buffer(char **paths_buf, char **path_keys, int32_t num_keys, char *stream, int32_t stream_len);
int32_t str_match(char *str0, char *str1, int32_t len, int32_t insensitive);

struct {
  char *ptr;
  int32_t len;
} StrSlice;

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
  uint8_t buf0[MAX_PATH], buf1[2048];
  Str8 config_path = str8_from_buf(buf0);
  Str8 config_fbuf = str8_from_buf(buf1);
  Str8 printer_paths[MAX_KEYS];

  set_head_dir(config_path, MAX_PATH);
  str_append(config_path, CSV_PATHS_FILE);

  FILE *fconfig = fopen(config_path, "r");
  int32_t fconfig_bytes_read = fread(config_buf, 1, sizeof(config_buf) - 1, fconfig);
  config_buf[fconfig_bytes_read] = '\0';

  set_paths_buffer(printer_paths, argv[1], argc - 1, config_buf, fconfig_bytes_read);
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
  char *temp_job_path = "D:\\JP\\PWI_printer\\pjobs\\temp_job.prn";
  FILE *ftemp_job = fopen(temp_job_path, "wb");
  int32_t bytes_written = fwrite(job_buf, 1, bytes_read, fp);
  fclose(ftemp_job);

  printf("Bytes written to %s: %d\n", temp_file_path, bytes_written);

  /*==================================================
    TODO: Create log file with job sizes
    ==================================================*/
  //CopyFile(temp_file_path, "\\\\xxx.xx.xx.x\\02_REST_SOBREMESA", FALSE);

  free(job_buf);
  return 0;
}


// Buffer the executable directory
int32_t set_head_dir(Str8 path_buf)
{
  path_buf.size = GetModuleFileName(NULL, path_buf.ptr, path_buf.size);
  int32_t i, ch;

  for (i = size)
  for (i = path_len - 1, ch = buf[i]; i >= 0 && (ch != '\\' && ch != '/'); --i, ch = buf[i])
    ; // Loop until a '\' or '/' char is found

  buf[i] = '\0';
  return path_len - i;
}

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
