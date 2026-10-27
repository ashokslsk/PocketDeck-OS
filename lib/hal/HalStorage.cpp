#define HAL_STORAGE_IMPL
#include "HalStorage.h"

#include <string>

#include <cstring>

#include <Arduino.h>
#include <FS.h>  // need to be included before SdFat.h for compatibility with FS.h's File class
#include <HalClock.h>
#include <Logging.h>
#include <Memory.h>
#include <SDCardManager.h>
#if FREEINK_CAP_USB_MSC
#include <UsbMassStorage.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>

#include "HalSpiBus.h"

#define SDCard SDCardManager::getInstance()

HalStorage HalStorage::instance;

#if FREEINK_CAP_USB_MSC
class HalStorage::UsbDriveContext {
 public:
  freeink::UsbMassStorage massStorage;
};
#endif

namespace {
constexpr uint16_t kFallbackYear = 2024;
constexpr uint8_t kFallbackMonth = 1;
constexpr uint8_t kFallbackDay = 1;
constexpr uint8_t kFallbackHour = 0;
constexpr uint8_t kFallbackMinute = 0;
const uint8_t* clockUtcOffsetQ = nullptr;

bool isLeapYear(const uint16_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

bool isValidFatDateTime(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour,
                        const uint8_t minute) {
  if (year < 1980 || year > 2107 || hour > 23 || minute > 59) return false;
  const uint8_t monthDays = daysInMonth(year, month);
  return monthDays > 0 && day >= 1 && day <= monthDays;
}

void adjustDateByDays(uint16_t& year, uint8_t& month, uint8_t& day, const int dayDelta) {
  if (dayDelta > 0) {
    const uint8_t monthDays = daysInMonth(year, month);
    if (day < monthDays) {
      day++;
      return;
    }
    day = 1;
    if (month < 12) {
      month++;
    } else {
      month = 1;
      year++;
    }
  } else if (dayDelta < 0) {
    if (day > 1) {
      day--;
      return;
    }
    if (month > 1) {
      month--;
    } else {
      month = 12;
      year--;
    }
    day = daysInMonth(year, month);
  }
}

void setPackedFatDateTime(uint16_t* date, uint16_t* time, const uint16_t year, const uint8_t month, const uint8_t day,
                          const uint8_t hour, const uint8_t minute) {
  *date = FS_DATE(year, month, day);
  *time = FS_TIME(hour, minute, 0);
}

void storageDateTimeCallback(uint16_t* date, uint16_t* time) {
  uint16_t year = kFallbackYear;
  uint8_t month = kFallbackMonth;
  uint8_t day = kFallbackDay;
  uint8_t hour = kFallbackHour;
  uint8_t minute = kFallbackMinute;

  if (halClock.getDateTime(year, month, day, hour, minute) && isValidFatDateTime(year, month, day, hour, minute)) {
    const uint8_t configuredOffsetQ = clockUtcOffsetQ ? *clockUtcOffsetQ : 48;
    const uint8_t offsetQ = configuredOffsetQ > 104 ? 104 : configuredOffsetQ;
    const int offsetQuarterHours = static_cast<int>(offsetQ) - 48;
    int localMinutes = static_cast<int>(hour) * 60 + static_cast<int>(minute) + offsetQuarterHours * 15;
    const int dayDelta = localMinutes < 0 ? -1 : (localMinutes >= 1440 ? 1 : 0);
    localMinutes = ((localMinutes % 1440) + 1440) % 1440;
    adjustDateByDays(year, month, day, dayDelta);
    hour = static_cast<uint8_t>(localMinutes / 60);
    minute = static_cast<uint8_t>(localMinutes % 60);
  }

  if (!isValidFatDateTime(year, month, day, hour, minute)) {
    year = kFallbackYear;
    month = kFallbackMonth;
    day = kFallbackDay;
    hour = kFallbackHour;
    minute = kFallbackMinute;
  }

  setPackedFatDateTime(date, time, year, month, day, hour, minute);
}
}  // namespace

HalStorage::HalStorage()
#if FREEINK_CAP_USB_MSC
    : usbDriveContext(new (std::nothrow) UsbDriveContext())
#endif
{
  storageMutex = xSemaphoreCreateMutex();
  assert(storageMutex != nullptr);
}

HalStorage::~HalStorage() = default;

// begin() and ready() are only called from setup, no need to acquire mutex for them

HalStorage::RootAlias HalStorage::rootAliases_[3] = {
    {"/tools", "/.tools", true, true, false},
    {"/My Clippings.txt", "/.clippings/My Clippings.txt", true, true, false},
    // Finished books move here; hiding it hides them from Explorer, so only
    // when the reader renamed it.
    {"/Read", "/.Read", false, false, false},
};

bool HalStorage::begin() {
  bool ok = false;
  {
    HalSpiBus::Lock spiLock;
    ok = SDCard.begin();
  }
  if (ok) resolveRootAliases();
  return ok;
}

void HalStorage::resolveRootAliases() {
  for (auto& alias : rootAliases_) {
    alias.useHidden = false;  // probe the real names
    bool hidden = SDCard.exists(alias.hidden);
    const bool visible = SDCard.exists(alias.visible);
    // The hidden home may sit in a folder of its own (/.clippings/...).
    const char* slash = strrchr(alias.hidden, '/');
    const std::string parent(alias.hidden, slash && slash != alias.hidden ? slash - alias.hidden : 0);
    if (!hidden && visible && alias.moveVisible) {
      // Rename only (instant, nothing copied or deleted); left alone on failure.
      if (!parent.empty()) SDCard.mkdir(parent.c_str());
      hidden = SDCard.rename(alias.visible, alias.hidden);
      LOG_INF("SD", "Moved %s to %s: %s", alias.visible, alias.hidden, hidden ? "ok" : "failed");
    }
    alias.useHidden = hidden || (!visible && alias.hiddenByDefault);
    if (alias.useHidden && !parent.empty() && !SDCard.exists(parent.c_str())) SDCard.mkdir(parent.c_str());
    LOG_INF("SD", "%s -> %s", alias.visible, alias.useHidden ? alias.hidden : alias.visible);
  }
}

const char* HalStorage::aliasPath(const char* path, std::string& scratch) {
  if (!path || path[0] != '/') return path;
  for (const auto& alias : rootAliases_) {
    if (!alias.useHidden) continue;
    const size_t len = strlen(alias.visible);
    if (strncasecmp(path, alias.visible, len) != 0 || (path[len] != '\0' && path[len] != '/')) continue;
    scratch.assign(alias.hidden);
    scratch.append(path + len);
    return scratch.c_str();
  }
  return path;
}

bool HalStorage::ready() const { return SDCard.ready(); }

bool HalStorage::beginUsbDrive() {
#if FREEINK_CAP_USB_MSC && FREEINK_SD_SDMMC
  if (!usbDriveContext) {
    LOG_ERR("USB", "USB Drive context allocation failed");
    return false;
  }
  auto* const blockDevice = SDCard.detachFilesystemForRawAccess();
  if (!blockDevice) {
    LOG_ERR("USB", "USB Drive requires a mounted SDMMC filesystem");
    return false;
  }
  if (!usbDriveContext->massStorage.begin(blockDevice)) {
    LOG_ERR("USB", "USB Drive MSC initialization failed");
    if (!SDCard.begin()) {
      LOG_ERR("USB", "Unable to remount SD card after USB Drive startup failure");
    }
    return false;
  }
  return true;
#elif defined(SIMULATOR) && CROSSINK_APP_CAP_USB_DRIVE
  return true;
#else
  return false;
#endif
}

bool HalStorage::disconnectUsbDriveHost() {
#if FREEINK_CAP_USB_MSC
  return usbDriveContext && usbDriveContext->massStorage.disconnectHost();
#else
  return false;
#endif
}

bool HalStorage::usbDriveHostSuspended() const {
#if FREEINK_CAP_USB_MSC
  return usbDriveContext && usbDriveContext->massStorage.hostSuspended();
#else
  return false;
#endif
}

void HalStorage::endUsbDrive() {
#if FREEINK_CAP_USB_MSC
  if (usbDriveContext) usbDriveContext->massStorage.end();
#endif
}

UsbDriveState HalStorage::usbDriveState() const {
#if FREEINK_CAP_USB_MSC
  if (!usbDriveContext) return UsbDriveState::Unsupported;
  switch (usbDriveContext->massStorage.state()) {
    case freeink::UsbMassStorageState::WaitingForHost:
      return UsbDriveState::WaitingForHost;
    case freeink::UsbMassStorageState::Connected:
      return UsbDriveState::Connected;
    case freeink::UsbMassStorageState::Accessed:
      return UsbDriveState::Accessed;
    case freeink::UsbMassStorageState::Ejected:
      return UsbDriveState::Ejected;
    case freeink::UsbMassStorageState::Disconnected:
      return UsbDriveState::Disconnected;
    case freeink::UsbMassStorageState::IoError:
      return UsbDriveState::IoError;
    case freeink::UsbMassStorageState::Idle:
      break;
  }
#endif
  return UsbDriveState::Unsupported;
}

// For the rest of the methods, we acquire the mutex to ensure thread safety

class HalStorage::StorageLock {
 public:
  StorageLock() : spiLock() { xSemaphoreTake(HalStorage::getInstance().storageMutex, portMAX_DELAY); }
  ~StorageLock() { xSemaphoreGive(HalStorage::getInstance().storageMutex); }

