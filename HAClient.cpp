/*
  Home Assistant data source for the M5Stack Nightscout monitor.

  One POST to the Home Assistant template API returns the states of all
  entities of the Carelink integration as JSON. The entities are found by the
  end of their entity_id (slug of the sensor name), so the pump name in the
  entity_id does not matter. The glucose history for the mini graph is
  collected on the device, the integration only provides the latest value.
*/

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "externs.h"
#include "HAClient.h"

extern void addErrorLog(int code);
extern void drawIcon(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t color);
extern const unsigned char wifi2_icon16x16[];
extern int icon_xpos[3];
extern int icon_ypos[3];

// Jinja template evaluated by Home Assistant: [[entity_id, state], ...]
// Timestamp sensors are returned as unix time in seconds (0 = unavailable).
static const char HA_TEMPLATE[] =
  "{%- set ns = namespace(s=[]) -%}"
  "{%- for e in integration_entities('carelink') -%}"
  "{%- set v = (as_timestamp(states(e), 0) | int) if state_attr(e, 'device_class') == 'timestamp' else states(e) -%}"
  "{%- set ns.s = ns.s + [[e, v]] -%}"
  "{%- endfor -%}"
  "{{ {'s': ns.s} | tojson }}";

static bool endsWith(const char *s, const char *suffix) {
  size_t ls = strlen(s);
  size_t lx = strlen(suffix);
  return ls >= lx && strcmp(s + ls - lx, suffix) == 0;
}

// state of the entity whose entity_id ends with suffix (null if not found)
static JsonVariant haFind(JsonArray states, const char *suffix) {
  for (JsonVariant pair : states) {
    const char *ent = pair[0];
    if (ent != NULL && endsWith(ent, suffix))
      return pair[1];
  }
  return JsonVariant();
}

// numeric value of a state, false if unavailable
static bool haNum(JsonVariant v, double *out) {
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    if (s == NULL || *s == 0 || strcmp(s, "unavailable") == 0 || strcmp(s, "unknown") == 0)
      return false;
    char *end = NULL;
    double d = strtod(s, &end);
    if (end == s || !isfinite(d))      // strtod also accepts "nan" and "inf"
      return false;
    *out = d;
    return true;
  }
  if (v.is<double>()) {
    double d = v.as<double>();
    if (!isfinite(d))
      return false;
    *out = d;
    return true;
  }
  return false;
}

// numeric value of a state inside a plausible range, false if unavailable or outside the range
static bool haNumIn(JsonVariant v, double lo, double hi, double *out) {
  double d;
  if (!haNum(v, &d) || d < lo || d > hi)
    return false;
  *out = d;
  return true;
}

// binary sensor: 1 = on, 0 = off, -1 = unknown
static int haOnOff(JsonVariant v) {
  const char *s = v.as<const char *>();
  if (s == NULL)
    return -1;
  if (strcmp(s, "on") == 0)
    return 1;
  if (strcmp(s, "off") == 0)
    return 0;
  return -1;
}

static int haInt(JsonArray st, const char *suffix, int unknown, int notApplicable = -9999) {
  double d;
  if (!haNumIn(haFind(st, suffix), -1000000, 1000000, &d))    // keeps the cast to int defined
    return unknown;
  if ((int)d == notApplicable)
    return unknown;
  return (int)d;
}

// Carelink trend to Nightscout direction name and arrow angle used by the display code
static void haTrend(const char *trend, char *dir, size_t dirLen, int *angle) {
  *angle = 180; // no arrow
  strlcpy(dir, "NONE", dirLen);
  if (trend == NULL)
    return;
  if (strcmp(trend, "UP") == 0)           { strlcpy(dir, "SingleUp", dirLen);   *angle = -75; }
  else if (strcmp(trend, "UP_DOUBLE") == 0 || strcmp(trend, "UP_TRIPLE") == 0)
                                          { strlcpy(dir, "DoubleUp", dirLen);   *angle = -90; }
  else if (strcmp(trend, "DOWN") == 0)    { strlcpy(dir, "SingleDown", dirLen); *angle = 75; }
  else if (strcmp(trend, "DOWN_DOUBLE") == 0 || strcmp(trend, "DOWN_TRIPLE") == 0)
                                          { strlcpy(dir, "DoubleDown", dirLen); *angle = 90; }
}

