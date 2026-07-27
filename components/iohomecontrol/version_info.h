#ifndef VERSION_INFO_H
#define VERSION_INFO_H

#include <string>

struct VersionInfoSnapshot {
  std::string version;
  std::string latestVersion;
  std::string releaseUrl;
  std::string error;
  bool currentIsDev;
  bool checkCompleted;
  bool checkOk;
  bool updateAvailable;
};

void initVersionInfo();
VersionInfoSnapshot getVersionInfo();

#endif
