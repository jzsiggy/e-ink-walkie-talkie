/* Markup and styles for the setup page. Pure C++ templates over a string type
   (Arduino String on the device, std::string for a desktop preview) so the
   exact page can be rendered and eyeballed in a browser before flashing.
   Everything is inline: the phone has no internet while on the hotspot.
   Visual language mirrors the companion web chat (b-gs-e-ink-walkie-talkie):
   paper/ink monochrome, system sans, mono uppercase labels, 12 px radii. */
#pragma once
#include <stddef.h>
#ifndef PROGMEM
#define PROGMEM
#endif

static const char PAGE_CSS[] PROGMEM =
  ":root{--paper:#fff;--ink:#111;--fill:#f4f4f4;--line:#e4e4e4;--muted:#6f6f6f;"
  "--mono:ui-monospace,'SF Mono','Cascadia Code',Menlo,Consolas,monospace}"
  "*{margin:0;padding:0;box-sizing:border-box}"
  "html{background:var(--paper)}"
  "body{min-height:100%;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:var(--paper);color:var(--ink)}"
  "button:focus-visible,input:focus-visible,a:focus-visible{outline:2px solid var(--ink);outline-offset:2px}"
  ".page{min-height:100vh;display:flex;flex-direction:column;align-items:center;gap:10px;padding:40px 24px 48px}"
  ".col{width:min(100%,320px);display:flex;flex-direction:column;gap:10px}"
  ".eyebrow{font-family:var(--mono);font-size:11px;text-transform:uppercase;letter-spacing:.1em;color:var(--muted)}"
  ".title{font-size:20px;font-weight:600;margin-bottom:8px}"
  ".banner{display:flex;align-items:center;gap:8px;padding:8px 12px;border-radius:8px;background:var(--ink);color:var(--paper);font-family:var(--mono);font-size:12px;line-height:1.4}"
  ".net{display:flex;align-items:center;gap:14px;padding:12px 14px;border:1px solid var(--line);border-radius:12px;background:var(--paper);color:var(--ink);font:inherit;font-size:15px;text-align:left;cursor:pointer;width:100%;transition:border-color 120ms ease}"
  ".net:hover,.net.on{border-color:var(--ink)}"
  ".w{width:22px;height:22px;flex:none;--s1:var(--line);--s2:var(--line);--s3:var(--line)}"
  ".b2 .w{--s1:var(--ink)}.b3 .w{--s1:var(--ink);--s2:var(--ink)}.b4 .w{--s1:var(--ink);--s2:var(--ink);--s3:var(--ink)}"
  ".nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:500}"
  ".l{width:14px;height:16px;flex:none;color:var(--muted)}"
  ".empty{font-family:var(--mono);font-size:13px;color:var(--muted);text-align:center;padding:14px 0}"
  ".retry{align-self:center;font-family:var(--mono);font-size:11px;text-transform:uppercase;letter-spacing:.06em;padding:8px 14px;border:1px solid var(--line);border-radius:8px;background:none;color:var(--ink);cursor:pointer;text-decoration:none;transition:border-color 120ms ease}"
  ".retry:hover{border-color:var(--ink)}"
  "form.f{display:flex;flex-direction:column;gap:10px;margin-top:14px}"
  ".label{font-family:var(--mono);font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin:4px 0 -4px 2px}"
  "input{width:100%;padding:10px 14px;border:1px solid var(--line);border-radius:12px;background:var(--paper);color:var(--ink);font:inherit;font-size:15px;line-height:20px;transition:border-color 120ms ease}"
  "input:focus{outline:none;border-color:var(--ink)}"
  "input::placeholder{color:var(--muted)}"
  ".go{margin-top:6px;height:42px;border:none;border-radius:12px;background:var(--ink);color:var(--paper);font:inherit;font-size:15px;font-weight:600;cursor:pointer;transition:opacity 120ms ease}"
  ".go:hover{opacity:.85}"
  ".forget{align-self:center;margin-top:18px;font-family:var(--mono);font-size:11px;text-transform:uppercase;letter-spacing:.06em;padding:8px 14px;border:none;border-radius:7px;background:none;color:var(--muted);cursor:pointer}"
  ".forget:hover{background:var(--fill);color:var(--ink)}"
  ".ok{width:44px;height:44px;margin:6px 0 4px;--s1:var(--ink);--s2:var(--ink);--s3:var(--ink)}"
  ".sub{font-family:var(--mono);font-size:13px;color:var(--muted);text-align:center;line-height:1.5}";

