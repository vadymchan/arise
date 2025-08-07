#include "config/config.h"

#include "file_loader/file_system_manager.h"
#include "utils/logger/log.h"

namespace arise {
void Config::reloadAsync() {
  LOG_INFO("Reloading config due to file modification...");
  loadFromFileAsync(m_filePath_);
  return;
}

void Config::loadFromFileAsync(const std::filesystem::path& filePath) {
  // Check that previous async load is complete
  asyncLoadComplete_();

  m_future_ = std::async(std::launch::async, &Config::loadFromFile, this, filePath);
}

bool Config::loadFromFile(const std::filesystem::path& filePath) {
  m_filePath_ = filePath;

  auto fileContent = FileSystemManager::readFile(filePath);
  if (!fileContent) {
    LOG_ERROR("Failed to read file: {}", filePath.string());
    return false;
  }

  m_root_.Parse(fileContent->c_str());

  if (m_root_.HasParseError()) {
    LOG_ERROR("Failed to parse JSON in file: {}", filePath.string());
    return false;
  }

  return true;
}

[[nodiscard]] std::string Config::toString() const {
  asyncLoadComplete_();
  if (!m_root_.IsObject()) {
    LOG_ERROR("Configuration not loaded or root is not an object.");
    return "";
  }

  rapidjson::StringBuffer                    buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  m_root_.Accept(writer);

  return buffer.GetString();
}

[[nodiscard]] const std::filesystem::path& Config::getFilename() const {
  return m_filePath_;
}

void Config::asyncLoadComplete_() const {
  if (m_future_.valid()) {
    m_future_.wait();
  }
}

const ConfigValue& Config::getMember_(const std::string& key) const {
  if (key.find('.') != std::string::npos) {
    std::string        currentKey   = key;
    const ConfigValue* currentValue = &m_root_;

    while (!currentKey.empty()) {
      size_t      dotPos  = currentKey.find('.');
      std::string keyPart = (dotPos != std::string::npos) ? currentKey.substr(0, dotPos) : currentKey;

      if (!currentValue->IsObject() || !currentValue->HasMember(keyPart.c_str())) {
        static ConfigValue nullValue(rapidjson::kNullType);
        LOG_ERROR("Key \"{}\" not found in config (failed at \"{}\").", key, keyPart);
        return nullValue;
      }

      currentValue = &(*currentValue)[keyPart.c_str()];

      if (dotPos != std::string::npos) {
        currentKey = currentKey.substr(dotPos + 1);
      } else {
        currentKey.clear();
      }
    }

    return *currentValue;
  }

  // Handle direct key access (original behavior)
  if (!m_root_.HasMember(key.c_str())) {
    static ConfigValue nullValue(rapidjson::kNullType);
    LOG_ERROR("Key \"{}\" not found in config.", key);
    return nullValue;
  }
  return m_root_[key.c_str()];
}
}  // namespace arise
