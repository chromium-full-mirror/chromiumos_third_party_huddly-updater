// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "manifest.h"

// Default path to fetch the manifest.json file.
const char kManifestJsonFile[] = "/lib/firmware/huddly/manifest.json";

int main(int argc, char* argv[]) {
  const base::FilePath path(FILE_PATH_LITERAL(kManifestJsonFile));
  huddly::Manifest manifest(path);

  if (!manifest.ParseFile()) {
    // Error messages are embedded within ParseFile().
    return EXIT_FAILURE;
  }

  manifest.Dump();

  /* other examples
  std::cout << "\n" << manifest.path();
  std::cout << "\n" << manifest.app_ver();
  std::cout << "\n" << manifest.boot_ver();
  std::cout << "\n" << manifest.hw_rev();
  std::cout << std::endl;
  */

  return EXIT_SUCCESS;
}
