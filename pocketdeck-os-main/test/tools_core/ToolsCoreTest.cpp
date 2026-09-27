#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "SrsSchedule.h"
#include "ToolsDate.h"
#include "ToolsStore.h"

namespace {

int64_t utcMinutes(int y, unsigned m, unsigned d, int hh, int mm) {
  return static_cast<int64_t>(tools::daysFromCivil(y, m, d)) * 1440 + hh * 60 + mm;
}

class ToolsStoreTest : public ::testing::Test {
 protected:
  void SetUp() override { Storage.reset(); }
};

}  // namespace

// ---------------------------------------------------------------------------
// Calendar arithmetic
// ---------------------------------------------------------------------------

TEST(ToolsDate, EpochAndKnownDates) {
  EXPECT_EQ(tools::daysFromCivil(1970, 1, 1), 0);
  EXPECT_EQ(tools::daysFromCivil(2000, 3, 1), 11017);
  EXPECT_EQ(tools::weekdayMon0(tools::daysFromCivil(2026, 9, 27)), 6);  // Sunday
  EXPECT_EQ(tools::weekdayMon0(tools::daysFromCivil(2024, 2, 29)), 3);  // Thursday
}

TEST(ToolsDate, CivilRoundTripEveryDayTo2099) {
  const int32_t last = tools::daysFromCivil(2099, 12, 31);
  for (int32_t day = 0; day <= last; ++day) {
    uint16_t y = 0;
    uint8_t m = 0, d = 0;
    tools::civilFromDays(day, y, m, d);
    ASSERT_EQ(tools::daysFromCivil(y, m, d), day) << y << "-" << int(m) << "-" << int(d);
  }
}

TEST(ToolsDate, IsoWeeksAcrossYearBoundaries) {
  uint16_t year = 0;
  uint8_t week = 0;
  tools::isoWeek(tools::daysFromCivil(2026, 9, 27), year, week);
  EXPECT_EQ(year, 2026);
  EXPECT_EQ(week, 39);
  tools::isoWeek(tools::daysFromCivil(2021, 1, 3), year, week);  // belongs to 2020-W53
  EXPECT_EQ(year, 2020);
  EXPECT_EQ(week, 53);
  tools::isoWeek(tools::daysFromCivil(2024, 12, 30), year, week);  // belongs to 2025-W01
  EXPECT_EQ(year, 2025);
  EXPECT_EQ(week, 1);
}

TEST(ToolsDate, IsoDateFormatAndStrictParse) {
  char buf[11];
  tools::formatIsoDate(buf, sizeof(buf), tools::daysFromCivil(2027, 1, 5));
  EXPECT_STREQ(buf, "2027-01-05");
  int32_t days = 0;
  EXPECT_TRUE(tools::parseIsoDate("2026-09-27", days));
  EXPECT_EQ(days, tools::daysFromCivil(2026, 9, 27));
  EXPECT_TRUE(tools::parseIsoDate("2026-09-27|quote", days));  // prefix of a quotes.txt line
  EXPECT_FALSE(tools::parseIsoDate("2026-9-27", days));
  EXPECT_FALSE(tools::parseIsoDate("2026-13-01", days));
  EXPECT_FALSE(tools::parseIsoDate("abcd-ef-gh", days));
  EXPECT_FALSE(tools::parseIsoDate(nullptr, days));
}

// ---------------------------------------------------------------------------
// World Clock daylight-saving rules (2026 transition instants)
// ---------------------------------------------------------------------------

TEST(ToolsDst, UnitedStates) {
  using tools::DstRule;
  // Starts 2026-03-08 02:00 EST = 07:00 UTC; ends 2026-11-01 02:00 EDT = 06:00 UTC.
  EXPECT_EQ(tools::effectiveUtcOffset(-300, DstRule::US, utcMinutes(2026, 3, 8, 6, 59)), -300);
  EXPECT_EQ(tools::effectiveUtcOffset(-300, DstRule::US, utcMinutes(2026, 3, 8, 7, 0)), -240);
  EXPECT_EQ(tools::effectiveUtcOffset(-300, DstRule::US, utcMinutes(2026, 11, 1, 5, 59)), -240);
  EXPECT_EQ(tools::effectiveUtcOffset(-300, DstRule::US, utcMinutes(2026, 11, 1, 6, 0)), -300);
}

