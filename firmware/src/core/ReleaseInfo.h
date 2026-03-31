#pragma once

#include <Arduino.h>

struct ReleaseInfo {
  String version = "V0.1";
  String releaseDate = "2026-03-29";
  String branch = "trunk";
  String repositoryUrl = "https://github.com/duxman/DUXMAN-LED-NEXT";
  String repositoryGitUrl = "https://github.com/duxman/DUXMAN-LED-NEXT.git";

  static ReleaseInfo defaults();
  String toJson() const;
  bool applyJson(const String &payload, String *error = nullptr);
};