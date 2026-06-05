#pragma once

#include <cstdint>

namespace {

struct OCVPoint {
  uint16_t cv;   // per-cell centivolts (10mV)
  uint8_t pct;   // 0-100
};

// Li-Ion voltage-vs-remaining-capacity curve derived from
// Samsung INR18650-35E (NCA, 3.4Ah) qOCV 0.1C discharge at 25°C.
// Source: RWTH Aachen ISEA, DOI 10.18154/RWTH-2021-02814.
// The 0.1C rate (~340mA) approximates TX16S idle current, so this
// maps loaded pack voltage to a practical capacity estimate.
constexpr OCVPoint liionOcvTable[] = {
  { 300,  5 },   //  6.0 V pack
  { 310,  8 },   //  6.2 V
  { 320, 11 },   //  6.4 V
  { 330, 14 },   //  6.6 V
  { 340, 19 },   //  6.8 V — BATTERY_MIN default
  { 350, 28 },   //  7.0 V
  { 360, 39 },   //  7.2 V
  { 370, 51 },   //  7.4 V — BATTERY_WARN default
  { 380, 62 },   //  7.6 V
  { 390, 74 },   //  7.8 V
  { 400, 82 },   //  8.0 V
  { 410, 96 },   //  8.2 V
  { 420, 100 },  //  8.4 V — full charge
};

constexpr uint8_t liionOcvTableSize =
    sizeof(liionOcvTable) / sizeof(liionOcvTable[0]);

}  // namespace

// Interpolate per-cell centivolts against the Li-Ion capacity table.
inline uint8_t batteryPctFromPerCellCv(uint16_t perCellCv)
{
  if (perCellCv <= liionOcvTable[0].cv)
    return liionOcvTable[0].pct;
  if (perCellCv >= liionOcvTable[liionOcvTableSize - 1].cv)
    return liionOcvTable[liionOcvTableSize - 1].pct;

  for (uint8_t i = 1; i < liionOcvTableSize; i++) {
    if (perCellCv < liionOcvTable[i].cv) {
      uint16_t range = liionOcvTable[i].cv - liionOcvTable[i - 1].cv;
      uint16_t offset = perCellCv - liionOcvTable[i - 1].cv;
      uint16_t pctRange = liionOcvTable[i].pct - liionOcvTable[i - 1].pct;
      return liionOcvTable[i - 1].pct +
             uint8_t((pctRange * offset) / range);
    }
  }

  return liionOcvTable[liionOcvTableSize - 1].pct;
}

// TX battery percentage from pack voltage in 100 mV units
// (g_vbat100mV). Uses a 2S Li-Ion curve derived from RWTH Aachen
// Samsung INR18650-35E qOCV data (DOI 10.18154/RWTH-2021-02814).
// Returns 0..100.
inline uint8_t txBatteryPercent(uint8_t decivolts)
{
  if (decivolts == 0) return 0;
  uint16_t perCellCv = uint16_t(decivolts) * 10 / 2;
  return batteryPctFromPerCellCv(perCellCv);
}
