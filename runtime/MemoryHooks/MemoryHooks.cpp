// Author: Jingyue

// Three types of messages:
// 1) Declare an addr-taken variable: allocator vid, start, bound
// 2) Top-level point to addr-taken: vid => pointee
// 3) Addr-taken point to addr-taken: pointer, pointee, instruction id of
//    the store instruction
//
// The third type of messages is not necessary for constructing a traditional
// point-to graph, because users query with top-level variables only. However,
// we put it there because we want to observe the shape.
//
// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <pthread.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>
#include <string>
#include <sstream>
#include <unistd.h>

#include "dyn-aa/LogRecord.h"

using namespace std;
using namespace dyn_aa;

struct Environment {
  FILE *LogFile;

  string GetLogFileName() {
    const char *LogFileEnv = getenv("LOG_FILE");
    string LogFileName;
    if (!LogFileEnv) {
      LogFileName = "/tmp/pts";
    } else {
      LogFileName = LogFileEnv;
    }
    ostringstream OS;
    OS << LogFileName << "-" << getpid();
    return OS.str();
  }

  Environment() {
    pthread_mutex_init(&Lock, NULL);
    OpenLogFile(false);
    assert(LogFile && "fail to open log file");
  }

  ~Environment() {
    CloseLogFile();
  }

  void CloseLogFile() {
    fclose(LogFile);
  }

  void OpenLogFile(bool append) {
    if (append)
      LogFile = fopen(GetLogFileName().c_str(), "ab");
    else
      LogFile = fopen(GetLogFileName().c_str(), "wb");
  }

  pthread_mutex_t Lock;
};

Environment *Global;

extern "C" void FinalizeMemHooks() {
  delete Global;
}

extern "C" void InitMemHooks() {
  Global = new Environment();
  atexit(FinalizeMemHooks);
}

// Must be called with Global->Lock held.
template<class T>
void PrintLogRecord(LogRecordType RecordType, const T &Record) {
  fwrite(&RecordType, sizeof RecordType, 1, Global->LogFile);
  fwrite(&Record, sizeof Record, 1, Global->LogFile);
}

extern "C" void HookBeforeFork() {
  Global->CloseLogFile();
}

extern "C" void HookFork(int Result) {
  if (Result == 0) {
    // child process: open a new log file
    Global->OpenLogFile(false);
  } else {
    // parent process: open the old log file
    Global->OpenLogFile(true);
  }
}

extern "C" void HookMemAlloc(unsigned ValueID, void *StartAddr,
                             unsigned long Bound) {
  // Bound is sometimes zero for array allocation.
  if (Bound > 0) {
    pthread_mutex_lock(&Global->Lock);
    // fprintf(stderr, "%u: HookMemAlloc(%p, %lu)\n", ValueID, Start, Bound);
    PrintLogRecord(AddrTakenDecl,
                   AddrTakenDeclLogRecord(StartAddr, Bound, ValueID));
    pthread_mutex_unlock(&Global->Lock);
  }
}

extern "C" void HookMainArgsAlloc(int Argc, char *Argv[],
                                  unsigned ArgvValueID) {
  HookMemAlloc(ArgvValueID, Argv, Argc * sizeof(char *));
  for (int i = 0; i < Argc; ++i)
    HookMemAlloc(-1, Argv[i], strlen(Argv[i]) + 1); // ends with '\0'
}

extern "C" void HookTopLevel(void *Value, void *Pointer, unsigned ValueID) {
  pthread_mutex_lock(&Global->Lock);
  // fprintf(stderr, "HookTopLevel(%p, %u)\n", Value, ValueID);
  PrintLogRecord(TopLevelPointTo,
                 TopLevelPointToLogRecord(ValueID, Value, Pointer));
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookAddrTaken(void *Value, void *Pointer, unsigned InsID) {
  pthread_mutex_lock(&Global->Lock);
  PrintLogRecord(AddrTakenPointTo,
                 AddrTakenPointToLogRecord(Pointer, Value, InsID));
  pthread_mutex_unlock(&Global->Lock);
}
