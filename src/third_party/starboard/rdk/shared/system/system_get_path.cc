// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "starboard/system.h"

#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>

#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/user.h"

namespace {

bool GetContentDirectory(char* out_path, int path_size)
{
  const char* paths = std::getenv("COBALT_CONTENT_DIR");
  if(paths){ // Treat the environment variable as PATH-like search variable
    std::stringstream pathsStream(paths);
    const std::string testFilePath = "/fonts/fonts.xml";
    std::string contentPath;
    while(getline(pathsStream, contentPath,':')){
      //check if fonts/fonts.xml file exists, if not, evaluate another path.
      std::string tmp = contentPath + testFilePath;
      if(SbFileExists(tmp.c_str())){
        return (starboard::strlcat<char>(out_path, contentPath.c_str(), path_size) < path_size);
      }
    }
    return false;
  } else { // Default to /usr/share/content/data if COBALT_CONTENT_PATH is not set
    return (starboard::strlcat<char>(out_path, "/usr/share/content/data", path_size) < path_size);
  }
}

// Gets the path to the cache directory, using the user's home directory.
bool GetCacheDirectory(char* out_path, int path_size) {
  char home_path[kSbFileMaxPath + 1];
  if (!SbUserGetProperty(SbUserGetCurrent(), kSbUserPropertyHomeDirectory,
                         home_path, kSbFileMaxPath)) {
    return false;
  }
  int result = SbStringFormatF(out_path, path_size, "%s/.cache", home_path);
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  return SbDirectoryCreate(out_path);
}

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (path_size < 1) {
    return false;
  }

  char path[kSbFileMaxPath + 1];
  ssize_t bytes_read = readlink("/proc/self/exe", path, kSbFileMaxPath);
  if (bytes_read < 1) {
    return false;
  }

  path[bytes_read] = '\0';
  if (bytes_read > path_size) {
    return false;
  }

  starboard::strlcpy<char>(out_path, path, path_size);
  return true;
}

// Places up to |path_size| - 1 characters of the path to the directory
// containing the current executable in |out_path|, ensuring it is
// NULL-terminated. Returns success status. The result being greater than
// |path_size| - 1 characters is a failure. |out_path| may be written to in
// unsuccessful cases.
bool GetExecutableDirectory(char* out_path, int path_size) {
  if (!GetExecutablePath(out_path, path_size)) {
    return false;
  }

  char* last_slash =
      const_cast<char*>(strrchr(out_path, '/'));
  if (!last_slash) {
    return false;
  }

  *last_slash = '\0';
  return true;
}

// Gets only the name portion of the current executable.
bool GetExecutableName(char* out_path, int path_size) {
  char path[kSbFileMaxPath] = {0};
  if (!GetExecutablePath(path, kSbFileMaxPath)) {
    return false;
  }

  const char* last_slash = strrchr(path, '/');
  if (starboard::strlcpy<char>(out_path, last_slash + 1, path_size) >= path_size) {
    return false;
  }
  return true;
}

// Gets the path to a temporary directory that is unique to this process.
bool GetTemporaryDirectory(char* out_path, int path_size) {
  auto* temp = std::getenv("COBALT_TEMP");
  if (temp) {
    out_path = temp;
    return true;
  }

  char binary_name[kSbFileMaxPath] = {0};
  if (!GetExecutableName(binary_name, kSbFileMaxPath)) {
    return false;
  }

  int result = SbStringFormatF(out_path, path_size, "/tmp/%s-%d", binary_name,
                               static_cast<int>(getpid()));
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }

  return true;
}
}  // namespace

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  char path[kSbFileMaxPath];
  path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory:
      if (!GetContentDirectory(path, kSbFileMaxPath)){
        return false;
      }
      break;

    case kSbSystemPathCacheDirectory:
      if (!GetCacheDirectory(path, kSbFileMaxPath)) {
        return false;
      }
      if (starboard::strlcat<char>(path, "/cobalt", kSbFileMaxPath) >= kSbFileMaxPath) {
        return false;
      }
      if (!SbDirectoryCreate(path)) {
        return false;
      }
      break;

    case kSbSystemPathDebugOutputDirectory:
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kSbFileMaxPath)) {
        return false;
      }
      if (starboard::strlcat<char>(path, "/log", kSbFileMaxPath) >= kSbFileMaxPath) {
        return false;
      }
      SbDirectoryCreate(path);
      break;

    case kSbSystemPathTempDirectory:
      if (!GetTemporaryDirectory(path, kSbFileMaxPath)) {
        return false;
      }
      SbDirectoryCreate(path);
      break;

    case kSbSystemPathTestOutputDirectory:
      return SbSystemGetPath(kSbSystemPathDebugOutputDirectory, out_path,
                             path_size);

    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);

    case kSbSystemPathFontConfigurationDirectory:
    case kSbSystemPathFontDirectory:
      return false;

    default:
      SB_NOTIMPLEMENTED() << "SbSystemGetPath not implemented for " << path_id;
      return false;
  }

  int length = strlen(path);
  if (length < 1 || length > path_size) {
    return false;
  }

  starboard::strlcpy<char>(out_path, path, path_size);
  return true;
}
