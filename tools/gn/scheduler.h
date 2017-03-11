// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCHEDULER_H_
#define TOOLS_GN_SCHEDULER_H_

#include <fstream>
#include <map>

#include "base/atomic_ref_count.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_worker_pool.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/label.h"
#include "tools/gn/source_file.h"
#include "tools/gn/token.h"

class Target;

// Maintains the thread pool and error state.
class Scheduler {
 public:
  Scheduler();
  ~Scheduler();

  bool Run();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return main_loop_.task_runner();
  }
  base::SequencedWorkerPool* pool() { return pool_.get(); }

  InputFileManager* input_file_manager() { return input_file_manager_.get(); }

  bool verbose_logging() const { return verbose_logging_; }
  void set_verbose_logging(bool v) { verbose_logging_ = v; }
  void set_verbose_log(const base::FilePath& file_name);

  bool env_logging() const { return env_logging_; }
  void set_env_logging(bool e) { env_logging_ = e; }

  // TODO(brettw) data race on this access (benign?).
  bool is_failed() const { return is_failed_; }

  void Log(const std::string& verb, const std::string& msg);
  void LogEnv(const std::string& var, const std::string& value);
  void FailWithError(const Err& err);

  void SaveEnvLog(const base::FilePath& file_name);

  void ScheduleWork(const base::Closure& work);

  void Shutdown();

  // Declares that the given file was read and affected the build output.
  //
  // TODO(brettw) this is global rather than per-BuildSettings. If we
  // start using >1 build settings, then we probably want this to take a
  // BuildSettings object so we know the depdency on a per-build basis.
  // If moved, most of the Add/Get functions below should move as well.
  void AddGenDependency(const base::FilePath& file);
  std::vector<base::FilePath> GetGenDependencies() const;

  // Tracks calls to write_file for resolving with the unknown generated
  // inputs (see AddUnknownGeneratedInput below).
  void AddWrittenFile(const SourceFile& file);

  // Schedules a file to be written due to a target setting write_runtime_deps.
  void AddWriteRuntimeDepsTarget(const Target* entry);
  std::vector<const Target*> GetWriteRuntimeDepsTargets() const;
  bool IsFileGeneratedByWriteRuntimeDeps(const OutputFile& file) const;

  // Unknown generated inputs are files that a target declares as an input
  // in the output directory, but which aren't generated by any dependency.
  //
  // Some of these files will be files written by write_file and will be
  // GenDependencies (see AddWrittenFile above). There are OK and include
  // things like response files for scripts. Others cases will be ones where
  // the file is generated by a target that's not a dependency.
  //
  // In order to distinguish these two cases, the checking for these input
  // files needs to be done after all targets are complete. This also has the
  // nice side effect that if a target generates the file we can find it and
  // tell the user which dependency is missing.
  //
  // The result returned by GetUnknownGeneratedInputs will not count any files
  // that were written by write_file during execution.
  void AddUnknownGeneratedInput(const Target* target, const SourceFile& file);
  std::multimap<SourceFile, const Target*> GetUnknownGeneratedInputs() const;
  void ClearUnknownGeneratedInputsAndWrittenFiles();  // For testing.

  // We maintain a count of the things we need to do that works like a
  // refcount. When this reaches 0, the program exits.
  void IncrementWorkCount();
  void DecrementWorkCount();

 private:
  void LogOnMainThread(const std::string& verb, const std::string& msg);
  void FailWithErrorOnMainThread(const Err& err);

  void DoTargetFileWrite(const Target* target);

  void DoWork(const base::Closure& closure);

  void OnComplete();

  base::MessageLoop main_loop_;
  scoped_refptr<base::SequencedWorkerPool> pool_;

  scoped_refptr<InputFileManager> input_file_manager_;

  base::RunLoop runner_;

  bool verbose_logging_;
  std::ofstream verbose_log_file_;

  bool env_logging_;

  base::AtomicRefCount work_count_;

  mutable base::Lock lock_;
  bool is_failed_;

  // Used to track whether the worker pool has been shutdown. This is necessary
  // to clean up after tests that make a scheduler but don't run the message
  // loop.
  bool has_been_shutdown_;

  // Protected by the lock. See the corresponding Add/Get functions above.
  std::vector<base::FilePath> gen_dependencies_;
  std::vector<SourceFile> written_files_;
  std::vector<const Target*> write_runtime_deps_targets_;
  std::multimap<SourceFile, const Target*> unknown_generated_inputs_;
  std::map<std::string, std::string> env_vars_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

extern Scheduler* g_scheduler;

#endif  // TOOLS_GN_SCHEDULER_H_