/* WiFi fan (3 arcs + dot, arcs coloured by --s1..--s3) and a padlock, defined once. */
static const char PAGE_SYMBOLS[] PROGMEM =
  "<svg width=\"0\" height=\"0\" style=\"position:absolute\" aria-hidden=\"true\">"
  "<symbol id=\"w\" viewBox=\"0 0 24 24\" fill=\"none\" stroke-width=\"2\" stroke-linecap=\"round\">"
  "<path d=\"M2.6 9.2a13.6 13.6 0 0 1 18.8 0\" stroke=\"var(--s3)\"/>"
  "<path d=\"M5.7 12.3a9.2 9.2 0 0 1 12.6 0\" stroke=\"var(--s2)\"/>"
  "<path d=\"M8.8 15.4a4.8 4.8 0 0 1 6.4 0\" stroke=\"var(--s1)\"/>"
  "<circle cx=\"12\" cy=\"19\" r=\"1.7\" fill=\"var(--ink)\" stroke=\"none\"/></symbol>"
  "<symbol id=\"l\" viewBox=\"0 0 24 24\">"
  "<path d=\"M7.5 10.5V8a4.5 4.5 0 0 1 9 0v2.5\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\"/>"
  "<rect x=\"5\" y=\"10.5\" width=\"14\" height=\"10\" rx=\"2.6\" fill=\"currentColor\"/></symbol></svg>";

static const char PAGE_HEAD_A[] PROGMEM =
  "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<meta name=\"color-scheme\" content=\"light\"><title>";
static const char PAGE_HEAD_B[] PROGMEM = "</title><style>";
static const char PAGE_HEAD_C[] PROGMEM = "</style></head><body>";
static const char PAGE_FOOT[]   PROGMEM = "</div></main></body></html>";
static const char PAGE_FORM[]   PROGMEM =
  "<form class=\"f\" method=\"post\" action=\"/save\">"
  "<label class=\"label\" for=\"s\">Network</label>"
  "<input id=\"s\" name=\"ssid\" required maxlength=\"32\" autocomplete=\"off\" autocapitalize=\"none\" autocorrect=\"off\" placeholder=\"Pick one above or type it\">"
  "<label class=\"label\" for=\"p\">Password</label>"
  "<input id=\"p\" name=\"pass\" type=\"password\" maxlength=\"63\">"
  "<button class=\"go\">Save and connect</button></form>";
static const char PAGE_SCRIPT[] PROGMEM =
  "<script>var f=document.getElementById('s'),p=document.getElementById('p'),n=document.querySelectorAll('.net');"
  "n.forEach(function(b){b.onclick=function(){n.forEach(function(x){x.classList.remove('on')});"
  "b.classList.add('on');f.value=b.dataset.s;p.focus()}})</script>";

template <class S>
static S htmlEscape(const S& s) {
  S o;
  for (size_t i = 0; i < (size_t)s.length(); i++) {
    char c = s[i];
    if      (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else               o += c;
  }
  return o;
}

static inline int signalBars(int rssi) { return rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1; }

/* One network card. `name` must already be HTML-escaped. */
template <class S>
static void appendNetworkRow(S& out, const S& name, int bars, bool locked) {
  out += "<button type=\"button\" class=\"net b"; out += (char)('0' + bars); out += "\" data-s=\""; out += name;
  out += "\"><svg class=\"w\"><use href=\"#w\"/></svg><span class=\"nm\">"; out += name; out += "</span>";
  if (locked) out += "<svg class=\"l\"><use href=\"#l\"/></svg>";
  out += "</button>";
}

/* Head, symbols, centred page with eyebrow (hotspot name) and title. */
template <class S>
static void appendTop(S& out, const char* ssid, const char* title) {
  out += PAGE_HEAD_A; out += ssid; out += PAGE_HEAD_B; out += PAGE_CSS; out += PAGE_HEAD_C;
  out += PAGE_SYMBOLS;
  out += "<main class=\"page\"><div class=\"eyebrow\">"; out += ssid; out += "</div><h1 class=\"title\">"; out += title;
  out += "</h1><div class=\"col\">";
}

/* The setup page. `rows` is the concatenated network cards (may be empty),
   `notice` an optional message (already escaped), `forgetName` the currently
   stored SSID (escaped) or nullptr when nothing is stored. */
template <class S>
static void buildSetupPage(S& out, const char* ssid, const S& notice, const S& rows, const char* forgetName) {
  appendTop(out, ssid, "Connect to WiFi");
  if (notice.length()) { out += "<div class=\"banner\" role=\"alert\">"; out += notice; out += "</div>"; }
  if (rows.length()) out += rows;
  else out += "<p class=\"empty\">No networks nearby.<br>Move closer to the router.</p>";
  out += "<a class=\"retry\" href=\"/?scan=1\">Scan again</a>";
  out += PAGE_FORM;
  if (forgetName) {
    out += "<form method=\"post\" action=\"/forget\" style=\"display:contents\"><button class=\"forget\">Forget "; out += forgetName; out += "</button></form>";
  }
  out += PAGE_SCRIPT;
  out += PAGE_FOOT;
}

/* Result page after save / forget: lit fan, a title and one mono line. */
template <class S>
static void buildDonePage(S& out, const char* ssid, const char* title, const char* sub) {
  appendTop(out, ssid, title);
  out += "<svg class=\"w ok\" style=\"align-self:center\"><use href=\"#w\"/></svg><p class=\"sub\">"; out += sub; out += "</p>";
  out += PAGE_FOOT;
}
