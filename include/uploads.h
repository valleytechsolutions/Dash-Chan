// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include <FS.h>
namespace dash {
struct UploadConfig {
  char ssid[33]{}, password[65]{}, wigleName[129]{}, wigleToken[129]{}, wdgKey[65]{};
  bool wigle = false, wdg = false, automatic = false, donate = false;
};
struct UploadStatus {
  bool busy = false, configured = false;
  bool wigle = false, wdg = false, automatic = false;
  char message[96] = "Set up uploads on the Dash-Chan website.";
  char files[24][40]{};
  unsigned count = 0, selected = 0;
};
extern UploadStatus uploads;
bool uploadConfigParse(const char *, UploadConfig &);
void uploadRefresh();
void uploadTick();
void uploadStart(bool automatic = false);
void uploadCancel();
void uploadAfterDrive();
bool uploadBusy();
bool uploadSetupCommand(const char *);
File storageOpen(const char *path, const char *mode = "r");
bool storageExists(const char *path);
bool storageRemove(const char *path);
bool storageMakeDir(const char *path);
bool storageRename(const char *from, const char *to);
} // namespace dash
