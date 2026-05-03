/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/core/ReleaseInfo.cpp
 * Last commit: 2c35a63 - 2026-04-28
 */

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