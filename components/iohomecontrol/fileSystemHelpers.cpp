#include "fileSystemHelpers.h"
#include "esp_log.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "iohc_fs";
static const char *FS_BASE = "/littlefs";

static void print_file_info(const char *path, const char *name, uint8_t level) {
  struct stat st{};
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
    ESP_LOGI(TAG, "%*s%s\t\t%ld", level * 2, "", name, (long) st.st_size);
  }
}

static void traverse_directory(const char *dir_name, uint8_t level) {
  DIR *dir = opendir(dir_name);
  if (dir == nullptr) {
    ESP_LOGW(TAG, "Cannot open directory: %s", dir_name);
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, entry->d_name);

    struct stat st{};
    if (stat(full_path, &st) != 0)
      continue;

    if (S_ISDIR(st.st_mode)) {
      ESP_LOGI(TAG, "%*s%s/", level * 2, "", entry->d_name);
      traverse_directory(full_path, level + 1);
    } else {
      print_file_info(full_path, entry->d_name, level);
    }
  }

  closedir(dir);
}

void listFS() {
  traverse_directory(FS_BASE, 0);
}

void cat(const char *fname) {
  char full_path[512];
  snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE, fname);

  FILE *file = fopen(full_path, "r");
  if (file == nullptr) {
    ESP_LOGW(TAG, "File %s does not exist", fname);
    return;
  }

  char buf[256];
  while (fgets(buf, sizeof(buf), file) != nullptr) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';
    ESP_LOGI(TAG, "%s", buf);
  }

  fclose(file);
}

void rm(const char *fname) {
  char full_path[512];
  snprintf(full_path, sizeof(full_path), "%s%s", FS_BASE, fname);

  if (unlink(full_path) == 0) {
    ESP_LOGI(TAG, "File %s removed", fname);
  } else {
    ESP_LOGW(TAG, "File %s does not exist", fname);
  }
}
