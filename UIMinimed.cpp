/*
  Minimed Monitor style first page for the M5Stack Nightscout monitor.
  Only uses drawing primitives and the built-in free fonts, so it needs
  no images and fits in the memory of the M5Stack Core (Basic/Gray).
*/

#include "M5NSconfig.h"
#include "Free_Fonts.h"
#include "externs.h"
#include "UIMinimed.h"
#include <math.h>

#define COL_SHIELD_FILL 0x0187   // dark teal
#define COL_UNIT        0x867D   // light blue
#define COL_GREY        0x7BEF
#define COL_ORANGE      0xFD20

static uint16_t colorLevel(int pct) {
  if (pct <= 0)  return TFT_RED;
  if (pct <= 25) return COL_ORANGE;
  return TFT_GREEN;
}

// pump battery, upright battery that is filled from the bottom
static void drawPumpBattery(int x, int y, int pct) {
  uint16_t col = (pct < 0) ? COL_GREY : colorLevel(pct);
  M5.Lcd.fillRect(x + 9, y + 1, 6, 3, TFT_WHITE);
  M5.Lcd.drawRoundRect(x + 4, y + 4, 16, 24, 2, TFT_WHITE);
  if (pct > 0) {
    int h = (20 * pct) / 100;
    M5.Lcd.fillRect(x + 6, y + 26 - h, 12, h, col);
  }
}

// insulin reservoir, fill level relative to 300 units
static void drawReservoir(int x, int y, float units) {
  uint16_t col = COL_GREY;
  if (units >= 0)
    col = (units > 150) ? TFT_GREEN : (units > 80 ? COL_ORANGE : TFT_RED);
  M5.Lcd.fillRect(x + 10, y, 4, 4, TFT_WHITE);          // plunger
  M5.Lcd.drawRoundRect(x + 4, y + 4, 16, 24, 2, TFT_WHITE);
  if (units > 0) {
    int h = (int)(20.0 * (units > 300 ? 300 : units) / 300.0);
    if (h < 1) h = 1;
    M5.Lcd.fillRect(x + 6, y + 26 - h, 12, h, col);
  }
}

// sensor connection, signal bars
static void drawSensorConn(int x, int y, int state) {
  uint16_t col = (state < 0) ? COL_GREY : (state == 1 ? TFT_GREEN : TFT_RED);
  for (int i = 0; i < 4; i++) {
    int h = 6 + i * 6;
    M5.Lcd.fillRect(x + 2 + i * 6, y + 28 - h, 4, h, (state == 0 && i > 0) ? COL_GREY : col);
  }
}

// calibration: ring with a drop, red ring if a calibration is due within 3 hours
static void drawCalibration(int x, int y, int hours) {
  uint16_t col = (hours >= 0 && hours <= 3) ? TFT_RED : TFT_GREEN;
  M5.Lcd.drawCircle(x + 12, y + 16, 12, col);
  M5.Lcd.drawCircle(x + 12, y + 16, 11, col);
  M5.Lcd.fillCircle(x + 12, y + 19, 4, TFT_WHITE);
  M5.Lcd.fillTriangle(x + 8, y + 17, x + 16, y + 17, x + 12, y + 8, TFT_WHITE);
}

// sensor age: calendar like box with the remaining days (hours if less than 10 h)
static void drawSensorAge(int x, int y, int hours) {
  char txt[16];
  uint16_t col = COL_GREY;
  if (hours < 0) {
    strcpy(txt, "?");
  } else {
    col = (hours > 9) ? TFT_GREEN : TFT_RED;
    sprintf(txt, "%d", hours > 9 ? (hours + 12) / 24 : hours);
  }
  M5.Lcd.drawRoundRect(x + 1, y + 3, 24, 26, 4, col);
  M5.Lcd.fillRect(x + 6, y + 1, 3, 5, col);
  M5.Lcd.fillRect(x + 17, y + 1, 3, 5, col);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.drawString(txt, x + 13, y + 18, GFXFF);
}