 private:
  HalSpiBus::Lock spiLock;
};

#define HAL_STORAGE_WRAPPED_CALL(method, ...) \
  HalStorage::StorageLock lock;               \
  return SDCard.method(__VA_ARGS__);

void HalStorage::shutdown() {
  StorageLock lock;
  SDCard.shutdown();
}

uint64_t HalStorage::totalBytes() const { return SDCard.sdTotalBytes(); }

uint64_t HalStorage::usedBytes() { HAL_STORAGE_WRAPPED_CALL(sdUsedBytes); }

namespace {
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}
// Copies an 11-byte FAT name field, trimming trailing spaces.
void copyFatLabel(const uint8_t* src, char* out, const size_t outSize) {
  size_t n = 11;
  while (n > 0 && src[n - 1] == ' ') --n;
  n = std::min(n, outSize - 1);
  memcpy(out, src, n);
  out[n] = '\0';
}
}  // namespace

bool HalStorage::describeVolume(char* fsType, const size_t fsTypeSize, char* label, const size_t labelSize) {
  if (!fsType || fsTypeSize == 0 || !label || labelSize == 0) return false;
  fsType[0] = '\0';
  label[0] = '\0';
  StorageLock lock;
  FsBlockDeviceInterface* dev = SDCard.rawBlockDevice();
  // Sector-sized scratch, not stack: this runs on activity task stacks.
  auto sector = makeUniqueNoThrow<uint8_t[]>(512);
  if (!dev || !sector) {
    LOG_ERR("SD", "describeVolume: no block device or scratch");
    return false;
  }
  if (!dev->readSector(0, sector.get())) {
    LOG_ERR("SD", "describeVolume: sector 0 read failed");
    return false;
  }
  // A partitioned card starts with an MBR; the volume's boot sector is at the
  // first partition's start. A "superfloppy" card has the boot sector at 0.
  uint32_t vbr = 0;
  const bool isBootSector = memcmp(sector.get() + 3, "EXFAT   ", 8) == 0 || memcmp(sector.get() + 0x52, "FAT32", 5) == 0 ||
                            memcmp(sector.get() + 0x36, "FAT", 3) == 0;
  if (!isBootSector) {
    vbr = le32(sector.get() + 0x1BE + 8);
    if (vbr == 0 || !dev->readSector(vbr, sector.get())) {
      LOG_ERR("SD", "describeVolume: boot sector read failed");
      return false;
    }
  }
  const uint8_t* b = sector.get();
  if (memcmp(b + 3, "EXFAT   ", 8) == 0) {
    snprintf(fsType, fsTypeSize, "exFAT");
    // The label is a 0x83 entry in the root directory (UTF-16, up to 11 chars).
    const uint32_t heapOffset = le32(b + 0x58);
    const uint32_t rootCluster = le32(b + 0x60);
    const uint8_t clusterShift = b[0x6D];  // sectors per cluster, log2
    const uint32_t rootSector = vbr + heapOffset + ((rootCluster - 2) << clusterShift);
    if (rootCluster >= 2 && dev->readSector(rootSector, sector.get())) {
      for (int e = 0; e < 16; ++e) {
        const uint8_t* entry = sector.get() + e * 32;
        if (entry[0] == 0x00) break;
        if (entry[0] != 0x83) continue;
        const uint8_t count = std::min<uint8_t>(entry[1], 11);
        size_t w = 0;
        for (uint8_t i = 0; i < count && w + 1 < labelSize; ++i) {
          const uint16_t ch = le16(entry + 2 + i * 2);
          label[w++] = ch < 0x80 ? static_cast<char>(ch) : '?';  // ASCII labels are the norm
        }
        label[w] = '\0';
        break;
      }
    }
    return true;
  }
  const bool fat32 = memcmp(b + 0x52, "FAT32", 5) == 0;
  snprintf(fsType, fsTypeSize, fat32 ? "FAT32" : "FAT16");
  // Boot-sector label as a fallback; formatters often leave "NO NAME" there and
  // keep the real one as a volume-label entry in the root directory.
  char bootLabel[12];
  copyFatLabel(b + (fat32 ? 0x47 : 0x2B), bootLabel, sizeof(bootLabel));
  const uint32_t reserved = le16(b + 0x0E);
  const uint8_t fats = b[0x10];
  const uint32_t fatSize = fat32 ? le32(b + 0x24) : le16(b + 0x16);
  uint32_t rootSector = vbr + reserved + fats * fatSize;
  if (fat32) rootSector += (le32(b + 0x2C) - 2) * b[0x0D];
  if (dev->readSector(rootSector, sector.get())) {
    for (int e = 0; e < 16; ++e) {
      const uint8_t* entry = sector.get() + e * 32;
      if (entry[0] == 0x00) break;
      if (entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08) == 0) continue;  // deleted, LFN, not a label
      copyFatLabel(entry, label, labelSize);
      return true;
    }
  }
  if (strcmp(bootLabel, "NO NAME") != 0) snprintf(label, labelSize, "%s", bootLabel);
  return true;
}

