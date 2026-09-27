#pragma once
#include <string>
#include <utility>

#include "activities/Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool canSnapshotOverlayBackground,
                         std::string currentBookPath = {}, bool fromTimeout = false,
                         GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait)
      : Activity("Sleep", renderer, mappedInput),
        canSnapshotOverlayBackground(canSnapshotOverlayBackground),
        currentBookPath(std::move(currentBookPath)),
        fromTimeout(fromTimeout),
        sleepPopupOrientation(sleepPopupOrientation) {}
  void onEnter() override;

  // PocketDeck-OS rotating wallpapers: true while a timer wake is only
  // swapping the wallpaper (no "Entering sleep" popup is drawn).
  static void setRotationWake(bool on) { rotationWake = on; }
  // True when Sleep > Change wallpaper is on and the sleep screen in effect
  // shows /sleep images (Custom, or Cover + Custom outside a book).
  static bool wallpaperRotationActive(bool fromReader);
  // Converts up to `maxCount` .jpg/.jpeg wallpapers in the sleep folder to
  // screen-sized .bmp copies ("photo.jpg" -> "photo.jpg.bmp"). Returns how
  // many were converted.
  static int convertPendingJpegWallpapers(int maxCount);

 private:
  static bool rotationWake;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderReadingStatsSleepScreen() const;
  void renderMinimalSleepScreen() const;
  void renderMinimalStatsSleepScreen() const;
  void renderDashboardSleepScreen() const;
  bool renderBitmapSleepScreen(Bitmap& bitmap) const;
  void renderLastScreenSleepScreen() const;
  static void drawBrandedSleepCard(const GfxRenderer& renderer, int pageWidth, int pageHeight);
  void renderBlankSleepScreen() const;
  void renderOverlaySleepScreen() const;
  bool canSnapshotOverlayBackground = false;
  bool overlayBackgroundBufferStored = false;
  std::string currentBookPath;
  bool fromTimeout = false;
  GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait;
};