TEST(ToolsDst, Europe) {
  using tools::DstRule;
  // 2026-03-29 and 2026-10-25, both at 01:00 UTC.
  EXPECT_EQ(tools::effectiveUtcOffset(0, DstRule::EU, utcMinutes(2026, 3, 29, 0, 59)), 0);
  EXPECT_EQ(tools::effectiveUtcOffset(0, DstRule::EU, utcMinutes(2026, 3, 29, 1, 0)), 60);
  EXPECT_EQ(tools::effectiveUtcOffset(60, DstRule::EU, utcMinutes(2026, 10, 25, 0, 59)), 120);
  EXPECT_EQ(tools::effectiveUtcOffset(60, DstRule::EU, utcMinutes(2026, 10, 25, 1, 0)), 60);
}

TEST(ToolsDst, AustraliaSouthernHemisphere) {
  using tools::DstRule;
  // Ends 2026-04-05 03:00 AEDT = 04-04 16:00 UTC; starts 2026-10-04 02:00 AEST = 10-03 16:00 UTC.
  EXPECT_EQ(tools::effectiveUtcOffset(600, DstRule::AU, utcMinutes(2026, 4, 4, 15, 59)), 660);
  EXPECT_EQ(tools::effectiveUtcOffset(600, DstRule::AU, utcMinutes(2026, 4, 4, 16, 0)), 600);
  EXPECT_EQ(tools::effectiveUtcOffset(600, DstRule::AU, utcMinutes(2026, 10, 3, 15, 59)), 600);
  EXPECT_EQ(tools::effectiveUtcOffset(600, DstRule::AU, utcMinutes(2026, 10, 3, 16, 0)), 660);
  EXPECT_EQ(tools::effectiveUtcOffset(600, DstRule::AU, utcMinutes(2026, 12, 31, 20, 0)), 660);
}

TEST(ToolsDst, NewZealand) {
  using tools::DstRule;
  // Starts 2026-09-27 02:00 NZST = 09-26 14:00 UTC.
  EXPECT_EQ(tools::effectiveUtcOffset(720, DstRule::NZ, utcMinutes(2026, 9, 26, 13, 59)), 720);
  EXPECT_EQ(tools::effectiveUtcOffset(720, DstRule::NZ, utcMinutes(2026, 9, 26, 14, 0)), 780);
}

TEST(ToolsDst, NoRuleKeepsStandardOffset) {
  EXPECT_EQ(tools::effectiveUtcOffset(330, tools::DstRule::None, utcMinutes(2026, 7, 1, 0, 0)), 330);
}

// ---------------------------------------------------------------------------
// Spaced repetition
// ---------------------------------------------------------------------------

TEST(Srs, IntervalDoublesAndCaps) {
  uint16_t interval = 0;
  const uint16_t expected[] = {1, 2, 4, 8, 16, 32, 64, 128, 180, 180};
  for (const uint16_t want : expected) {
    interval = srs::nextInterval(interval, true);
    EXPECT_EQ(interval, want);
  }
  EXPECT_EQ(srs::nextInterval(64, false), 0);
}

TEST(Srs, DueRules) {
  EXPECT_TRUE(srs::isDue(100, false, 0, 0));   // never reviewed
  EXPECT_TRUE(srs::isDue(100, true, 100, 0));  // forgot today
  EXPECT_FALSE(srs::isDue(100, true, 99, 2));
  EXPECT_TRUE(srs::isDue(101, true, 99, 2));
}

TEST(Srs, QuestionHashIsStableFnv1a) {
  EXPECT_EQ(srs::hashQuestion(""), 2166136261u);
  EXPECT_EQ(srs::hashQuestion("a"), 0xe40c292cu);
  EXPECT_NE(srs::hashQuestion("What is a list?"), srs::hashQuestion("What is a tuple?"));
}

// ---------------------------------------------------------------------------
// Line reader
// ---------------------------------------------------------------------------

