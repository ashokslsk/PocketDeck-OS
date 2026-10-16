#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Offline Hindu calendar (panchanga) arithmetic, header-only so the native test
// suite can validate it. All angles are degrees, times are Julian Days (UT).
//
//   Sun:  VSOP87 (Meeus ch. 32 truncation) + FK5, aberration, nutation (~1 arcsec)
//   Moon: Meeus ch. 47, full 60-term ELP-2000/82 longitude series (~10 arcsec)
//   Ayanamsha: Lahiri (Chitrapaksha), calibrated to Drik Panchang
//   Sunrise/sunset: Sun's centre at -0.833 deg (refraction + semi-diameter)
//
// Values follow the Karnataka (amanta, Shalivahana) convention: the day's
// tithi/nakshatra/yoga/karana are those current at local sunrise.
namespace panchanga {

constexpr double kPi = 3.14159265358979323846;
inline double rad(const double d) { return d * kPi / 180.0; }
inline double deg(const double r) { return r * 180.0 / kPi; }
inline double norm360(double a) {
  a = std::fmod(a, 360.0);
  return a < 0 ? a + 360.0 : a;
}

// Julian Day at 0h UT of a Gregorian date, plus fractional hours.
inline double julianDay(int y, int m, const int d, const double hoursUt = 0.0) {
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  const int a = y / 100;
  const int b = 2 - a + a / 4;
  return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + b - 1524.5 + hoursUt / 24.0;
}

inline double centuries(const double jd) { return (jd - 2451545.0) / 36525.0; }

// Nutation in longitude, short series (deg). Applied to both bodies so it
// cancels in the Moon-Sun elongation.
inline double nutationLongitude(const double t) {
  const double omega = rad(125.04452 - 1934.136261 * t);
  const double l = rad(280.4665 + 36000.7698 * t);
  const double lp = rad(218.3165 + 481267.8813 * t);
  return (-17.20 * std::sin(omega) - 1.32 * std::sin(2 * l) - 0.23 * std::sin(2 * lp) + 0.21 * std::sin(2 * omega)) /
         3600.0;
}

struct VsopTerm {
  double a, b, c;  // A * cos(B + C * tau), A in 1e-8 rad (L) or 1e-8 AU (R)
};

// VSOP87 Earth series truncated as in Meeus Appendix III (about 1 arcsecond).
inline constexpr VsopTerm kL0[] = {{175347045.673, 0.0, 0.0},
                                   {3341656.456, 4.66925680417, 6283.0758499914},
                                   {34894.275, 4.62610241759, 12566.1516999828},
                                   {3497.056, 2.74411800971, 5753.3848848968},
                                   {3417.571, 2.82886579606, 3.523118349},
                                   {3135.896, 3.62767041758, 77713.7714681205},
                                   {2676.218, 4.41808351397, 7860.4193924392},
                                   {2342.687, 6.13516237631, 3930.2096962196},
                                   {1324.292, 0.74246356352, 11506.7697697936},
                                   {1273.166, 2.03709655772, 529.6909650946},
                                   {1199.167, 1.10962944315, 1577.3435424478},
                                   {990.25, 5.23268129594, 5884.9268465832},
                                   {901.855, 2.04505443513, 26.2983197998},
                                   {857.223, 3.50849156957, 398.1490034082},
                                   {779.786, 1.17882652114, 5223.6939198022},
                                   {753.141, 2.53339053818, 5507.5532386674},
                                   {505.264, 4.58292563052, 18849.2275499742},
                                   {492.379, 4.20506639861, 775.522611324},
                                   {356.655, 2.91954116867, 0.0673103028},
                                   {317.087, 5.84901952218, 11790.6290886588},
                                   {284.125, 1.89869034186, 796.2980068164},
                                   {271.039, 0.31488607649, 10977.078804699},
                                   {242.81, 0.34481140906, 5486.777843175},
                                   {206.16, 4.80646606059, 2544.3144198834},
                                   {205.385, 1.86947813692, 5573.1428014331},
                                   {202.261, 2.45767795458, 6069.7767545534},
                                   {155.516, 0.83306073807, 213.299095438},
                                   {132.212, 3.41118275555, 2942.4634232916},
                                   {126.184, 1.0830263021, 20.7753954924},
                                   {115.132, 0.64544911683, 0.9803210682},
                                   {102.851, 0.63599846727, 4694.0029547076},
                                   {101.895, 0.97569221824, 15720.8387848784},
                                   {101.724, 4.26679821365, 7.1135470008},
                                   {99.206, 6.20992940258, 2146.1654164752},
                                   {97.607, 0.6810127227, 155.4203994342},
                                   {85.803, 5.98322631256, 161000.6857376741},
                                   {85.128, 1.29870743025, 6275.9623029906},
                                   {84.711, 3.67080093025, 71430.69561812909},
                                   {79.637, 1.807913307, 17260.1546546904},
                                   {78.756, 3.03698313141, 12036.4607348882},
                                   {74.651, 1.75508916159, 5088.6288397668},
                                   {73.874, 3.50319443167, 3154.6870848956},
                                   {73.547, 4.67926565481, 801.8209311238},
                                   {69.627, 0.83297596966, 9437.762934887},
                                   {62.449, 3.97763880587, 8827.3902698748},
                                   {61.148, 1.81839811024, 7084.8967811152},
                                   {56.963, 2.78430398043, 6286.5989683404},
                                   {56.116, 4.38694880779, 14143.4952424306},
                                   {55.577, 3.47006009062, 6279.5527316424},
                                   {51.992, 0.18914945834, 12139.5535091068},
                                   {51.605, 1.33282746983, 1748.016413067},
                                   {51.145, 0.28306864501, 5856.4776591154},
                                   {49.0, 0.48735065033, 1194.4470102246},
                                   {41.036, 5.36817351402, 8429.2412664666},
                                   {40.938, 2.39850881707, 19651.048481098},
                                   {39.2, 6.16832995016, 10447.3878396044},
                                   {36.77, 6.04133859347, 10213.285546211},
                                   {36.596, 2.56955238628, 1059.3819301892},
                                   {35.954, 1.70876111898, 2352.8661537718},
                                   {35.566, 1.77597314691, 6812.766815086},
                                   {33.291, 0.59309499459, 17789.845619785},
                                   {30.412, 0.44294464135, 83996.84731811189},
                                   {30.047, 2.73975123935, 1349.8674096588},
                                   {25.352, 3.16470953405, 4690.4798363586}};
inline constexpr VsopTerm kL1[] = {{628331966747.491, 0.0, 0.0},
                                   {206058.863, 2.67823455584, 6283.0758499914},
                                   {4303.43, 2.63512650414, 12566.1516999828},
                                   {425.264, 1.59046980729, 3.523118349},
                                   {119.261, 5.79557487799, 26.2983197998},
                                   {108.977, 2.96618001993, 1577.3435424478},
                                   {93.478, 2.59212835365, 18849.2275499742},
                                   {72.122, 1.13846158196, 529.6909650946},
                                   {67.768, 1.87472304791, 398.1490034082},
                                   {67.327, 4.40918235168, 5507.5532386674},
                                   {59.027, 2.8879703846, 5223.6939198022},
                                   {55.976, 2.17471680261, 155.4203994342},
                                   {45.407, 0.39803079805, 796.2980068164},
                                   {36.369, 0.46624739835, 775.522611324},
                                   {28.958, 2.64707383882, 7.1135470008},
                                   {20.844, 5.34138275149, 0.9803210682},
                                   {19.097, 1.84628332577, 5486.777843175},
                                   {18.508, 4.96855124577, 213.299095438},
                                   {17.293, 2.99116864949, 6275.9623029906},
                                   {16.233, 0.03216483047, 2544.3144198834},
                                   {15.832, 1.43049285325, 2146.1654164752},
                                   {14.615, 1.20532366323, 10977.078804699},
                                   {12.461, 2.83432285512, 1748.016413067},
                                   {11.877, 3.25804815607, 5088.6288397668},
                                   {11.808, 5.2737979048, 1194.4470102246},
                                   {11.514, 2.07502418155, 4694.0029547076},
                                   {10.641, 0.76614199202, 553.5694028424},
                                   {9.969, 1.30262991097, 6286.5989683404},
                                   {9.721, 4.23925472239, 1349.8674096588},
                                   {9.452, 2.69957062864, 242.728603974},
                                   {8.577, 5.64475868067, 951.7184062506},
                                   {7.576, 5.30062664886, 2352.8661537718},
                                   {6.385, 2.65033984967, 9437.762934887},
                                   {6.101, 4.66632584188, 4690.4798363586}};
inline constexpr VsopTerm kL2[] = {{52918.87, 0.0, 0.0},
                                   {8719.837, 1.07209665242, 6283.0758499914},
                                   {309.125, 0.86728818832, 12566.1516999828},
                                   {27.339, 0.05297871691, 3.523118349},
                                   {16.334, 5.18826691036, 26.2983197998},
                                   {15.752, 3.6845788943, 155.4203994342},
                                   {9.541, 0.75742297675, 18849.2275499742},
                                   {8.937, 2.05705419118, 77713.7714681205},
                                   {6.952, 0.8267330541, 775.522611324},
                                   {5.064, 4.66284525271, 1577.3435424478},
                                   {4.061, 1.03057162962, 7.1135470008},
                                   {3.81, 3.4405080349, 5573.1428014331},
                                   {3.463, 5.14074632811, 796.2980068164},
                                   {3.169, 6.05291851171, 5507.5532386674},
                                   {3.02, 1.19246506441, 242.728603974},
                                   {2.886, 6.11652627155, 529.6909650946},
                                   {2.714, 0.30637881025, 398.1490034082},
                                   {2.538, 2.27992810679, 553.5694028424},
                                   {2.371, 4.38118838167, 5223.6939198022},
                                   {2.079, 3.75435330484, 0.9803210682}};
inline constexpr VsopTerm kL3[] = {{289.226, 5.84384198723, 6283.0758499914}, {34.955, 0.0, 0.0},
                                   {16.819, 5.48766912348, 12566.1516999828}, {2.962, 5.19577265202, 155.4203994342},
                                   {1.288, 4.72200252235, 3.523118349},       {0.714, 5.30045809128, 18849.2275499742},
                                   {0.635, 5.96925937141, 242.728603974}};
inline constexpr VsopTerm kL4[] = {
    {114.084, 3.14159265359, 0.0}, {7.717, 4.13446589358, 6283.0758499914}, {0.765, 3.83803776214, 12566.1516999828}};
inline constexpr VsopTerm kL5[] = {{0.878, 3.14159265359, 0.0}};
inline constexpr VsopTerm kR0[] = {{100013988.799, 0.0, 0.0},
                                   {1670699.626, 3.09846350771, 6283.0758499914},
                                   {13956.023, 3.0552460962, 12566.1516999828},
                                   {3083.72, 5.19846674381, 77713.7714681205},
                                   {1628.461, 1.17387749012, 5753.3848848968},
                                   {1575.568, 2.84685245825, 7860.4193924392},
                                   {924.799, 5.45292234084, 11506.7697697936},
                                   {542.444, 4.56409149777, 3930.2096962196},
                                   {472.11, 3.66100022149, 5884.9268465832},
                                   {345.983, 0.96368617687, 5507.5532386674},
                                   {328.78, 5.89983646482, 5223.6939198022},
                                   {306.784, 0.29867139512, 5573.1428014331},
                                   {243.189, 4.27349536153, 11790.6290886588},
                                   {211.829, 5.84714540314, 1577.3435424478},
                                   {185.752, 5.02194447178, 10977.078804699},
                                   {174.844, 3.01193636534, 18849.2275499742},
                                   {109.835, 5.05510636285, 5486.777843175},
                                   {98.316, 0.88681311277, 6069.7767545534},
                                   {86.499, 5.68959778254, 15720.8387848784},
                                   {85.825, 1.27083733351, 161000.6857376741},
                                   {64.903, 0.27250613787, 17260.1546546904},
                                   {62.916, 0.92177108832, 529.6909650946},
                                   {57.056, 2.01374292014, 83996.84731811189},
                                   {55.736, 5.24159798933, 71430.69561812909},
                                   {49.384, 3.24501240359, 2544.3144198834},
                                   {46.963, 2.57805070386, 775.522611324},
                                   {44.661, 5.53715807302, 9437.762934887},
                                   {42.515, 6.01110242003, 6275.9623029906},
                                   {38.968, 5.36071738169, 4694.0029547076},
                                   {38.245, 2.39255343974, 8827.3902698748},
                                   {37.49, 0.82952922332, 19651.048481098},
                                   {36.957, 4.90107591914, 12139.5535091068},
                                   {35.66, 1.67468058995, 12036.4607348882},
                                   {34.537, 1.84270693282, 2942.4634232916},
                                   {33.193, 0.24370300098, 7084.8967811152},
                                   {31.921, 0.18368229781, 5088.6288397668},
                                   {31.846, 1.77775642085, 398.1490034082},
                                   {28.464, 1.21344868176, 6286.5989683404},
                                   {27.793, 1.89934330904, 6279.5527316424},
                                   {26.275, 4.58896850401, 10447.3878396044}};
inline constexpr VsopTerm kR1[] = {{103018.608, 1.10748969588, 6283.0758499914},
                                   {1721.238, 1.06442301418, 12566.1516999828},
                                   {702.215, 3.14159265359, 0.0},
                                   {32.346, 1.02169059149, 18849.2275499742},
                                   {30.799, 2.84353804832, 5507.5532386674},
                                   {24.971, 1.31906709482, 5223.6939198022},
                                   {18.485, 1.42429748614, 1577.3435424478},
                                   {10.078, 5.91378194648, 10977.078804699},
                                   {8.654, 1.42046854427, 6275.9623029906},
                                   {8.634, 0.27146150602, 5486.777843175}};
inline constexpr VsopTerm kR2[] = {{4359.385, 5.78455133738, 6283.0758499914},
                                   {123.633, 5.57934722157, 12566.1516999828},
                                   {12.341, 3.14159265359, 0.0},
                                   {8.792, 3.62777733395, 77713.7714681205},
                                   {5.689, 1.86958905084, 5573.1428014331},
                                   {3.301, 5.47027913302, 18849.2275499742}};
inline constexpr VsopTerm kR3[] = {{144.595, 4.27319435148, 6283.0758499914}, {6.729, 3.91697608662, 12566.1516999828}};
inline constexpr VsopTerm kR4[] = {{3.858, 2.56384387339, 6283.0758499914}};

template <size_t N>
inline double vsopSum(const VsopTerm (&terms)[N], const double tau) {
  double s = 0.0;
  // cppcheck-suppress useStlAlgorithm ; a plain series sum reads clearest
  for (const VsopTerm& t : terms) s += t.a * std::cos(t.b + t.c * tau);
  return s;
}

// Delta T (TT - UT) in days, interpolated from the Espenak & Meeus (NASA)
// polynomials sampled every 10 years (within 0.7 s over 1976-2075,
// the Panchanga's 1976-2075 range).
inline double deltaTDays(const double jd) {
  static constexpr float kSeconds[] = {40.2f, 50.5f, 56.9f, 63.9f,  66.7f,  71.6f,
                                       77.6f, 84.7f, 93.0f, 113.7f, 135.0f, 156.9f};  // 1970..2080
  const double y = 2000.0 + (jd - 2451545.0) / 365.25;
  const double pos = std::clamp((y - 1970.0) / 10.0, 0.0, 10.999);
  const int i = static_cast<int>(pos);
  const double f = pos - i;
  return (kSeconds[i] + (kSeconds[i + 1] - kSeconds[i]) * f) / 86400.0;
}

// Apparent geocentric ecliptic longitude of the Sun (VSOP87, ~1 arcsecond).
inline double sunLongitude(const double jdUt) {
  const double jde = jdUt + deltaTDays(jdUt);
  const double tau = (jde - 2451545.0) / 365250.0;
  const double l = (vsopSum(kL0, tau) +
                    tau * (vsopSum(kL1, tau) +
                           tau * (vsopSum(kL2, tau) +
                                  tau * (vsopSum(kL3, tau) + tau * (vsopSum(kL4, tau) + tau * vsopSum(kL5, tau)))))) /
                   1e8;
  const double r =
      (vsopSum(kR0, tau) +
       tau * (vsopSum(kR1, tau) + tau * (vsopSum(kR2, tau) + tau * (vsopSum(kR3, tau) + tau * vsopSum(kR4, tau))))) /
      1e8;
  const double geocentric = deg(l) + 180.0;
  const double fk5 = -0.09033 / 3600.0;
  const double aberration = -20.4898 / 3600.0 / r;
  return norm360(geocentric + fk5 + aberration + nutationLongitude(centuries(jde)));
}

struct MoonTerm {
  int8_t d, m, mp, f;
  int32_t sl;  // 1e-6 deg
};

// Meeus Table 47.A (longitude terms).
inline constexpr MoonTerm kMoonTerms[] = {
    {0, 0, 1, 0, 6288774}, {2, 0, -1, 0, 1274027}, {2, 0, 0, 0, 658314},  {0, 0, 2, 0, 213618}, {0, 1, 0, 0, -185116},
    {0, 0, 0, 2, -114332}, {2, 0, -2, 0, 58793},   {2, -1, -1, 0, 57066}, {2, 0, 1, 0, 53322},  {2, -1, 0, 0, 45758},
    {0, 1, -1, 0, -40923}, {1, 0, 0, 0, -34720},   {0, 1, 1, 0, -30383},  {2, 0, 0, -2, 15327}, {0, 0, 1, 2, -12528},
    {0, 0, 1, -2, 10980},  {4, 0, -1, 0, 10675},   {0, 0, 3, 0, 10034},   {4, 0, -2, 0, 8548},  {2, 1, -1, 0, -7888},
    {2, 1, 0, 0, -6766},   {1, 0, -1, 0, -5163},   {1, 1, 0, 0, 4987},    {2, -1, 1, 0, 4036},  {2, 0, 2, 0, 3994},
    {4, 0, 0, 0, 3861},    {2, 0, -3, 0, 3665},    {0, 1, -2, 0, -2689},  {2, 0, -1, 2, -2602}, {2, -1, -2, 0, 2390},
    {1, 0, 1, 0, -2348},   {2, -2, 0, 0, 2236},    {0, 1, 2, 0, -2120},   {0, 2, 0, 0, -2069},  {2, -2, -1, 0, 2048},
    {2, 0, 1, -2, -1773},  {2, 0, 0, 2, -1595},    {4, -1, -1, 0, 1215},  {0, 0, 2, 2, -1110},  {3, 0, -1, 0, -892},
    {2, 1, 1, 0, -810},    {4, -1, -2, 0, 759},    {0, 2, -1, 0, -713},   {2, 2, -1, 0, -700},  {2, 1, -2, 0, 691},
    {2, -1, 0, -2, 596},   {4, 0, 1, 0, 549},      {0, 0, 4, 0, 537},     {4, -1, 0, 0, 520},   {1, 0, -2, 0, -487},
    {2, 1, 0, -2, -399},   {0, 0, 2, -2, -381},    {1, 1, 1, 0, 351},     {3, 0, -2, 0, -340},  {4, 0, -3, 0, 330},
    {2, -1, 2, 0, 327},    {0, 2, 1, 0, -323},     {1, 1, -1, 0, 299},    {2, 0, 3, 0, 294},    {2, 0, -1, -2, 0}};

// Apparent geocentric ecliptic longitude of the Moon.
inline double moonLongitude(const double jdUt) {
  const double t = centuries(jdUt + deltaTDays(jdUt));
  const double t2 = t * t, t3 = t2 * t, t4 = t3 * t;
  const double lp = 218.3164477 + 481267.88123421 * t - 0.0015786 * t2 + t3 / 538841.0 - t4 / 65194000.0;
  const double d = 297.8501921 + 445267.1114034 * t - 0.0018819 * t2 + t3 / 545868.0 - t4 / 113065000.0;
  const double m = 357.5291092 + 35999.0502909 * t - 0.0001536 * t2 + t3 / 24490000.0;
  const double mp = 134.9633964 + 477198.8675055 * t + 0.0087414 * t2 + t3 / 69699.0 - t4 / 14712000.0;
  const double f = 93.2720950 + 483202.0175233 * t - 0.0036539 * t2 - t3 / 3526000.0 + t4 / 863310000.0;
  const double a1 = 119.75 + 131.849 * t;
  const double a2 = 53.09 + 479264.290 * t;
  const double e = 1.0 - 0.002516 * t - 0.0000074 * t2;
  double sum = 0.0;
  for (const MoonTerm& k : kMoonTerms) {
    double coeff = k.sl;
    if (k.m == 1 || k.m == -1) coeff *= e;
    if (k.m == 2 || k.m == -2) coeff *= e * e;
    sum += coeff * std::sin(rad(k.d * d + k.m * m + k.mp * mp + k.f * f));
  }
  sum += 3958.0 * std::sin(rad(a1)) + 1962.0 * std::sin(rad(lp - f)) + 318.0 * std::sin(rad(a2));
  return norm360(lp + sum / 1e6 + nutationLongitude(t));
}

constexpr double kLahiriJ2000 = 23.86379956;

// Lahiri (Chitrapaksha) mean ayanamsha, calibrated to Drik Panchang's
// published values: 24.167476 deg on 2021-09-27 and 24.237323 deg on
// 2026-09-27 (0h UT) are both reproduced to 0.000001 deg.
inline double ayanamsha(const double jd) {
  const double t = centuries(jd);
  return kLahiriJ2000 + (5029.0966 * t + 1.11113 * t * t) / 3600.0;
}

inline double sunSidereal(const double jd) { return norm360(sunLongitude(jd) - ayanamsha(jd)); }
inline double moonSidereal(const double jd) { return norm360(moonLongitude(jd) - ayanamsha(jd)); }
// Moon - Sun, 0..360 (tithi angle).
inline double elongation(const double jd) { return norm360(moonLongitude(jd) - sunLongitude(jd)); }

// Low-precision apparent solar longitude (Meeus ch. 25, ~0.01 deg). Plenty for
// sunrise (0.01 deg is ~2 s) and about 40x cheaper than the VSOP87 series,
// which matters on the ESP32-C3 where doubles are emulated in software.
inline double sunLongitudeFast(const double jd) {
  const double t = centuries(jd);
  const double l0 = 280.46646 + 36000.76983 * t + 0.0003032 * t * t;
  const double m = rad(357.52911 + 35999.05029 * t - 0.0001537 * t * t);
  const double c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * std::sin(m) +
                   (0.019993 - 0.000101 * t) * std::sin(2 * m) + 0.000289 * std::sin(3 * m);
  return norm360(l0 + c - 0.00569 - 0.00478 * std::sin(rad(125.04 - 1934.136 * t)));
}

// Sun altitude (deg) at a place, for sunrise/sunset iteration.
inline double sunAltitude(const double jd, const double latDeg, const double lonDeg) {
  const double t = centuries(jd);
  const double lambda = rad(sunLongitudeFast(jd));
  const double omega = rad(125.04 - 1934.136 * t);
  const double eps = rad(23.439291 - 0.0130042 * t + 0.00256 * std::cos(omega));
  const double ra = std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda));
  const double dec = std::asin(std::sin(eps) * std::sin(lambda));
  const double gmst = norm360(280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * t * t);
  const double ha = rad(gmst + lonDeg) - ra;
  const double lat = rad(latDeg);
  return deg(std::asin(std::sin(lat) * std::sin(dec) + std::cos(lat) * std::cos(dec) * std::cos(ha)));
}