int readHomeAssistant(struct NSinfo *ns) {
  int err = 0;

  if (WiFiMultiple.run() != WL_CONNECTED) {
    // WiFi not connected, same behavior as the Nightscout reader
    ESP.restart();
  }

  if (strlen(cfg.ha_host) == 0 || strlen(cfg.ha_token) == 0) {
    Serial.println("Home Assistant host or token not configured (ha_host, ha_token)");
    err = 1002;
    addErrorLog(err);
    return err;
  }

  M5.Lcd.fillRect(icon_xpos[0], icon_ypos[0], 16, 16, BLACK);
  drawIcon(icon_xpos[0], icon_ypos[0], (uint8_t *)wifi2_icon16x16, TFT_BLUE);

  char url[160];
  snprintf(url, sizeof(url), "http://%s:%d/api/template", cfg.ha_host, cfg.ha_port);
  Serial.print("Home Assistant query "); Serial.println(url);

  String body;
  {
    StaticJsonDocument<1024> req;
    req["template"] = HA_TEMPLATE;
    serializeJson(req, body);
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + cfg.ha_token);
  http.addHeader("Content-Type", "application/json");

  unsigned long t0 = millis();
  int httpCode = http.POST(body);
  Serial.printf("[HTTP] POST... code: %d after %lu ms\r\n", httpCode, millis() - t0);

  if (httpCode != HTTP_CODE_OK) {
    if (httpCode == 401)
      Serial.println("Home Assistant refused the access token");
    addErrorLog(httpCode);
    err = httpCode;
  } else {
    String payload = http.getString();
    Serial.printf("Response length = %d, free heap = %u\r\n", payload.length(), ESP.getFreeHeap());
    DynamicJsonDocument doc(12288);
    DeserializationError jerr = deserializeJson(doc, payload);
    if (jerr || doc.capacity() == 0) {
      Serial.printf("JSON DeserializationError %s\r\n", jerr.c_str());
      err = 1001;
      addErrorLog(err);
    } else {
      JsonArray st = doc["s"];
      double sg = 0, ts = 0, d = 0;
      // Carelink delivers 40-400 mg/dL; anything outside 20-600 is not a glucose value
      if (!haNumIn(haFind(st, "_last_glucose_level_mg_dl"), 20, 600, &sg))
        sg = 0;
      // time after 2001 and before 2096, so it fits a 32 bit time_t
      if (!haNumIn(haFind(st, "_last_glucose_update"), 1e9, 4e9, &ts) &&
          !haNumIn(haFind(st, "_last_update"), 1e9, 4e9, &ts))
        ts = 0;
      if (st.isNull() || sg <= 0 || ts <= 0) {
        // no entities, or glucose/time unavailable (Home Assistant marks old data as unavailable)
        Serial.println("No usable glucose data from Home Assistant");
        err = 1002;
        addErrorLog(err);
      } else {
        ns->is_Sugarmate = 0;
        ns->is_xDrip = 0;
        strlcpy(ns->sensDev, "Carelink (Home Assistant)", sizeof(ns->sensDev));

        ns->sensSgvMgDl = sg;
        ns->sensSgv = sg / 18.0;

        time_t newTime = (time_t)ts;
        if (newTime != ns->sensTime) {
          // new reading: keep a short history for the graph (index 0 = newest)
          for (int i = 9; i > 0; i--)
            ns->last10sgv[i] = ns->last10sgv[i - 1];
          ns->last10sgv[0] = ns->sensSgv;
        } else {
          ns->last10sgv[0] = ns->sensSgv;
        }
        ns->sensTime = newTime;
        ns->rawtime = (uint64_t)newTime * 1000;
        localtime_r(&ns->sensTime, &ns->sensTm);

        haTrend(haFind(st, "_last_glucose_trend").as<const char *>(), ns->sensDir, sizeof(ns->sensDir), &ns->arrowAngle);

        if (haNumIn(haFind(st, "_last_glucose_delta"), -600, 600, &d)) {
          ns->delta_mgdl = (int)lround(d);
          ns->delta_absolute = ns->delta_mgdl;
          ns->delta_scaled = ns->delta_mgdl / 18.0;
          if (cfg.show_mgdl)
            snprintf(ns->delta_display, sizeof(ns->delta_display), "%+d", ns->delta_mgdl);
          else
            snprintf(ns->delta_display, sizeof(ns->delta_display), "%+.1f", ns->delta_scaled);
        } else {
          ns->delta_mgdl = 0;
          ns->delta_absolute = 0;
          ns->delta_scaled = 0;
          strcpy(ns->delta_display, "--");
        }

        if (haNumIn(haFind(st, "_active_insulin"), 0, 1000, &d)) {
          ns->iob = d;
          snprintf(ns->iob_display, sizeof(ns->iob_display), "%.1f", d);
          snprintf(ns->iob_displayLine, sizeof(ns->iob_displayLine), "IOB: %.1fU", d);
        } else {
          ns->iob = 0;
          strcpy(ns->iob_display, "N/A");
          strcpy(ns->iob_displayLine, "IOB: N/A");
        }
        ns->cob = 0;
        strcpy(ns->cob_display, "N/A");
        strcpy(ns->cob_displayLine, "COB: N/A");

        // pump and sensor data for the Minimed style page
        ns->pump_battery = haInt(st, "_pump_battery_level", -1);
        ns->reservoir_units = haNumIn(haFind(st, "_reservoir_remaining_units"), 0, 1000, &d) ? (float)d : -1;
        ns->sensor_hours = haInt(st, "_sensor_duration_hours", -1, 255);
        ns->calib_hours = haInt(st, "_sensor_time_to_next_calibration_hours", -1, 255);
        ns->conduit_ok = haOnOff(haFind(st, "_conduit_pump_in_range"));
        ns->sensor_conn_ok = haOnOff(haFind(st, "_conduit_sensor_in_range"));
        ns->tir = haInt(st, "_time_in_range", -1);
        ns->tir_above = haInt(st, "_above_hyper_limit", -1);
        ns->tir_below = haInt(st, "_below_hypo_limit", -1);
        ns->avg_mgdl = haInt(st, "_average_glucose_level_mg_dl", -1);

        Serial.printf("HA sgv=%.0f mg/dL dir=%s delta=%s iob=%s battery=%d reservoir=%.1f\r\n",
                      sg, ns->sensDir, ns->delta_display, ns->iob_display, ns->pump_battery, ns->reservoir_units);
      }
    }
  }
  http.end();

  M5.Lcd.fillRect(icon_xpos[0], icon_ypos[0], 16, 16, BLACK);
  return err;
}