TEST_F(ToolsStoreTest, ReadLineHandlesCrlfTruncationAndEof) {
  Storage.put("/f.txt", "first\r\n0123456789abcdef\nlast");
  FsFile f = Storage.open("/f.txt");
  char buf[8];
  EXPECT_EQ(tools::readLine(f, buf, sizeof(buf)), 5);
  EXPECT_STREQ(buf, "first");
  EXPECT_EQ(tools::readLine(f, buf, sizeof(buf)), 7);  // truncated, rest of line consumed
  EXPECT_STREQ(buf, "0123456");
  EXPECT_EQ(tools::readLine(f, buf, sizeof(buf)), 4);
  EXPECT_STREQ(buf, "last");
  EXPECT_EQ(tools::readLine(f, buf, sizeof(buf)), -1);
}

TEST_F(ToolsStoreTest, ReadLineNeverSplitsUtf8) {
  Storage.put("/f.txt", "ab\xE2\x80\x94xyz\n");  // em dash straddles the cap
  FsFile f = Storage.open("/f.txt");
  char buf[5];
  EXPECT_EQ(tools::readLine(f, buf, sizeof(buf)), 2);
  EXPECT_STREQ(buf, "ab");
}

// ---------------------------------------------------------------------------
// Streaming JSON reader
// ---------------------------------------------------------------------------

TEST_F(ToolsStoreTest, JsonReaderTokensEscapesAndUnicode) {
  Storage.put("/j.json", R"({"text": "say \"hi\"\né 😀", "done": true, "n": -12})");
  FsFile f = Storage.open("/j.json");
  tools::JsonReader r(f);
  using T = tools::JsonReader::Token;
  char buf[64];
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::ObjectStart);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::String);
  EXPECT_STREQ(buf, "text");
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Colon);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::String);
  EXPECT_STREQ(buf, "say \"hi\"\n\xC3\xA9 \xF0\x9F\x98\x80");
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Comma);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::String);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Colon);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Literal);
  EXPECT_STREQ(buf, "true");
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Comma);
  r.next(buf, sizeof(buf));
  r.next(buf, sizeof(buf));
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::Literal);
  EXPECT_STREQ(buf, "-12");
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::ObjectEnd);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::End);
}

TEST_F(ToolsStoreTest, JsonReaderTruncatesWithoutSplittingCodePoints) {
  Storage.put("/j.json", "\"abc\xC3\xA9\xC3\xA9\"");
  FsFile f = Storage.open("/j.json");
  tools::JsonReader r(f);
  char buf[6];  // room for "abc" + one 2-byte char, not two
  EXPECT_EQ(r.next(buf, sizeof(buf)), tools::JsonReader::Token::String);
  EXPECT_STREQ(buf, "abc\xC3\xA9");
}

TEST_F(ToolsStoreTest, JsonReaderOffsetsSeekAndSkip) {
  // Offsets recorded while scanning must let a later seek re-read one card,
  // which is how flashcard decks are indexed.
  const std::string deck = R"({"name": "deck", "meta": {"a": [1, {"b": 2}]}, "cards": [)"
                           R"({"question": "Q1", "answer": "A1"}, {"question": "Q2", "answer": "A2"}]})";
  Storage.put("/d.json", deck);
  FsFile f = Storage.open("/d.json");
  tools::JsonReader r(f);
  using T = tools::JsonReader::Token;
  ASSERT_TRUE(tools::findFirstArray(r));  // skips nested "meta" array too
  // findFirstArray stops at the first '[' (inside "meta"); skip to "cards".
  char buf[32];
  ASSERT_TRUE(r.skipValue(T::ArrayStart));
  ASSERT_TRUE(tools::findFirstArray(r));
  ASSERT_EQ(r.next(buf, sizeof(buf)), T::ObjectStart);
  const uint32_t first = r.tokenOffset();
  EXPECT_EQ(deck[first], '{');
  ASSERT_TRUE(r.skipValue(T::ObjectStart));
  ASSERT_EQ(r.next(buf, sizeof(buf)), T::Comma);
  ASSERT_EQ(r.next(buf, sizeof(buf)), T::ObjectStart);
  const uint32_t second = r.tokenOffset();

  ASSERT_TRUE(r.seek(second));
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::ObjectStart);
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::String);
  EXPECT_STREQ(buf, "question");
  r.next(buf, sizeof(buf));
  EXPECT_EQ(r.next(buf, sizeof(buf)), T::String);
  EXPECT_STREQ(buf, "Q2");
  ASSERT_TRUE(r.seek(first));
  r.next(buf, sizeof(buf));
  r.next(buf, sizeof(buf));
  r.next(buf, sizeof(buf));
  r.next(buf, sizeof(buf));
  EXPECT_STREQ(buf, "Q1");
}

