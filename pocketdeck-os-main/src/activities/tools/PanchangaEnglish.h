#pragma once

// English (IAST-free, Drik-style transliteration) names for the Panchanga.
// Every table has the same order and length as its Kannada counterpart in
// PanchangaStrings.h, so the activity can index either with the same value.
// These are Latin strings drawn with the normal UI fonts; they live in flash.
namespace en {

inline constexpr const char* kWeekday[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                           "Thursday", "Friday", "Saturday"};

inline constexpr const char* kGregorianMonth[] = {"January",   "February", "March",    "April",
                                                  "May",       "June",     "July",     "August",
                                                  "September", "October",  "November", "December"};

inline constexpr const char* kMasa[] = {"Chaitra",   "Vaishakha", "Jyeshtha",   "Ashadha", "Shravana", "Bhadrapada",
                                        "Ashwayuja", "Kartika",   "Margashira", "Pushya",  "Magha",    "Phalguna"};

inline constexpr const char* kSamvatsara[] = {
    "Prabhava",     "Vibhava",   "Shukla",   "Pramoduta",  "Prajotpatti", "Angirasa",   "Shrimukha",  "Bhava",
    "Yuva",         "Dhatu",     "Ishvara",  "Bahudhanya", "Pramathi",    "Vikrama",    "Vrisha",     "Chitrabhanu",
    "Svabhanu",     "Tarana",    "Parthiva", "Vyaya",      "Sarvajit",    "Sarvadhari", "Virodhi",    "Vikriti",
    "Khara",        "Nandana",   "Vijaya",   "Jaya",       "Manmatha",    "Durmukhi",   "Hevilambi",  "Vilambi",
    "Vikari",       "Sharvari",  "Plava",    "Shubhakrit", "Shobhakrit",  "Krodhi",     "Vishvavasu", "Parabhava",
    "Plavanga",     "Kilaka",    "Saumya",   "Sadharana",  "Virodhikrit", "Paridhavi",  "Pramadicha", "Ananda",
    "Rakshasa",     "Nala",      "Pingala",  "Kalayukti",  "Siddharthi",  "Raudri",     "Durmati",    "Dundubhi",
    "Rudhirodgari", "Raktakshi", "Krodhana", "Akshaya"};

// 0..13 = pratipada..chaturdashi, 14 = purnima, 15 = amavasya
inline constexpr const char* kTithi[] = {"Pratipada",  "Dwitiya",     "Tritiya", "Chaturthi", "Panchami", "Shashthi",
                                         "Saptami",    "Ashtami",     "Navami",  "Dashami",   "Ekadashi", "Dwadashi",
                                         "Trayodashi", "Chaturdashi", "Purnima", "Amavasya"};

inline constexpr const char* kPaksha[] = {"Shukla Paksha", "Krishna Paksha"};
inline constexpr const char* kPakshaShort[] = {"Shukla", "Krishna"};

inline constexpr const char* kNakshatra[] = {"Ashwini",
                                             "Bharani",
                                             "Krittika",
                                             "Rohini",
                                             "Mrigashira",
                                             "Ardra",
                                             "Punarvasu",
                                             "Pushya",
                                             "Ashlesha",
                                             "Magha",
                                             "Purva Phalguni",
                                             "Uttara Phalguni",
                                             "Hasta",
                                             "Chitra",
                                             "Swati",
                                             "Vishakha",
                                             "Anuradha",
                                             "Jyeshtha",
                                             "Mula",
                                             "Purva Ashadha",
                                             "Uttara Ashadha",
                                             "Shravana",
                                             "Dhanishta",
                                             "Shatabhisha",
                                             "Purva Bhadrapada",
                                             "Uttara Bhadrapada",
                                             "Revati"};

inline constexpr const char* kYoga[] = {
    "Vishkambha", "Priti",   "Ayushman", "Saubhagya", "Shobhana", "Atiganda", "Sukarma", "Dhriti",    "Shula",
    "Ganda",      "Vriddhi", "Dhruva",   "Vyaghata",  "Harshana", "Vajra",    "Siddhi",  "Vyatipata", "Variyana",
    "Parigha",    "Shiva",   "Siddha",   "Sadhya",    "Shubha",   "Shukla",   "Brahma",  "Indra",     "Vaidhriti"};

inline constexpr const char* kKarana[] = {"Bava",   "Balava",  "Kaulava",     "Taitila", "Garaja",    "Vanija",
                                          "Vishti", "Shakuni", "Chatushpada", "Naga",    "Kimstughna"};

// Order matches panchanga::Special (index 0 = None is unused).
inline constexpr const char* kSpecial[] = {"",
                                           "Ugadi",
                                           "Sri Rama Navami",
                                           "Akshaya Tritiya",
                                           "Naga Panchami",
                                           "Krishna Janmashtami",
                                           "Ganesha Chaturthi",
                                           "Mahalaya Amavasya",
                                           "Navaratri begins",
                                           "Vijayadashami",
                                           "Naraka Chaturdashi",
                                           "Deepavali",
                                           "Bali Padyami",
                                           "Maha Shivaratri",
                                           "Holi Purnima",
                                           "Makara Sankranti",
                                           "Sankramana",
                                           "Ekadashi",
                                           "Purnima",
                                           "Amavasya",
                                           "Sankashti Chaturthi"};

// Order matches kn::Label.
inline constexpr const char* kLabel[] = {"Samvatsara", "Masa",          "Tithi",          "Nakshatra", "Yoga",
                                         "Karana",     "Vara",          "Sunrise",        "Sunset",    "Rahu Kalam",
                                         "Yamaganda",  "Gulika Kalam",  "Abhijit",        "Special",   "Moon",
                                         "Lit",        "Moon star",     "upto",           "Today",     "Bengaluru",
                                         "Adhika",     "Clock not set", "No special day", "next day",  "whole night"};

inline constexpr const char* kTitle = "Panchanga";

}  // namespace en