// Time (JD) when the Sun's centre crosses -0.833 deg near a first guess.
// rising=true picks the ascending crossing. Returns 0 if it never happens.
inline double sunEvent(double jd, const double lat, const double lon, const bool rising) {
  constexpr double kTarget = -0.833;
  for (int i = 0; i < 12; ++i) {
    const double h = sunAltitude(jd, lat, lon);
    const double dh = (sunAltitude(jd + 1.0 / 1440.0, lat, lon) - h) * 1440.0;  // deg per day
    if ((rising && dh <= 0) || (!rising && dh >= 0) || std::fabs(dh) < 1e-6) return 0.0;
    const double step = (kTarget - h) / dh;
    jd += step;
    if (std::fabs(step) < 1e-6) break;  // < 0.1 s
  }
  return jd;
}

// Next time after jd when f(t) (an angle growing at ~rate deg/day) reaches target.
template <typename F>
inline double nextCrossing(F f, const double jd, const double target, const double rate) {
  double t = jd + norm360(target - f(jd)) / rate;
  for (int i = 0; i < 20; ++i) {
    double diff = target - f(t);
    diff = std::fmod(diff + 540.0, 360.0) - 180.0;  // -180..180
    t += diff / rate;
    if (std::fabs(diff) < 1e-6) break;
  }
  return t;
}

