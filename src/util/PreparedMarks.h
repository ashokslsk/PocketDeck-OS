#pragma once

#include <cstdint>
#include <string>

// Explorer's "prepared" marks.
//   Book  1 tick: its book info is cached (Prepare Folder/Library, or opened).
//         2 ticks: Prepare Whole Book finished for the layout the book is read
//         with (prepared.sig matches layout.sig in its cache folder; reading
//         it with another font/size rewrites layout.sig, dropping to 1 tick).
//   Folder tick: Prepare Folder finished walking it. The mark stores a
//         fingerprint of the folder's entries; Explorer re-checks it whenever
//         the folder is opened and drops the mark if the folder changed.
// All marks are small files in rebuildable cache folders (Clear Prepared Data
// and Clear Reading Cache remove them).
namespace PreparedMarks {

enum class BookState : uint8_t { None = 0, Info = 1, Whole = 2 };

BookState bookState(const std::string& epubPath);

// Reader: the layout signature currently used for the book, and the one a
// completed Prepare Whole Book laid every chapter out with.
void writeLayoutSignature(const std::string& cachePath, uint32_t signature);
void writeWholeBookSignature(const std::string& cachePath, uint32_t signature);
void clearWholeBookSignature(const std::string& cachePath);

// Folders: fingerprint = sum of FsHelpers::entryHash over its entries.
void writeFolderMark(const std::string& folderPath, uint32_t fingerprint, uint32_t entries);
bool hasFolderMark(const std::string& folderPath);
// Drops the mark when the folder's current fingerprint differs.
void validateFolderMark(const std::string& folderPath, uint32_t fingerprint, uint32_t entries);
void dropFolderMark(const std::string& folderPath);

constexpr const char* kFolderMarksDir = "/.pocketdeck-os/system_cache/prepared-folders";

}  // namespace PreparedMarks