// trend arrow, angle like ns.arrowAngle: 0 = flat, 90 = down, -90 = up, 180 = none
static void drawTrendArrow(int cx, int cy, int angleDeg, uint16_t color) {
  if (angleDeg == 180)
    return;
  float a = angleDeg * PI / 180.0;
  float dx = cos(a), dy = sin(a);     // direction
  float px = -dy, py = dx;            // perpendicular
  int heads = (angleDeg == 90 || angleDeg == -90) ? 2 : 1;
  float len = 38.0;
  float tipx = cx + dx * len / 2, tipy = cy + dy * len / 2;
  float tailx = cx - dx * len / 2, taily = cy - dy * len / 2;
  float shaftEnd = len - 14.0 - (heads == 2 ? 11.0 : 0.0);
  float sx = tailx + dx * shaftEnd, sy = taily + dy * shaftEnd;
  // shaft
  M5.Lcd.fillTriangle(tailx + px * 3, taily + py * 3, tailx - px * 3, taily - py * 3, sx + px * 3, sy + py * 3, color);
  M5.Lcd.fillTriangle(tailx - px * 3, taily - py * 3, sx - px * 3, sy - py * 3, sx + px * 3, sy + py * 3, color);
  // head(s)
  for (int h = 0; h < heads; h++) {
    float tx = tipx - dx * 11.0 * h, ty = tipy - dy * 11.0 * h;
    float bx = tx - dx * 14.0, by = ty - dy * 14.0;
    M5.Lcd.fillTriangle(tx, ty, bx + px * 11, by + py * 11, bx - px * 11, by - py * 11, color);
  }
}

static void drawShieldShape(uint16_t fill, uint16_t outline) {
  const int L = 84, R = 236, T = 44, S = 112, B = 190, M = 160;
  M5.Lcd.fillRect(L, T, R - L + 1, S - T, fill);
  M5.Lcd.fillTriangle(L, S, R, S, M, B, fill);
  for (int o = 0; o < 2; o++) {
    M5.Lcd.drawLine(L + o, T, R - o, T, outline);
    M5.Lcd.drawLine(L + o, T, L + o, S, outline);
    M5.Lcd.drawLine(R - o, T, R - o, S, outline);
    M5.Lcd.drawLine(L + o, S, M, B - o, outline);
    M5.Lcd.drawLine(R - o, S, M, B - o, outline);
  }
}