inline double newMoonBefore(const double jd) {
  const double e = elongation(jd);
  double t = jd - e / 12.19;
  for (int i = 0; i < 20; ++i) {
    double diff = std::fmod(elongation(t) + 180.0, 360.0) - 180.0;
    t -= diff / 12.19;
    if (std::fabs(diff) < 1e-6) break;
  }
  if (t > jd) t -= 29.530588;  // guard: must be at or before jd
  return t;
}

struct Day {
  // Inputs
  int year = 0, month = 0, day = 0;
  double tzHours = 5.5, lat = 12.9716, lon = 77.5946;
  // Results (indices are 0-based)
  bool valid = false;
  double sunriseJd = 0, sunsetJd = 0, nextSunriseJd = 0;
  int vara = 0;   // 0 = Sunday
  int tithi = 0;  // 0..29 (0..14 shukla, 15..29 krishna)
  double tithiEndJd = 0;
  int nakshatra = 0;  // 0..26
  double nakshatraEndJd = 0;
  int yoga = 0;  // 0..26
  double yogaEndJd = 0;
  int karana = 0;  // 0..10 name index
  double karanaEndJd = 0;
  int masa = 0;  // 0 = Chaitra .. 11 = Phalguna (amanta)
  bool adhika = false;
  int samvatsara = 0;  // 0 = Prabhava .. 59 = Akshaya
  double rahuStartJd = 0, yamagandaStartJd = 0, gulikaStartJd = 0, kaalaLengthDays = 0;
  double abhijitStartJd = 0, abhijitEndJd = 0;
  bool sankranti = false;  // Sun enters a new rashi before next sunrise
  int sankrantiRashi = 0;  // 0 = Mesha
};

