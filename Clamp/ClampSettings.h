#pragma once

// Editor settings in Settings.ini, with the keys, defaults and clamps of the
// original (ReadSettings 0x1805456e0, WriteSettings 0x180545c70).

/** Values in Settings.ini. The member defaults are the in-code defaults of the original OnUIOpen. */
struct ClampSettings
{
  /** [graphics] scale: the editor draw scale. */
  float scale = 1.f;
  /** [graphics] color: index into the accent palette. */
  int color = 4;
  /** [util] tooltips: 1 when the footer tooltips are on. */
  int tooltips = 0;
};

/** Reads Settings.ini into settings. Keys that are missing or malformed keep their value.
 * Like the original, a missing or empty file leaves settings unchanged and writes a default file. */
void ReadClampSettings(ClampSettings& settings);

/** Updates the values in Settings.ini and keeps the rest of the file. */
void WriteClampSettings(const ClampSettings& settings);
