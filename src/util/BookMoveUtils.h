#pragma once

#include <string>

namespace BookMoveUtils {

enum class RenameMigrationResult {
  Success,
  RolledBack,
  KeepRenamed,
};

std::string buildReadFolderDestination(const std::string& srcPath);
// Prepares reader metadata, renames the physical book, then commits the state
// migration so one canonical metadata path is always available across resets.
RenameMigrationResult migrateRenamedBookState(const std::string& oldPath, const std::string& newPath,
                                              const std::string& oldCachePath, const std::string& title,
                                              const std::string& author, const char* bookType);
// After a whole folder was renamed or moved, carries one book's reader state
// (cache with progress and stats, bookmarks, clippings, recents, open book) from
// its old path to its new one. The book file itself must already be at newPath.
// Non-book files return true. Best effort: logs and returns false on a partial move.
bool migrateMovedBookState(const std::string& oldPath, const std::string& newPath);
bool migrateMovedEpubState(const std::string& oldPath, const std::string& newPath, const std::string& oldCachePath,
                           const std::string& title, const std::string& author, bool keepInRecents);

}  // namespace BookMoveUtils
