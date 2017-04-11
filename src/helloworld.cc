// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

using namespace std;

const char* const COMPILED_AT = __DATE__ " " __TIME__;
int main(void)
{
  // Placeholder code.
  // TODO(porce): Implement the updater logics.
  cout << "Hello Huddly Updater" << endl;
  cout << "Compiled at: " << COMPILED_AT << endl;
  return EXIT_SUCCESS;
}
