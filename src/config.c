#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *s) {
  // Trim leading whitespace
  char *start = s;
  while (*start == ' ' || *start == '\t')
    start++;
  if (start != s)
    memmove(s, start, strlen(start) + 1);

  // Trim trailing whitespace and newlines
  int len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                      s[len - 1] == '\n' || s[len - 1] == '\r'))
    s[--len] = '\0';
}

void config_set_defaults(AppConfig *cfg) {
  memset(cfg, 0, sizeof(AppConfig));
  strncpy(cfg->download_path, "./downloads", sizeof(cfg->download_path) - 1);
}

int config_load(AppConfig *cfg) {
  char path[1024];
  const char *home = getenv("HOME");
  if (!home)
    return -1;

  snprintf(path, sizeof(path), "%s/.config/manga_reader/config.ini", home);

  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  char line[1024];
  char section[64] = "";

  while (fgets(line, sizeof(line), f)) {
    trim(line);

    // Skip empty lines and comments
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
      continue;

    // Section header
    if (line[0] == '[') {
      char *end = strchr(line, ']');
      if (end) {
        *end = '\0';
        strncpy(section, line + 1, sizeof(section) - 1);
      }
      continue;
    }

    // Key = Value
    char *eq = strchr(line, '=');
    if (!eq)
      continue;

    *eq = '\0';
    char *key = line;
    char *val = eq + 1;
    trim(key);
    trim(val);

    if (strcmp(section, "komga") == 0) {
      if (strcmp(key, "url") == 0)
        strncpy(cfg->komga_url, val, sizeof(cfg->komga_url) - 1);
      else if (strcmp(key, "api_key") == 0)
        strncpy(cfg->komga_api_key, val, sizeof(cfg->komga_api_key) - 1);
      else if (strcmp(key, "username") == 0)
        strncpy(cfg->komga_username, val, sizeof(cfg->komga_username) - 1);
      else if (strcmp(key, "password") == 0)
        strncpy(cfg->komga_password, val, sizeof(cfg->komga_password) - 1);
    } else if (strcmp(section, "downloads") == 0) {
      if (strcmp(key, "path") == 0)
        strncpy(cfg->download_path, val, sizeof(cfg->download_path) - 1);
    }
  }

  fclose(f);
  return 0;
}

int config_has_komga(const AppConfig *cfg) {
  return cfg->komga_url[0] != '\0';
}
