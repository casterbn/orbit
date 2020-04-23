#ifndef ORBIT_LINUX_TRACING_UTILS_H_
#define ORBIT_LINUX_TRACING_UTILS_H_

#include <OrbitBase/Logging.h>
#include <OrbitBase/SafeStrerror.h>
#include <sys/resource.h>

#include <fstream>
#include <thread>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace LinuxTracing {

std::optional<std::string> ReadFile(std::string_view filename) {
  std::ifstream file{std::string{filename}, std::ios::in | std::ios::binary};
  if (!file) {
    ERROR("Could not open \"%s\"", std::string{filename}.c_str());
    return std::optional<std::string>{};
  }

  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

std::string ReadMaps(pid_t pid) {
  std::string maps_filename = absl::StrFormat("/proc/%d/maps", pid);
  std::optional<std::string> maps_content_opt = ReadFile(maps_filename);
  if (maps_content_opt.has_value()) {
    return maps_content_opt.value();
  } else {
    return "";
  }
}

std::optional<std::string> ExecuteCommand(const std::string& cmd) {
  std::unique_ptr<FILE, decltype(&pclose)> pipe{popen(cmd.c_str(), "r"),
                                                pclose};
  if (!pipe) {
    ERROR("Could not open pipe for \"%s\"", cmd.c_str());
    return std::optional<std::string>{};
  }

  std::array<char, 128> buffer;
  std::string result;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::vector<pid_t> ListThreads(pid_t pid) {
  std::vector<pid_t> threads;
  std::optional<std::string> tasks =
      ExecuteCommand(absl::StrFormat("ls /proc/%d/task", pid));
  if (!tasks.has_value()) {
    return {};
  }

  std::stringstream ss(tasks.value());
  std::string line;
  while (std::getline(ss, line, '\n')) {
    threads.push_back(std::stol(line));
  }

  return threads;
}

int GetNumCores() {
  int hw_conc = static_cast<int>(std::thread::hardware_concurrency());
  // Some compilers do not support std::thread::hardware_concurrency().
  if (hw_conc != 0) {
    return hw_conc;
  }

  std::optional<std::string> nproc_str = ExecuteCommand("nproc");
  if (nproc_str.has_value() && !nproc_str.value().empty()) {
    return std::stoi(nproc_str.value());
  }

  return 1;
}

// Read /proc/<pid>/cgroup.
static std::optional<std::string> ReadCgroupContent(pid_t pid) {
  std::string cgroup_filename = absl::StrFormat("/proc/%d/cgroup", pid);
  return ReadFile(cgroup_filename);
}

// Extract the cpuset entry from the content of /proc/<pid>/cgroup.
std::optional<std::string> ExtractCpusetFromCgroup(
    const std::string& cgroup_content) {
  std::istringstream cgroup_content_ss{cgroup_content};
  std::string cgroup_line;
  while (std::getline(cgroup_content_ss, cgroup_line)) {
    if (cgroup_line.find("cpuset:") != std::string::npos ||
        cgroup_line.find("cpuset,") != std::string::npos) {
      // For example "8:cpuset:/" or "8:cpuset:/game", but potentially also
      // "5:cpuacct,cpu,cpuset:/daemons".
      return cgroup_line.substr(cgroup_line.find_last_of(':') + 1);
    }
  }

  return std::optional<std::string>{};
}

// Read /sys/fs/cgroup/cpuset/<cgroup>/cpuset.cpus.
static std::optional<std::string> ReadCpusetCpusContent(
    const std::string& cgroup_cpuset) {
  std::string cpuset_cpus_filename =
      absl::StrFormat("/sys/fs/cgroup/cpuset%s/cpuset.cpus",
                      cgroup_cpuset == "/" ? "" : cgroup_cpuset);
  return ReadFile(cpuset_cpus_filename);
}

std::vector<int> ParseCpusetCpus(const std::string& cpuset_cpus_content) {
  std::vector<int> cpuset_cpus{};
  // Example of format: "0-2,7,12-14".
  for (const auto& range :
       absl::StrSplit(cpuset_cpus_content, ',', absl::SkipEmpty())) {
    std::vector<std::string> values = absl::StrSplit(range, '-');
    if (values.size() == 1) {
      int cpu = std::stoi(values[0]);
      cpuset_cpus.push_back(cpu);
    } else if (values.size() == 2) {
      for (int cpu = std::stoi(values[0]); cpu <= std::stoi(values[1]); ++cpu) {
        cpuset_cpus.push_back(cpu);
      }
    }
  }
  return cpuset_cpus;
}

// Read and parse /sys/fs/cgroup/cpuset/<cgroup_cpuset>/cpuset.cpus for the
// cgroup cpuset of the process with this pid.
// An empty result indicates an error, as trying to start a process with an
// empty cpuset fails with message "cgroup change of group failed".
std::vector<int> GetCpusetCpus(pid_t pid) {
  std::optional<std::string> cgroup_content_opt = ReadCgroupContent(pid);
  if (!cgroup_content_opt.has_value()) {
    return {};
  }

  // For example "/" or "/game".
  std::optional<std::string> cgroup_cpuset_opt =
      ExtractCpusetFromCgroup(cgroup_content_opt.value());
  if (!cgroup_cpuset_opt.has_value()) {
    return {};
  }

  // For example "0-2,7,12-14".
  std::optional<std::string> cpuset_cpus_content_opt =
      ReadCpusetCpusContent(cgroup_cpuset_opt.value());
  if (!cpuset_cpus_content_opt.has_value()) {
    return {};
  }

  return ParseCpusetCpus(cpuset_cpus_content_opt.value());
}

int GetTracepointId(const char* tracepoint_category,
                    const char* tracepoint_name) {
  std::string filename =
      absl::StrFormat("/sys/kernel/debug/tracing/events/%s/%s/id",
                      tracepoint_category, tracepoint_name);

  std::optional<std::string> file_content = ReadFile(filename);
  if (!file_content.has_value()) {
    return -1;
  }
  int tp_id = -1;
  if (!absl::SimpleAtoi(file_content.value(), &tp_id)) {
    ERROR("Error parsing tracepoint id for: %s:%s", tracepoint_category,
          tracepoint_name);
    return -1;
  }
  return tp_id;
}

uint64_t GetMaxOpenFilesHardLimit() {
  rlimit limit;
  int ret = getrlimit(RLIMIT_NOFILE, &limit);
  if (ret != 0) {
    ERROR("getrlimit: %s", SafeStrerror(errno));
    return 0;
  }
  return limit.rlim_max;
}

bool SetMaxOpenFilesSoftLimit(uint64_t soft_limit) {
  uint64_t hard_limit = GetMaxOpenFilesHardLimit();
  if (hard_limit == 0) {
    return false;
  }
  rlimit limit{soft_limit, hard_limit};
  int ret = setrlimit(RLIMIT_NOFILE, &limit);
  if (ret != 0) {
    ERROR("setrlimit: %s", SafeStrerror(errno));
    return false;
  }
  return true;
}

}  // namespace LinuxTracing

#endif  // ORBIT_LINUX_TRACING_UTILS_H_