// Karana name index (0 Bava .. 6 Vishti, 7 Shakuni, 8 Chatushpada, 9 Naga, 10 Kimstughna).
inline int karanaName(const int half) {
  if (half == 0) return 10;
  if (half >= 57) return 7 + (half - 57);
  return (half - 1) % 7;
}

// Weekday (0 = Sunday) of a Gregorian date.
inline int weekday(const int y, const int m, const int d) {
  return static_cast<int>(std::fmod(std::floor(julianDay(y, m, d) + 1.5), 7.0));
}

inline Day compute(const int year, const int month, const int dayOfMonth, const double tzHours, const double lat,
                   const double lon) {
  Day r;
  r.year = year;
  r.month = month;
  r.day = dayOfMonth;
  r.tzHours = tzHours;
  r.lat = lat;
  r.lon = lon;
  const double localMidnight = julianDay(year, month, dayOfMonth) - tzHours / 24.0;
  r.sunriseJd = sunEvent(localMidnight + 0.25, lat, lon, true);
  r.sunsetJd = sunEvent(localMidnight + 0.75, lat, lon, false);
  if (r.sunriseJd == 0.0 || r.sunsetJd == 0.0) return r;
  const double nextSunrise = sunEvent(r.sunriseJd + 1.0, lat, lon, true);
  r.nextSunriseJd = nextSunrise;
  const double sr = r.sunriseJd;
  r.vara = weekday(year, month, dayOfMonth);

  const double e = elongation(sr);
  r.tithi = static_cast<int>(e / 12.0) % 30;
  r.tithiEndJd = nextCrossing(elongation, sr, std::fmod((r.tithi + 1) * 12.0, 360.0), 12.19);
  const int half = static_cast<int>(e / 6.0) % 60;
  r.karana = karanaName(half);
  r.karanaEndJd = nextCrossing(elongation, sr, std::fmod((half + 1) * 6.0, 360.0), 12.19);

  constexpr double kSeg = 360.0 / 27.0;
  const double ms = moonSidereal(sr);
  r.nakshatra = static_cast<int>(ms / kSeg) % 27;
  r.nakshatraEndJd = nextCrossing(moonSidereal, sr, std::fmod((r.nakshatra + 1) * kSeg, 360.0), 13.18);
  auto yogaAngle = [](const double jd) { return norm360(sunSidereal(jd) + moonSidereal(jd)); };
  r.yoga = static_cast<int>(yogaAngle(sr) / kSeg) % 27;
  r.yogaEndJd = nextCrossing(yogaAngle, sr, std::fmod((r.yoga + 1) * kSeg, 360.0), 14.17);

  // Amanta month: named after the rashi the Sun occupies at the new moon that
  // starts it (Sun in Meena -> Chaitra). Two new moons in one rashi make the
  // first month adhika.
  const double nmStart = newMoonBefore(sr);
  const double nmNext = nextCrossing(elongation, nmStart + 1.0, 0.0, 12.19);
  const int rashiStart = static_cast<int>(sunSidereal(nmStart) / 30.0) % 12;
  const int rashiNext = static_cast<int>(sunSidereal(nmNext) / 30.0) % 12;
  r.masa = (rashiStart + 1) % 12;
  r.adhika = rashiStart == rashiNext;

  // Samvatsara changes at Ugadi (Chaitra shukla pratipada); 1987-88 was Prabhava.
  int lunarYear = year;
  if (month <= 4 && r.masa >= 9) lunarYear -= 1;
  r.samvatsara = ((lunarYear - 1987) % 60 + 60) % 60;

  // Day split into eight parts; part numbers per weekday (Sunday first).
  static constexpr int kRahu[7] = {8, 2, 7, 5, 6, 4, 3};
  static constexpr int kYama[7] = {5, 4, 3, 2, 1, 7, 6};
  static constexpr int kGulika[7] = {7, 6, 5, 4, 3, 2, 1};
  const double part = (r.sunsetJd - r.sunriseJd) / 8.0;
  r.kaalaLengthDays = part;
  r.rahuStartJd = r.sunriseJd + (kRahu[r.vara] - 1) * part;
  r.yamagandaStartJd = r.sunriseJd + (kYama[r.vara] - 1) * part;
  r.gulikaStartJd = r.sunriseJd + (kGulika[r.vara] - 1) * part;
  // Abhijit: the 8th of 15 daytime muhurtas.
  const double muhurta = (r.sunsetJd - r.sunriseJd) / 15.0;
  r.abhijitStartJd = r.sunriseJd + 7 * muhurta;
  r.abhijitEndJd = r.abhijitStartJd + muhurta;

  const int rashiNow = static_cast<int>(sunSidereal(sr) / 30.0) % 12;
  const int rashiTomorrow = static_cast<int>(sunSidereal(nextSunrise) / 30.0) % 12;
  r.sankranti = rashiNow != rashiTomorrow;
  r.sankrantiRashi = rashiTomorrow;
  r.valid = true;
  return r;
}

