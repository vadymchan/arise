#include "file_loader/file_system_manager.h"

#include "utils/logger/global_logger.h"

#include <fstream>

namespace arise {
std::optional<std::string> FileSystemManager::readFile(const std::filesystem::path& filePath) {
  std::ifstream file(filePath, std::ios::in | std::ios::binary);
  if (!file) {
    GlobalLogger::Log(LogLevel::Error, "Failed to open file: " + filePath.string());
    return std::nullopt;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();
  return content;
}

bool FileSystemManager::writeFile(const std::filesystem::path& filePath, const std::string& content) {
  std::ofstream file(filePath, std::ios::out | std::ios::binary);
  if (!file) {
    GlobalLogger::Log(LogLevel::Error, "Failed to open file for writing: " + filePath.string());
    return false;
  }

  file << content;
  file.close();
  GlobalLogger::Log(LogLevel::Info, "File written successfully: " + filePath.string());
  return true;
}

bool FileSystemManager::fileExists(const std::filesystem::path& filePath) {
  return std::filesystem::exists(filePath);
}

std::vector<std::filesystem::path> FileSystemManager::getAllFilesInDirectory(const std::filesystem::path& dirPath) {
  std::vector<std::filesystem::path> filePaths;

  if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
      if (entry.is_regular_file()) {
        filePaths.push_back(entry.path());
      }
    }
  } else {
    GlobalLogger::Log(LogLevel::Error, "Directory does not exist or is not a directory: " + dirPath.string());
  }

  return filePaths;
}

bool FileSystemManager::createDirectory(const std::filesystem::path& dirPath) {
  if (!std::filesystem::exists(dirPath)) {
    if (std::filesystem::create_directory(dirPath)) {
      GlobalLogger::Log(LogLevel::Info, "Directory created successfully: " + dirPath.string());
      return true;
    } else {
      GlobalLogger::Log(LogLevel::Error, "Failed to create directory: " + dirPath.string());
      return false;
    }
  } else {
    GlobalLogger::Log(LogLevel::Warning, "Directory already exists: " + dirPath.string());
    return false;
  }
}

bool FileSystemManager::remove(const std::filesystem::path& filePath) {
  if (std::filesystem::exists(filePath)) {
    if (std::filesystem::remove_all(filePath) > 0) {
      GlobalLogger::Log(LogLevel::Info, "File or directory removed successfully: " + filePath.string());
      return true;
    } else {
      GlobalLogger::Log(LogLevel::Error, "Failed to remove file or directory: " + filePath.string());
      return false;
    }
  } else {
    GlobalLogger::Log(LogLevel::Warning, "File or directory does not exist: " + filePath.string());
    return false;
  }
}
}  // namespace arise
