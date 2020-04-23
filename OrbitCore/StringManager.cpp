#include "StringManager.h"

void StringManager::Add(uint64_t key, std::string_view str) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (key_to_string_.count(key) == 0) {
    key_to_string_.emplace(key, str);
  }
}

std::optional<std::string> StringManager::Get(uint64_t key) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = key_to_string_.find(key);
  if (it != key_to_string_.end()) {
    return it->second;
  } else {
    return std::optional<std::string>{};
  }
}

bool StringManager::Exists(uint64_t key) {
  std::lock_guard<std::mutex> lock(mutex_);
  return key_to_string_.count(key) > 0;
}

void StringManager::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  key_to_string_.clear();
}
