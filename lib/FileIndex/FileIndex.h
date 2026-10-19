#pragma once

#include <HalStorage.h>

#include <cstddef>
#include <cstdint>
#include <memory>

class FileIndex {
 public:
  static constexpr size_t MAX_NAME = 511;

  struct Entry {
    bool isDir;
    char name[MAX_NAME + 1];
  };

  using AcceptFn = bool (*)(const char* name, bool isDir);

  FileIndex() = default;
  ~FileIndex() { close(); }
  FileIndex(const FileIndex&) = delete;
  FileIndex& operator=(const FileIndex&) = delete;

  bool open(const char* dirPath, AcceptFn accept);
  // Opens the saved index for `dirPath` without re-reading the folder (instant
  // even for thousands of entries). Pair it with the verify calls below, which
  // re-read the folder a slice at a time and report whether it changed.
  bool openCached(const char* dirPath);
  bool beginVerify(const char* dirPath, AcceptFn accept);
  enum class VerifyStatus : uint8_t { Running, Unchanged, Changed };
  VerifyStatus verifyStep(uint32_t budgetMs);
  void endVerify();
  bool verifying() const { return static_cast<bool>(verifyDir); }
  // Order-independent fingerprint (FsHelpers::entryHash sum) and entry count of
  // the folder as last scanned by open() or a finished verify.
  uint32_t setFingerprint() const { return setFingerprint_; }
  uint32_t setEntries() const { return setEntries_; }
  void close();
  bool isOpen() const { return opened; }
  bool directoryReadFailed() const { return directoryReadFailed_; }

  size_t totalCount() const { return hdr.dirCount + hdr.fileCount; }

  bool entryAt(size_t row, Entry& out);
  size_t findRowByName(const char* name);

 private:
#pragma pack(push, 1)
  struct IndexHeader {
    char magic[4];
    uint8_t version;
    uint16_t pathLen;
    uint32_t dirSignature;
    uint32_t dirCount;
    uint32_t fileCount;
    uint32_t blobStart;
    uint32_t blobLen;
    uint32_t offsetsStart;
  };

  struct RecordHeader {
    uint8_t flags;  // bit0 = directory
    uint8_t reserved;
    uint16_t nameLen;
  };

  struct RunRecord {
    uint8_t key[28];
    uint32_t blobOffset;
  };
#pragma pack(pop)
  static_assert(sizeof(RunRecord) == 32, "run record packing");

  struct BuildState;

  bool scanDirectory(const char* dirPath, AcceptFn accept, uint32_t& signature, uint32_t& dirs, uint32_t& files);
  bool loadExisting(const char* dirPath, uint32_t signature, uint32_t dirs, uint32_t files,
                    bool checkSignature = true);
  bool build(const char* dirPath, AcceptFn accept, uint32_t signature, uint32_t dirs, uint32_t files);
  bool flushChunk(BuildState& bs);
  bool mergeRuns(BuildState& bs, uint32_t recordCount);
  bool writeOffsets(BuildState& bs, uint32_t recordCount);
  void makeKey(RunRecord& rec, bool isDir, const char* name) const;
  bool readNameAt(HalFile& file, uint32_t recordOffset, char* out, size_t cap);
  bool readOffsetForPhysIndex(size_t physIndex, uint32_t& recordOffset);

  HalFile idxFile;
  HalFile verifyDir;
  AcceptFn verifyAccept = nullptr;
  uint32_t verifySig = 0;
  uint32_t verifyDirs = 0;
  uint32_t verifyFiles = 0;
  std::unique_ptr<char[]> verifyName;
  uint32_t verifySetFp = 0;
  uint32_t verifySetCount = 0;
  uint32_t setFingerprint_ = 0;
  uint32_t setEntries_ = 0;
  IndexHeader hdr{};
  bool opened = false;
  bool directoryReadFailed_ = false;
  char idxPath[64] = {0};
  std::unique_ptr<char[]> nameBuf;

  static constexpr size_t OFFSETS_CACHE_ENTRIES = 64;
  uint32_t offsetsCache[OFFSETS_CACHE_ENTRIES] = {0};
  size_t offsetsCacheFirst = SIZE_MAX;
};