void drawPageMinimed(struct NSinfo *ns) {
  char str[32];
  struct tm timeinfo;
  bool timeOK = getLocalTime(&timeinfo);

  M5.Lcd.fillRect(0, 0, 320, 220, TFT_BLACK);
  M5.Lcd.setTextSize(1);

  // top left: pump and sensor icons (only with the Carelink data), otherwise the name
  if (cfg.data_source == 1) {
    drawPumpBattery(2, 2, ns->pump_battery);
    drawReservoir(31, 2, ns->reservoir_units);
    drawSensorConn(60, 2, ns->sensor_conn_ok);
    drawCalibration(89, 2, ns->calib_hours);
    drawSensorAge(118, 2, ns->sensor_hours);
  } else {
    M5.Lcd.setFreeFont(FSSB12);
    M5.Lcd.setTextDatum(TL_DATUM);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.drawString(cfg.userName, 0, 4, GFXFF);
  }

  // top right: clock
  if (timeOK) {
    if (cfg.time_format == 1) {
      strftime(str, sizeof(str), "%I:%M%p", &timeinfo);
      str[strlen(str) - 1] = 0; // "AM" -> "A", short
    } else {
      strftime(str, sizeof(str), "%H:%M", &timeinfo);
    }
  } else {
    strcpy(str, "--:--");
  }
  M5.Lcd.setFreeFont(FSSB18);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.drawString(str, 319, 2, GFXFF);

  // colors by glucose range (thresholds in mmol/L)
  uint16_t glColor = TFT_WHITE;
  uint16_t outline = TFT_CYAN;
  if (ns->sensSgv < cfg.yellow_low || ns->sensSgv > cfg.yellow_high) {
    glColor = TFT_YELLOW;
    outline = TFT_YELLOW;
  }
  if (ns->sensSgv < cfg.red_low || ns->sensSgv > cfg.red_high) {
    glColor = TFT_RED;
    outline = TFT_RED;
  }
  drawShieldShape(COL_SHIELD_FILL, outline);

  // big glucose value, smaller font if it does not fit into the shield
  if (cfg.show_mgdl)
    sprintf(str, "%.0f", ns->sensSgvMgDl);
  else
    sprintf(str, "%.1f", ns->sensSgv);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSSB18);
  M5.Lcd.setTextSize(2);
  if (M5.Lcd.textWidth(str) > 136) {
    M5.Lcd.setFreeFont(FSSB24);
    M5.Lcd.setTextSize(1);
  }
  M5.Lcd.setTextColor(glColor, COL_SHIELD_FILL);
  M5.Lcd.drawString(str, 160, 84, GFXFF);

  // unit and delta
  M5.Lcd.setTextSize(1);
  M5.Lcd.setFreeFont(FSS12);
  M5.Lcd.setTextColor(COL_UNIT, COL_SHIELD_FILL);
  M5.Lcd.drawString(cfg.show_mgdl ? "mg/dL" : "mmol/L", 160, 130, GFXFF);
  M5.Lcd.setFreeFont(FSSB12);
  M5.Lcd.setTextColor(TFT_WHITE, COL_SHIELD_FILL);
  M5.Lcd.drawString(ns->delta_display, 160, 156, GFXFF);

  // trend arrow right of the shield
  drawTrendArrow(280, 92, ns->arrowAngle, glColor);

  // bottom right: active insulin
  bool haveIob = (strncmp(ns->iob_display, "N/A", 3) != 0) && (cfg.data_source == 1 || cfg.show_COB_IOB);
  if (haveIob) {
    sprintf(str, "%s U", ns->iob_display);
    M5.Lcd.setTextDatum(BR_DATUM);
    M5.Lcd.setFreeFont(FSSB18);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.drawString(str, 319, 176, GFXFF);
    M5.Lcd.setFreeFont(FSS9);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.drawString("Act Insulin", 319, 196, GFXFF);
  }

  // bottom left: time in range with the Carelink data, carbs on board otherwise
  M5.Lcd.setTextDatum(BL_DATUM);
  if (cfg.data_source == 1 && ns->tir >= 0) {
    sprintf(str, "%d %%", ns->tir);
    M5.Lcd.setFreeFont(FSSB18);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.drawString(str, 2, 176, GFXFF);
    M5.Lcd.setFreeFont(FSS9);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.drawString("In range", 2, 196, GFXFF);
  } else if (cfg.data_source == 0 && cfg.show_COB_IOB && strncmp(ns->cob_display, "N/A", 3) != 0) {
    sprintf(str, "%s g", ns->cob_display);
    M5.Lcd.setFreeFont(FSSB18);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.drawString(str, 2, 176, GFXFF);
    M5.Lcd.setFreeFont(FSS9);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.drawString("Carbs", 2, 196, GFXFF);
  }

  // data age below the shield
  int difMin = -1;
  if (timeOK && ns->sensTime > 0)
    difMin = (int)((difftime(mktime(&timeinfo), ns->sensTime) + 30) / 60);
  M5.Lcd.setTextDatum(MC_DATUM);
  M5.Lcd.setFreeFont(FSS12);
  if (difMin < 0) {
    strcpy(str, "---");
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  } else if (difMin > 15) {
    strcpy(str, " No data ");
    M5.Lcd.setTextColor(TFT_BLACK, COL_ORANGE);
  } else if (difMin == 0) {
    strcpy(str, "Now");
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    sprintf(str, "%d min ago", difMin);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  M5.Lcd.drawString(str, 160, 207, GFXFF);
}