TEST_F(ToolsStoreTest, JsonWriterEscapesRoundTrip) {
  struct Ctx {
    const char* text;
  } ctx{"quote \"x\" \\ tab\tnew\nline"};
  ASSERT_TRUE(tools::writeFileAtomic(
      "/tools/s.json", [](FsFile& out, void* c) { return tools::writeJsonString(out, static_cast<Ctx*>(c)->text); },
      &ctx));
  FsFile f = Storage.open("/tools/s.json");
  tools::JsonReader r(f);
  char buf[64];
  ASSERT_EQ(r.next(buf, sizeof(buf)), tools::JsonReader::Token::String);
  EXPECT_STREQ(buf, "quote \"x\" \\ tab\tnew\nline");
}

// ---------------------------------------------------------------------------
// Atomic writes
// ---------------------------------------------------------------------------

namespace {
bool writeHello(FsFile& out, void*) { return tools::writeText(out, "hello"); }
bool writeWorld(FsFile& out, void*) { return tools::writeText(out, "world"); }
bool writeFails(FsFile& out, void*) {
  tools::writeText(out, "partial");
  return false;
}
}  // namespace

TEST_F(ToolsStoreTest, AtomicWriteCreatesAndReplaces) {
  ASSERT_TRUE(tools::writeFileAtomic("/tools/a.txt", &writeHello, nullptr));
  EXPECT_EQ(Storage.get("/tools/a.txt"), "hello");
  ASSERT_TRUE(tools::writeFileAtomic("/tools/a.txt", &writeWorld, nullptr));
  EXPECT_EQ(Storage.get("/tools/a.txt"), "world");
  EXPECT_FALSE(Storage.exists("/tools/a.txt.tmp"));
  EXPECT_FALSE(Storage.exists("/tools/a.txt.bak"));
}

TEST_F(ToolsStoreTest, FailedWriterLeavesOriginalUntouched) {
  Storage.put("/tools/a.txt", "original");
  EXPECT_FALSE(tools::writeFileAtomic("/tools/a.txt", &writeFails, nullptr));
  EXPECT_EQ(Storage.get("/tools/a.txt"), "original");
  EXPECT_FALSE(Storage.exists("/tools/a.txt.tmp"));
}

TEST_F(ToolsStoreTest, FailedFinalRenameRestoresOriginal) {
  Storage.put("/tools/a.txt", "original");
  Storage.failNextRenameFrom("/tools/a.txt.tmp");
  EXPECT_FALSE(tools::writeFileAtomic("/tools/a.txt", &writeWorld, nullptr));
  EXPECT_EQ(Storage.get("/tools/a.txt"), "original");
}

TEST_F(ToolsStoreTest, BackupIsRecoveredAfterInterruptedSwap) {
  // Power lost after the original was moved aside but before tmp was renamed.
  Storage.put("/tools/a.txt.bak", "saved");
  tools::recoverFromBackup("/tools/a.txt");
  EXPECT_EQ(Storage.get("/tools/a.txt"), "saved");
  EXPECT_FALSE(Storage.exists("/tools/a.txt.bak"));
}

// ---------------------------------------------------------------------------
// Panchanga (Bengaluru, UTC+5:30) against Drik Panchang
// ---------------------------------------------------------------------------

#include "PanchangaMath.h"

namespace {
constexpr double kLat = 12.9716, kLon = 77.5946, kTz = 5.5;

// Local clock minutes since midnight for a JD.
double localMinutes(const double jd) { return panchanga::localHours(jd, kTz) * 60.0; }

bool hasSpecial(const panchanga::Day& d, const panchanga::Special s) {
  panchanga::Special out[2];
  const int n = panchanga::specials(d, out, 2);
  for (int i = 0; i < n; ++i)
    if (out[i] == s) return true;
  return false;
}
}  // namespace

TEST(Panchanga, AyanamshaMatchesDrik) {
  EXPECT_NEAR(panchanga::ayanamsha(2459484.5), 24.167476, 0.00001);  // 2021-09-27
  EXPECT_NEAR(panchanga::ayanamsha(2461310.5), 24.237323, 0.00001);  // 2026-09-27
}

