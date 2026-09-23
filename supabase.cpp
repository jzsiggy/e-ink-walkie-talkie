#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "state.h"
#include "textfmt.h"
#include "supabase.h"

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

/* Copy one {user, message} entry into a slot if it is newer than what we hold,
   shaping text for the display on the way in (ASCII only, local timestamp).
   A null entry (never sent / nobody wrote yet) is a no-op. */
static bool applyEntry(MsgSlot& slot, JsonVariantConst entry) {
  if (entry.isNull()) return false;
  int64_t id = entry["message"]["id"] | int64_t(0);
  if (id <= slot.id) return false;
  slot.id   = id;
  slot.have = true;
  toDisplayAscii(entry["message"]["content"] | "", slot.text, sizeof slot.text);
  toDisplayAscii(entry["user"]["first_name"]  | "", slot.name, sizeof slot.name);
  if (!slot.name[0]) strlcpy(slot.name, "?", sizeof slot.name);
  isoToLocalStamp(entry["message"]["created_at"] | "", TZ_OFFSET, slot.time, sizeof slot.time);
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