std::vector<String> HalStorage::listFiles(const char* path, int maxFiles) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(listFiles, path, maxFiles);
}

String HalStorage::readFile(const char* path) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(readFile, path);
}

bool HalStorage::readFileToStream(const char* path, Print& out, size_t chunkSize) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(readFileToStream, path, out, chunkSize);
}

size_t HalStorage::readFileToBuffer(const char* path, char* buffer, size_t bufferSize, size_t maxBytes) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(readFileToBuffer, path, buffer, bufferSize, maxBytes);
}

bool HalStorage::writeFile(const char* path, const String& content) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(writeFile, path, content);
}

bool HalStorage::ensureDirectoryExists(const char* path) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(ensureDirectoryExists, path);
}

void HalStorage::installDateTimeCallback(const uint8_t* utcOffsetQuarterHoursBiased) {
  if (!halClock.isAvailable()) return;
  clockUtcOffsetQ = utcOffsetQuarterHoursBiased;
  FsDateTime::setCallback(storageDateTimeCallback);
  LOG_INF("SD", "Installed RTC-backed SD timestamp callback");
}

class HalFile::Impl {
 public:
  Impl(FsFile&& fsFile) : file(std::move(fsFile)) {}
  FsFile file;
};

void HalFile::ImplDeleter::operator()(Impl* const impl) const {
  if (!impl) return;
  impl->~Impl();
  std::free(impl);
}

