/* Supabase REST access. Requires WiFi to be up.
 *   send: POST /rest/v1/messages    {"sender": MY_USER_ID, "content": ...}
 *   view: POST /rest/v1/rpc/latest  {"me": MY_USER_ID}
 *         -> {"outgoing": {user, message} | null, "incoming": {user, message} | null}
 */
#pragma once

enum FetchResult {
  FETCH_FAILED,      // HTTP or parse error; `cache` untouched
  FETCH_UNCHANGED,   // server answered, nothing newer than `cache`
  FETCH_CHANGED      // `cache` updated
};

FetchResult fetchLatest();
bool postMessage(const char* text);    // true on 201 Created
