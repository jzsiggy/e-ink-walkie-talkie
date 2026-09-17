#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "state.h"
#include "supabase.h"

/* "2024-05-01T13:07:22.123+00:00" -> "13:07" shifted by TZ_OFFSET. */
static void isoToLocalHHMM(const char* iso, char* out, size_t n) {
  if (strlen(iso) < 16) { strlcpy(out, "--:--", n); return; }
  int h = (iso[11]-'0')*10 + (iso[12]-'0');
  int m = (iso[14]-'0')*10 + (iso[15]-'0');
  h = ((h + TZ_OFFSET) % 24 + 24) % 24;
  snprintf(out, n, "%02d:%02d", h, m);
}

/* One authenticated JSON POST. Returns the HTTP status (negative on client
   error). Fills *response with the body when non-null and the request went out. */
static int supaPost(const char* path, const String& body, String* response,
                    const char* prefer = nullptr) {
  WiFiClientSecure client;
  client.setInsecure();          // private toy; cert pinning is the tidy upgrade
  HTTPClient http;
  http.begin(client, String(SUPA_URL) + path);
  http.addHeader("apikey", SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Content-Type", "application/json");
  if (prefer) http.addHeader("Prefer", prefer);
  int code = http.POST(body);
  if (response && code > 0) *response = http.getString();
  http.end();
  return code;
}

/* Copy one {user, message} entry into a slot if it is newer than what we hold.
   A null entry (never sent / nobody wrote yet) is a no-op. */
static bool applyEntry(MsgSlot& slot, JsonVariantConst entry) {
  if (entry.isNull()) return false;
  int64_t id = entry["message"]["id"] | int64_t(0);
  if (id <= slot.id) return false;
  slot.id   = id;
  slot.have = true;
  strlcpy(slot.text, entry["message"]["content"] | "", sizeof slot.text);
  strlcpy(slot.name, entry["user"]["first_name"]  | "?", sizeof slot.name);
  isoToLocalHHMM(entry["message"]["created_at"] | "", slot.time, sizeof slot.time);
  return true;
}

FetchResult fetchLatest() {
  JsonDocument req;
  req["me"] = MY_USER_ID;
  String body;
  serializeJson(req, body);

  String resp;
  if (supaPost("/rest/v1/rpc/latest", body, &resp) != 200) return FETCH_FAILED;

  JsonDocument doc;
  if (deserializeJson(doc, resp)) return FETCH_FAILED;

  bool changed = false;
  changed |= applyEntry(cache.mine,   doc["outgoing"]);
  changed |= applyEntry(cache.theirs, doc["incoming"]);
  return changed ? FETCH_CHANGED : FETCH_UNCHANGED;
}

bool postMessage(const char* text) {
  JsonDocument doc;
  doc["sender"]  = MY_USER_ID;
  doc["content"] = text;
  String body;
  serializeJson(doc, body);
  return supaPost("/rest/v1/messages", body, nullptr, "return=minimal") == 201;   // PostgREST: 201 Created
}