void* HalFile::allocateImplStorage() {
  // The ESP32 build disables exceptions, so `new (std::nothrow)` can still
  // terminate through libstdc++ when the allocation cannot be satisfied.
  // malloc gives this storage wrapper the recoverable failure contract callers
  // expect while preserving FsFile's value semantics.
  return std::malloc(sizeof(Impl));
}

HalFile::HalFile() = default;

HalFile::HalFile(ImplPtr impl) : impl(std::move(impl)) {}

HalFile::~HalFile() { close(); }

HalFile::HalFile(HalFile&&) = default;

HalFile& HalFile::operator=(HalFile&& other) {
  if (this == &other) return *this;
  close();
  impl = std::move(other.impl);
  allocationFailed_ = other.allocationFailed_;
  iterationFailed_ = other.iterationFailed_;
  other.allocationFailed_ = false;
  other.iterationFailed_ = false;
  return *this;
}

HalFile HalStorage::open(const char* path, const oflag_t oflag) {
  std::string mapped;
  path = aliasPath(path, mapped);
  FsFile fsFile;
  {
    StorageLock lock;  // ensure thread safety for the duration of this function
    fsFile = SDCard.open(path, oflag);
  }
  if (!fsFile) {
    return HalFile();
  }
  void* const storage = HalFile::allocateImplStorage();
  HalFile::ImplPtr impl(storage ? ::new (storage) HalFile::Impl(std::move(fsFile)) : nullptr);
  if (!impl) {
    LOG_ERR("SD", "OOM: HalFile wrapper for %s (%u free, %u max alloc)", path, ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    StorageLock lock;
    fsFile.close();
    HalFile failed;
    failed.allocationFailed_ = true;
    return failed;
  }
  return HalFile(std::move(impl));
}

bool HalStorage::mkdir(const char* path, const bool pFlag) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(mkdir, path, pFlag);
}

