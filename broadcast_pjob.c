#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>

#define STDIN_BUF_SIZE 1048576 /* 1MB */
#define MAX_PATH 260
#define MAX_ARGS 20

#define nazstrlen(s) do {  \
  char *_s_tmp_ = (s);     \
  char *_p_tmp_ = _s_tmp_; \
  while (*_p_tmp_)         \
    ++_p_tmp_;             \
  _p_tmp_ - _s_tmp_;       \
} while (0)

// Prototypes
int32_t buf_stdin(uint8_t *buf);
int16_t get_head_dir(char *buf, int16_t size);

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

  // Get path of the .exe to read a config file on its directory
  char head_dir[MAX_PATH];
  get_head_dir(config_path, MAX_PATH);

  char *printer_paths[MAX_PATH];

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
  char *temp_file_path = "D:\\JP\\PWI_printer\\pjobs\\temp_job.prn";
  FILE *fp = fopen(temp_file_path, "wb");
  int32_t bytes_written = fwrite(job_buf, 1,bytes_read, fp);
  fclose(fp);

  printf("Bytes written to %s: %d\n", temp_file_path, bytes_written);

  /*==================================================
    TODO: Create log file with job sizes
    ==================================================*/
  //CopyFile(temp_file_path, "\\\\xxx.xx.xx.x\\02_REST_SOBREMESA", FALSE);

  free(job_buf);
  return 0;
}

// Buffer the executable directory
int16_t get_head_dir(char *buf, int16_t size)
{
  int16_t path_len = GetModuleFileName(NULL, buf, size);
  int16_t i, ch;

  for (i = path_len - 1, ch = buf[i]; i >= 0 && (ch != '\\' && ch != '/'); --i, ch = buf[i])
    ; // Loop until a '\' or '/' char is found

  buf[i] = '\0';
  return path_len - i;
}

// Reads stdin stream into buffer and returns bytes read (-1 for error)
int32_t buf_stdin(uint8_t *buf)
{
  int32_t bytes_read = fread(buf, 1, STDIN_BUF_SIZE, stdin);

  if (!ferror(stdin) && feof(stdin))
    return bytes_read;
  return -1;
}
