#ifndef SD_H
#define SD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int lsdir(const char *path);
int mount_sd_card(void);
int write_to_file(const char *path, const char *file_name, size_t file_size, const char *data);

#endif // SD_H