// Illuminated fraction of the Moon's disc (0 new .. 1 full) from elongation.
inline double illumination(const double elongationDeg) { return (1.0 - std::cos(rad(elongationDeg))) / 2.0; }

// Local clock time (hours, 0..24) of a JD.
inline double localHours(const double jd, const double tzHours) {
  const double h = std::fmod((jd + 0.5) * 24.0 + tzHours, 24.0);
  return h < 0 ? h + 24.0 : h;
}

// Festivals and special days (Karnataka amanta calendar). Each festival is
// decided by the tithi at its traditional time of day, as Drik Panchang does:
// e.g. Vijayadashami by the afternoon (aparahna), Deepavali by the evening
// (pradosha), Janmashtami and Shivaratri by midnight (nishita). A festival is
// placed on the first day whose deciding time falls in the required tithi.
enum class Special : uint8_t {
  None,
  Ugadi,
  RamaNavami,
  AkshayaTritiya,
  NagaPanchami,
  Janmashtami,
  GaneshaChaturthi,
  MahalayaAmavasya,
  NavaratriStart,
  Vijayadashami,
  NarakaChaturdashi,
  Deepavali,
  BaliPadyami,
  MahaShivaratri,
  Holi,
  MakaraSankranti,
  Sankranti,
  Ekadashi,
  Purnima,
  Amavasya,
  Sankashti,
  Count
};