TEST(Panchanga, September27_2026) {
  const auto d = panchanga::compute(2026, 9, 27, kTz, kLat, kLon);
  ASSERT_TRUE(d.valid);
  EXPECT_EQ(d.vara, 0);                                          // Sunday
  EXPECT_EQ(d.tithi, 15);                                        // Krishna Pratipada
  EXPECT_NEAR(localMinutes(d.tithiEndJd), 20 * 60 + 58.5, 1.0);  // Drik: upto 20:58
  EXPECT_EQ(d.nakshatra, 25);                                    // Uttara Bhadrapada
  EXPECT_NEAR(localMinutes(d.nakshatraEndJd), 11 * 60 + 8.5, 1.0);
  EXPECT_EQ(d.yoga, 10);                                             // Vriddhi
  EXPECT_EQ(d.karana, 1);                                            // Balava
  EXPECT_EQ(d.masa, 5);                                              // Bhadrapada (amanta)
  EXPECT_EQ(d.samvatsara, 39);                                       // Parabhava
  EXPECT_NEAR(localMinutes(d.sunriseJd), 6 * 60 + 9, 1.0);           // 06:09
  EXPECT_NEAR(localMinutes(d.sunsetJd), 18 * 60 + 12, 1.0);          // 18:12
  EXPECT_NEAR(localMinutes(d.rahuStartJd), 16 * 60 + 42, 1.5);       // 16:42
  EXPECT_NEAR(localMinutes(d.yamagandaStartJd), 12 * 60 + 11, 1.5);  // 12:11
  EXPECT_NEAR(localMinutes(d.gulikaStartJd), 15 * 60 + 11, 1.5);     // 15:11
  EXPECT_NEAR(localMinutes(d.abhijitStartJd), 11 * 60 + 46, 1.5);    // 11:46
}

TEST(Panchanga, September27_2021) {
  const auto d = panchanga::compute(2021, 9, 27, kTz, kLat, kLon);
  EXPECT_EQ(d.tithi, 20);                                        // Krishna Shashthi
  EXPECT_NEAR(localMinutes(d.tithiEndJd), 15 * 60 + 43.5, 1.0);  // upto 15:43
  EXPECT_EQ(d.nakshatra, 3);                                     // Rohini
  EXPECT_NEAR(localMinutes(d.nakshatraEndJd), 17 * 60 + 42.5, 1.0);
  EXPECT_EQ(d.yoga, 15);                                       // Siddhi
  EXPECT_NEAR(localMinutes(d.rahuStartJd), 7 * 60 + 39, 1.5);  // Monday Rahu 07:39
}

TEST(Panchanga, FestivalsFollowDeciderTimes) {
  using panchanga::Special;
  EXPECT_TRUE(hasSpecial(panchanga::compute(2026, 3, 19, kTz, kLat, kLon), Special::Ugadi));  // kshaya pratipada
  EXPECT_TRUE(hasSpecial(panchanga::compute(2024, 1, 15, kTz, kLat, kLon), Special::MakaraSankranti));
  EXPECT_FALSE(hasSpecial(panchanga::compute(2024, 1, 14, kTz, kLat, kLon), Special::MakaraSankranti));
  EXPECT_TRUE(hasSpecial(panchanga::compute(2024, 10, 12, kTz, kLat, kLon), Special::Vijayadashami));
  EXPECT_TRUE(hasSpecial(panchanga::compute(2025, 8, 27, kTz, kLat, kLon), Special::GaneshaChaturthi));
  const auto diwali = panchanga::compute(2026, 11, 8, kTz, kLat, kLon);
  EXPECT_TRUE(hasSpecial(diwali, Special::NarakaChaturdashi));
  EXPECT_TRUE(hasSpecial(diwali, Special::Deepavali));
}

TEST(Panchanga, SamvatsaraAndAdhikaMasa) {
  EXPECT_EQ(panchanga::compute(2024, 6, 1, kTz, kLat, kLon).samvatsara, 37);  // Krodhi
  EXPECT_EQ(panchanga::compute(2027, 5, 1, kTz, kLat, kLon).samvatsara, 40);  // Plavanga
  // 2026 has an adhika Jyeshtha (about 17 May - 15 June).
  const auto adhika = panchanga::compute(2026, 6, 1, kTz, kLat, kLon);
  EXPECT_TRUE(adhika.adhika);
  EXPECT_EQ(adhika.masa, 2);
}
