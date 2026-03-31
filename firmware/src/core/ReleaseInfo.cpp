#include "core/ReleaseInfo.h"

#include <ArduinoJson.h>

namespace {
void setIfPresent(const JsonObjectConst &obj, const char *key, String &target) {
  if (!obj[key].isNull()) {
    target = obj[key].as<String>();
  }
}
} // namespace

ReleaseInfo ReleaseInfo::defaults() {
  return ReleaseInfo{};
}

String ReleaseInfo::toJson() const {
  JsonDocument doc;
  doc["version"] = version;
  doc["releaseDate"] = releaseDate;
  doc["branch"] = branch;
  doc["repositoryUrl"] = repositoryUrl;
  doc["repositoryGitUrl"] = repositoryGitUrl;

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool ReleaseInfo::applyJson(const String &payload, String *error) {
  JsonDocument doc;
  const DeserializationError parseResult = deserializeJson(doc, payload);
  if (parseResult) {
    if (error != nullptr) {
      *error = "invalid_json";
    }
    return false;
  }

  JsonObjectConst root = doc.as<JsonObjectConst>();
  ReleaseInfo next = *this;
  setIfPresent(root, "version", next.version);
  setIfPresent(root, "releaseDate", next.releaseDate);
  setIfPresent(root, "branch", next.branch);
  setIfPresent(root, "repositoryUrl", next.repositoryUrl);
  setIfPresent(root, "repositoryGitUrl", next.repositoryGitUrl);

  if (next.version.isEmpty() || next.releaseDate.isEmpty() || next.branch.isEmpty() ||
      next.repositoryUrl.isEmpty() || next.repositoryGitUrl.isEmpty()) {
    if (error != nullptr) {
      *error = "invalid_release_info";
    }
    return false;
  }

  const bool changed = (next.toJson() != this->toJson());
  *this = next;
  if (error != nullptr) {
    error->clear();
  }
  return changed;
}