enum class Kaala : uint8_t { Sunrise, Madhyahna, Aparahna, Pradosha, Nishita, Arunodaya, Moonrise, KaalaCount };

inline double kaalaTime(const Day& d, const Kaala k, const double dayOffset) {
  const double sr = d.sunriseJd + dayOffset, ss = d.sunsetJd + dayOffset, next = d.nextSunriseJd + dayOffset;
  const double daylen = ss - sr;
  switch (k) {
    case Kaala::Sunrise:
      return sr + 1.0 / 1440.0;
    case Kaala::Madhyahna:
      return sr + daylen * 0.5;  // middle fifth of the day
    case Kaala::Aparahna:
      return sr + daylen * 0.7;  // fourth fifth of the day
    case Kaala::Pradosha:
      return ss + 1.2 / 24.0;  // middle of the three muhurtas after sunset
    case Kaala::Nishita:
      return ss + (next - ss) / 2.0;
    case Kaala::Arunodaya:
      return sr - 1.6 / 24.0;  // four ghatikas before sunrise
    case Kaala::Moonrise:
      return ss + 2.5 / 24.0;  // Krishna chaturthi moonrise is ~2-3 h after sunset
    default:
      return sr;
  }
}

inline int tithiAt(const double jd) { return static_cast<int>(elongation(jd) / 12.0) % 30; }

