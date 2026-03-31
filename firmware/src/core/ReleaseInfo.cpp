#include "core/ReleaseInfo.h"
#include "core/BuildProfile.h"

#include <ArduinoJson.h>

String ReleaseInfo::toJson() {
  JsonDocument doc;
  doc["version"] = BuildProfile::kFwVersion;
  doc["releaseDate"] = BuildProfile::kFwDate;
  doc["branch"] = BuildProfile::kFwBranch;
  doc["board"] = BuildProfile::kName;
  doc["repositoryUrl"] = BuildProfile::kRepoUrl;
  doc["repositoryGitUrl"] = BuildProfile::kRepoGitUrl;

  String out;
  serializeJsonPretty(doc, out);
  return out;
}