bool HalStorage::exists(const char* path) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(exists, path);
}

bool HalStorage::remove(const char* path) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(remove, path);
}
bool HalStorage::rename(const char* oldPath, const char* newPath) {
  std::string mappedOld;
  std::string mappedNew;
  oldPath = aliasPath(oldPath, mappedOld);
  newPath = aliasPath(newPath, mappedNew);
  HAL_STORAGE_WRAPPED_CALL(rename, oldPath, newPath);
}

bool HalStorage::rmdir(const char* path) {
  std::string mapped;
  path = aliasPath(path, mapped);
  HAL_STORAGE_WRAPPED_CALL(rmdir, path);
}

bool HalStorage::openFileForRead(const char* moduleName, const char* path, HalFile& file) {
  std::string mapped;
  path = aliasPath(path, mapped);
  file.close();
  FsFile fsFile;
  bool ok = false;
  {
    StorageLock lock;  // ensure thread safety for the duration of this function
    ok = SDCard.openFileForRead(moduleName, path, fsFile);
  }
  if (!ok) {
    return false;
  }
  void* const storage = HalFile::allocateImplStorage();
  HalFile::ImplPtr impl(storage ? ::new (storage) HalFile::Impl(std::move(fsFile)) : nullptr);
  if (!impl) {
    LOG_ERR(moduleName, "OOM: HalFile read wrapper for %s (%u free, %u max alloc)", path, ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    StorageLock lock;
    fsFile.close();
    return false;
  }
  file = HalFile(std::move(impl));
  return true;
}

bool HalStorage::openFileForRead(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForRead(const char* moduleName, const String& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const char* path, HalFile& file) {
  std::string mapped;
  path = aliasPath(path, mapped);
  file.close();
  FsFile fsFile;
  bool ok = false;
  {
    StorageLock lock;  // ensure thread safety for the duration of this function
    ok = SDCard.openFileForWrite(moduleName, path, fsFile);
  }
  if (!ok) {
    return false;
  }
  void* const storage = HalFile::allocateImplStorage();
  HalFile::ImplPtr impl(storage ? ::new (storage) HalFile::Impl(std::move(fsFile)) : nullptr);
  if (!impl) {
    LOG_ERR(moduleName, "OOM: HalFile write wrapper for %s (%u free, %u max alloc)", path, ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    StorageLock lock;
    fsFile.close();
    return false;
  }
  file = HalFile(std::move(impl));
  return true;
}

bool HalStorage::openFileForWrite(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const String& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::removeDir(const char* path) {
  if (!path || path[0] == '\0') {
    return false;
  }

  HalFile dir = open(path);
  if (!dir || !dir.isDirectory()) {
    dir.close();
    return false;
  }

  char name[128];
  for (HalFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    const bool isDirectory = entry.isDirectory();
    const size_t nameLen = entry.getName(name, sizeof(name));

    // SdFat cannot reopen or delete this path while its directory entry is open.
    entry.close();
    if (nameLen == 0) {
      dir.close();
      return false;
    }

    std::string entryPath(path);
    if (entryPath.back() != '/') {
      entryPath += '/';
    }
    entryPath += name;

    const bool removed = isDirectory ? removeDir(entryPath.c_str()) : remove(entryPath.c_str());
    if (!removed) {
      dir.close();
      return false;
    }
  }

  dir.close();
  return rmdir(path);
}

// HalFile implementation
// Allow doing file operations while ensuring thread safety via HalStorage's mutex.
// Please keep the list below in sync with the HalFile.h header

#define HAL_FILE_WRAPPED_CALL(method, ...) \
  HalStorage::StorageLock lock;            \
  assert(impl != nullptr);                 \
  return impl->file.method(__VA_ARGS__);

#define HAL_FILE_FORWARD_CALL(method, ...) \
  assert(impl != nullptr);                 \
  return impl->file.method(__VA_ARGS__);

void HalFile::flush() { HAL_FILE_WRAPPED_CALL(flush, ); }
size_t HalFile::getName(char* name, size_t len) { HAL_FILE_WRAPPED_CALL(getName, name, len); }
size_t HalFile::size() { HAL_FILE_FORWARD_CALL(size, ); }              // already thread-safe, no need to wrap
size_t HalFile::fileSize() { HAL_FILE_FORWARD_CALL(fileSize, ); }      // already thread-safe, no need to wrap
uint64_t HalFile::fileSize64() { HAL_FILE_FORWARD_CALL(fileSize, ); }  // already thread-safe, no need to wrap
bool HalFile::seek(size_t pos) { HAL_FILE_WRAPPED_CALL(seekSet, pos); }
bool HalFile::seek64(uint64_t pos) { HAL_FILE_WRAPPED_CALL(seekSet, pos); }
bool HalFile::seekCur(int64_t offset) { HAL_FILE_WRAPPED_CALL(seekCur, offset); }
bool HalFile::seekSet(size_t offset) { HAL_FILE_WRAPPED_CALL(seekSet, offset); }
int HalFile::available() const { HAL_FILE_WRAPPED_CALL(available, ); }
size_t HalFile::position() const { HAL_FILE_WRAPPED_CALL(position, ); }
int HalFile::read(void* buf, size_t count) { HAL_FILE_WRAPPED_CALL(read, buf, count); }
int HalFile::read() { HAL_FILE_WRAPPED_CALL(read, ); }
size_t HalFile::write(const void* buf, size_t count) { HAL_FILE_WRAPPED_CALL(write, buf, count); }
size_t HalFile::write(uint8_t b) { HAL_FILE_WRAPPED_CALL(write, b); }
bool HalFile::sync() { HAL_FILE_WRAPPED_CALL(sync, ); }
bool HalFile::rename(const char* newPath) { HAL_FILE_WRAPPED_CALL(rename, newPath); }
bool HalFile::isDirectory() const { HAL_FILE_FORWARD_CALL(isDirectory, ); }  // already thread-safe, no need to wrap
void HalFile::rewindDirectory() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
  impl->file.rewindDirectory();
  allocationFailed_ = false;
  // SdFat's read-error bits are sticky for the lifetime of the handle and
  // FsFile does not expose clearError(). Reopen the directory to retry after
  // an iteration failure rather than making rewind appear to clear it.
}
bool HalFile::close() {
  if (!impl) return true;
  HalStorage::StorageLock lock;
  const bool ok = impl->file.close();
  impl.reset();
  allocationFailed_ = false;
  iterationFailed_ = false;
  return ok;
}
HalFile HalFile::openNextFile() {
  allocationFailed_ = false;
  iterationFailed_ = false;
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
  auto fsFile = impl->file.openNextFile();
  if (!fsFile) {
    const uint8_t error = impl->file.getError();
    if (error != 0) {
      iterationFailed_ = true;
      LOG_ERR("SD", "Directory iteration failed (SdFat error 0x%02x)", error);
    }
    return HalFile();
  }
  void* const storage = allocateImplStorage();
  ImplPtr childImpl(storage ? ::new (storage) Impl(std::move(fsFile)) : nullptr);
  if (!childImpl) {
    allocationFailed_ = true;
    LOG_ERR("SD", "OOM: HalFile directory entry wrapper (%u free, %u max alloc)", ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    fsFile.close();
    return HalFile();
  }
  return HalFile(std::move(childImpl));
}
bool HalFile::isOpen() const { return impl != nullptr && impl->file.isOpen(); }  // already thread-safe, no need to wrap
HalFile::operator bool() const { return isOpen(); }
