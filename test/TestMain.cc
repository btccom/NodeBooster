/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <signal.h>
#include <execinfo.h>
#include <string>
#include "gtest/gtest.h"

using std::string;

extern "C" {

static void handler(int sig);

// just for debug, should be removed when release
void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}
}

typedef char * CString;

int main(int argc, char **argv) {
  signal(SIGSEGV, handler);
  signal(SIGFPE, handler);
  signal(SIGPIPE, handler);
  
  CString * newArgv = new CString [argc];
  memcpy(newArgv, argv, argc * sizeof(CString));
  string testname = "--gtest_filter=";
  if (argc == 2 && newArgv[1][0] != '-') {
    testname.append(newArgv[1]);
    newArgv[1] = (char*)testname.c_str();
  }
  testing::InitGoogleTest(&argc, newArgv);
  
  int ret = RUN_ALL_TESTS();
  delete [] newArgv;
  return ret;
}