// Amanta month in force at an instant (see compute()).
inline int masaAt(const double jd, bool* adhika) {
  const double nmStart = newMoonBefore(jd);
  const double nmNext = nextCrossing(elongation, nmStart + 1.0, 0.0, 12.19);
  const int rashiStart = static_cast<int>(sunSidereal(nmStart) / 30.0) % 12;
  const int rashiNext = static_cast<int>(sunSidereal(nmNext) / 30.0) % 12;
  if (adhika != nullptr) *adhika = rashiStart == rashiNext;
  return (rashiStart + 1) % 12;
}

// Fills up to `cap` specials for the day (festivals first); returns the count.
//
// A festival falls on day D when its tithi is current at D's deciding time
// and was not at the previous day's. If the tithi is kshaya (it begins after
// one deciding time and ends before the next, so it never touches one), the
// festival is observed on the day it begins. The month is taken at the
// moment the tithi is current, so festivals on month boundaries stay correct.
inline int specials(const Day& d, Special* out, const int cap) {
  struct Rule {
    Special id;
    int8_t masa;  // -1 = any month
    int8_t tithi;
    Kaala when;
  };
  static constexpr Rule kRules[] = {
      {Special::Ugadi, 0, 0, Kaala::Sunrise},
      {Special::RamaNavami, 0, 8, Kaala::Madhyahna},
      {Special::AkshayaTritiya, 1, 2, Kaala::Sunrise},
      {Special::NagaPanchami, 4, 4, Kaala::Sunrise},
      {Special::Janmashtami, 4, 22, Kaala::Nishita},
      {Special::GaneshaChaturthi, 5, 3, Kaala::Madhyahna},
      {Special::MahalayaAmavasya, 5, 29, Kaala::Aparahna},
      {Special::NavaratriStart, 6, 0, Kaala::Sunrise},
      {Special::Vijayadashami, 6, 9, Kaala::Aparahna},
      {Special::NarakaChaturdashi, 6, 28, Kaala::Arunodaya},
      {Special::Deepavali, 6, 29, Kaala::Pradosha},
      {Special::BaliPadyami, 7, 0, Kaala::Sunrise},
      {Special::MahaShivaratri, 10, 28, Kaala::Nishita},
      {Special::Holi, 11, 14, Kaala::Pradosha},
      {Special::Ekadashi, -1, 10, Kaala::Sunrise},
      {Special::Ekadashi, -1, 25, Kaala::Sunrise},
      {Special::Purnima, -1, 14, Kaala::Sunrise},
      {Special::Amavasya, -1, 29, Kaala::Sunrise},
      {Special::Sankashti, -1, 18, Kaala::Moonrise},
  };
  constexpr int kKaalas = static_cast<int>(Kaala::KaalaCount);
  int prev[kKaalas], cur[kKaalas], next[kKaalas];
  bool have[kKaalas] = {};
  int n = 0;
  auto push = [&](const Special s) {
    for (int i = 0; i < n; ++i)
      if (out[i] == s) return;
    if (n < cap) out[n++] = s;
  };
  // Sankranti belongs to the day whose sunset follows the Sun's transit
  // (a transit after sunset is observed the next day).
  {
    const int before = static_cast<int>(sunSidereal(d.sunsetJd - 1.0) / 30.0) % 12;
    const int after = static_cast<int>(sunSidereal(d.sunsetJd) / 30.0) % 12;
    if (before != after) push(after == 9 ? Special::MakaraSankranti : Special::Sankranti);
  }
  for (const Rule& r : kRules) {
    const int k = static_cast<int>(r.when);
    if (!have[k]) {
      prev[k] = tithiAt(kaalaTime(d, r.when, -1.0));
      cur[k] = tithiAt(kaalaTime(d, r.when, 0.0));
      next[k] = tithiAt(kaalaTime(d, r.when, 1.0));
      have[k] = true;
    }
    const bool normal = cur[k] == r.tithi && prev[k] != r.tithi;
    const bool kshaya = cur[k] == (r.tithi + 29) % 30 && next[k] == (r.tithi + 1) % 30;
    if (!normal && !kshaya) continue;
    if (r.masa >= 0) {
      bool adhika = false;
      const int m = masaAt(kaalaTime(d, r.when, kshaya ? 1.0 : 0.0), &adhika);
      if (adhika || m != r.masa) continue;  // no festivals in an adhika month
    }
    push(r.id);
    if (n >= cap) break;
  }
  return n;
}

inline Special special(const Day& d) {
  Special s[1] = {Special::None};
  return specials(d, s, 1) > 0 ? s[0] : Special::None;
}

}  // namespace panchanga
