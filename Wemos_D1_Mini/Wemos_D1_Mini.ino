// =================================
//   Speed Sensor WiFi Bridge
//   Wemos D1 Mini (ESP8266)
//   Receives data from Arduino Mega
//   via Serial and serves a webpage
//   Phone connects to:
//   SSID: SpeedSensor
//   Pass: speedsensor
//   Browser opens automatically via
//   Captive Portal (like hotel WiFi)
//   or manually at: 4.3.2.1
//
//   Endpoints:
//   /       -- main HTML page (loads once)
//   /data   -- JSON data (polled every second by JS)
//   /clear  -- clears reading history
// =================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

const char* AP_SSID = "SpeedSensor";
const char* AP_PASS = "speedsensor";

const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer server(80);

#define MAX_HISTORY 20

struct Reading {
  String speed_ms;
  String speed_kmh;
  String speed_mph;
  String direction;
  String timestamp;
  String count;
};

Reading history[MAX_HISTORY];
int historyCount = 0;
int historyHead  = 0;

String systemStatus = "Waiting for Arduino...";
bool   arduinoReady = false;

const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Speed Sensor</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@400;600;700&display=swap');
    :root {
      --bg:       #eef2f7;
      --panel:    #ffffff;
      --panel2:   #f0f4f9;
      --border:   #b8cce0;
      --accent:   #0077cc;
      --green:    #00875a;
      --orange:   #d9520d;
      --yellow:   #b86e00;
      --text:     #0d1b2a;
      --subtext:  #2c4a6e;
      --dim:      #5a7a9a;
      --font-mono:'Share Tech Mono',monospace;
      --font-ui:  'Rajdhani',sans-serif;
    }

    *{box-sizing:border-box;margin:0;padding:0}
    body{background:var(--bg);color:var(--text);font-family:var(--font-ui);min-height:100vh;padding:16px}
    header{text-align:center;padding:20px 0 16px;border-bottom:2px solid var(--border);margin-bottom:8px}
    header h1{font-size:2rem;font-weight:700;letter-spacing:.2em;text-transform:uppercase;color:var(--accent)}
    header p{font-family:var(--font-mono);font-size:.72rem;color:var(--dim);margin-top:6px;letter-spacing:.12em}
    .conn-info{display:flex;justify-content:center;gap:24px;padding:10px 0 14px;font-family:var(--font-mono);font-size:.7rem;color:var(--dim);letter-spacing:.06em;border-bottom:1px solid var(--border);margin-bottom:16px;flex-wrap:wrap}
    .conn-info span{color:var(--accent);font-weight:bold}
    .status-bar{font-family:var(--font-mono);font-size:.82rem;text-align:center;margin-bottom:20px;padding:10px 16px;border-radius:6px;letter-spacing:.06em;transition:all .3s;font-weight:bold}
    .status-bar.armed{background:rgba(0,135,90,.12);border:2px solid rgba(0,135,90,.5);color:var(--green)}
    .status-bar.waiting{background:rgba(217,82,13,.12);border:2px solid rgba(217,82,13,.5);color:var(--orange)}
    .status-bar .dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:8px;vertical-align:middle}
    .armed .dot{background:var(--green);animation:pulse 1.5s infinite}
    .waiting .dot{background:var(--orange)}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:.2}}
    .latest-card{background:var(--panel);border:1px solid var(--border);border-top:3px solid var(--accent);border-radius:8px;padding:20px;margin-bottom:20px;box-shadow:0 2px 8px rgba(0,0,0,.08)}
    .card-label{font-size:.68rem;letter-spacing:.22em;text-transform:uppercase;color:var(--dim);margin-bottom:16px;font-family:var(--font-mono)}
    .speed-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin-bottom:18px}
    .speed-item{background:var(--panel2);border:1px solid rgba(0,119,204,.25);border-radius:6px;padding:16px 8px;text-align:center}
    .speed-item .unit-name{font-family:var(--font-mono);font-size:.65rem;color:var(--dim);letter-spacing:.1em;text-transform:uppercase;margin-bottom:8px}
    .speed-item .val{font-family:var(--font-mono);font-size:1.9rem;color:var(--accent);display:block;line-height:1}
    .speed-item .unit{font-size:.65rem;color:var(--subtext);letter-spacing:.1em;text-transform:uppercase;margin-top:5px;display:block}
    .meta-row{display:flex;justify-content:space-between;align-items:center;padding-top:16px;border-top:1px solid var(--border)}
    .dir-label{font-family:var(--font-mono);font-size:.65rem;color:var(--dim);letter-spacing:.12em;text-transform:uppercase;margin-bottom:6px}
    .direction-badge{font-family:var(--font-mono);font-size:1.1rem;font-weight:bold;padding:7px 20px;border-radius:5px;background:rgba(0,135,90,.1);border:2px solid rgba(0,135,90,.5);color:var(--green);letter-spacing:.08em;display:inline-block}
    .meta-info{font-family:var(--font-mono);font-size:.75rem;color:var(--dim);text-align:right;line-height:2}
    .meta-info span{color:var(--subtext);font-weight:bold}
    .no-data{text-align:center;padding:32px;font-family:var(--font-mono);font-size:.85rem;color:var(--dim);letter-spacing:.05em}
    .no-data .hint{font-size:.7rem;margin-top:8px;color:var(--dim);opacity:.8}
    .history-section{background:var(--panel);border:1px solid var(--border);border-radius:8px;overflow:hidden;margin-bottom:20px;box-shadow:0 2px 8px rgba(0,0,0,.08)}
    .history-header{padding:12px 16px;border-bottom:1px solid var(--border);display:flex;justify-content:space-between;align-items:center;background:var(--panel2)}
    .history-header .title{font-size:.68rem;letter-spacing:.2em;text-transform:uppercase;color:var(--subtext);font-family:var(--font-mono);font-weight:bold}
    .history-header .sub{font-size:.62rem;color:var(--dim);font-family:var(--font-mono);margin-top:3px}
    .clear-btn{font-family:var(--font-mono);font-size:.68rem;padding:6px 14px;border-radius:5px;border:2px solid rgba(217,82,13,.5);background:rgba(217,82,13,.08);color:var(--orange);cursor:pointer;letter-spacing:.06em;font-weight:bold;transition:all .2s}
    .clear-btn:hover{background:rgba(217,82,13,.18);border-color:var(--orange)}

    .history-table{width:100%;border-collapse:collapse;font-family:var(--font-mono);font-size:.74rem}
    .history-table th{padding:9px 12px;text-align:left;color:var(--dim);letter-spacing:.1em;font-weight:400;border-bottom:1px solid var(--border);white-space:nowrap;font-size:.65rem;text-transform:uppercase;background:var(--panel2)}
    .history-table td{padding:10px 12px;border-bottom:1px solid var(--border);color:var(--subtext);white-space:nowrap}
    .history-table tr:last-child td{border-bottom:none}
    .history-table tr:first-child td{color:var(--accent);background:rgba(0,119,204,.05);font-weight:bold}
    .dir-cell{color:var(--green)!important;font-weight:bold}
    .empty-history{padding:26px;text-align:center;font-family:var(--font-mono);font-size:.78rem;color:var(--dim)}
    footer{text-align:center;padding:16px;font-family:var(--font-mono);font-size:.62rem;color:var(--dim);letter-spacing:.08em;opacity:.7}
  </style>
</head>
<body>

<header>
  <h1>Speed Sensor</h1>
  <p>KEYENCE LR-TB5000C x2 // ARDUINO MEGA 2560</p>
</header>

<div class="conn-info">
  WiFi: <span>SpeedSensor</span>
  &nbsp;|&nbsp;
  Address: <span>4.3.2.1</span>
</div>

<div id="statusBar" class="status-bar waiting">
  <span class="dot"></span>
  <span id="statusText">Waiting for Arduino...</span>
</div>

<div class="latest-card">
  <div class="card-label">Latest Reading</div>
  <div id="latestContent">
    <div class="no-data">No readings yet
      <div class="hint">Pass an object through both sensor beams to begin</div>
    </div>
  </div>
  <div class="meta-row" style="margin-top:16px;padding-top:16px;border-top:1px solid var(--border)">
    <div>
      <div class="dir-label">Direction of Travel</div>
      <span class="direction-badge" id="dirBadge">-- awaiting --</span>
    </div>

  </div>
</div>

<div class="history-section">
  <div class="history-header">
    <div>
      <div class="title">Reading History</div>
      <div class="sub">Newest at top -- oldest at bottom</div>
    </div>
    <button class="clear-btn" onclick="clearHistory()">&#10005; CLEAR</button>
  </div>
  <div id="historyContent">
    <div class="empty-history">No history yet -- waiting for first measurement</div>
  </div>
</div>

<footer>Seegrid Internal Use Only // Not For External Users</footer>

<script>
function clearHistory() {
  fetch('/clear').catch(function(){});
}
function updateData() {
  fetch('/data')
    .then(function(r){return r.json();})
    .then(function(d){
      var bar=document.getElementById('statusBar');
      var txt=document.getElementById('statusText');
      bar.className='status-bar '+(d.armed?'armed':'waiting');
      txt.textContent=(d.armed?'SYSTEM ARMED -- ':'WAITING -- ')+d.status;

      var lc=document.getElementById('latestContent');
      var dir=document.getElementById('dirBadge');

      if(d.count===0){
        lc.innerHTML='<div class="no-data">No readings yet<div class="hint">Pass an object through both sensor beams to begin</div></div>';
        dir.textContent='-- awaiting --';
        dir.style.background='rgba(90,122,154,.1)';
        dir.style.borderColor='rgba(90,122,154,.4)';
        dir.style.color='var(--dim)';

      } else {
        var r=d.latest;
        lc.innerHTML=
          '<div class="speed-grid">'+
            '<div class="speed-item"><div class="unit-name">Meters per Second</div><span class="val">'+r.ms+'</span><span class="unit">m/s</span></div>'+
            '<div class="speed-item"><div class="unit-name">Kilometers per Hour</div><span class="val">'+r.kmh+'</span><span class="unit">km/h</span></div>'+
            '<div class="speed-item"><div class="unit-name">Miles per Hour</div><span class="val">'+r.mph+'</span><span class="unit">mph</span></div>'+
          '</div>';
        var isRight=r.dir.toUpperCase().indexOf('RIGHT')===0;
        dir.innerHTML=isRight?'&#9658; '+r.dir:r.dir+' &#9668;';
        dir.style.background=isRight?'rgba(0,135,90,.1)':'rgba(0,119,204,.1)';
        dir.style.borderColor=isRight?'rgba(0,135,90,.5)':'rgba(0,119,204,.5)';
        dir.style.color=isRight?'var(--green)':'var(--accent)';

      }

      var hc=document.getElementById('historyContent');
      if(d.history.length===0){
        hc.innerHTML='<div class="empty-history">No history yet -- waiting for first measurement</div>';
      } else {
        var rows='';
        for(var i=0;i<d.history.length;i++){
          var h=d.history[i];
          var hr=h.dir.toUpperCase().indexOf('RIGHT')>=0;
          rows+='<tr><td>'+h.num+'</td><td>'+h.ms+'</td><td>'+h.kmh+'</td><td>'+h.mph+'</td><td class="dir-cell">'+(hr?'&#9658; '+h.dir:h.dir+' &#9668;')+'</td></tr>';
        }
        hc.innerHTML='<table class="history-table"><thead><tr><th>Reading #</th><th>m/s</th><th>km/h</th><th>mph</th><th>Direction</th></tr></thead><tbody>'+rows+'</tbody></table>';
      }
    })
    .catch(function(){});
}
updateData();
setInterval(updateData,1000);
</script>
</body>
</html>
)rawhtml";

// ================================
//   JSON Data Endpoint
// ================================
void handleData() {
  StaticJsonDocument<4096> doc;
  doc["armed"]  = arduinoReady;
  doc["status"] = systemStatus;
  doc["count"]  = historyCount;

  if (historyCount > 0) {
    int latest = (historyHead - 1 + MAX_HISTORY) % MAX_HISTORY;
    Reading& r = history[latest];
    JsonObject lat = doc.createNestedObject("latest");
    lat["ms"]  = r.speed_ms;
    lat["kmh"] = r.speed_kmh;
    lat["mph"] = r.speed_mph;
    lat["dir"] = r.direction;
    lat["num"] = r.count;
    lat["ts"]  = r.timestamp;
  }

  JsonArray arr = doc.createNestedArray("history");
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyHead - 1 - i + MAX_HISTORY) % MAX_HISTORY;
    Reading& r = history[idx];
    JsonObject row = arr.createNestedObject();
    row["ms"]  = r.speed_ms;
    row["kmh"] = r.speed_kmh;
    row["mph"] = r.speed_mph;
    row["dir"] = r.direction;
    row["num"] = r.count;
    row["ts"]  = r.timestamp;
  }

  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ================================
//   Clear History Endpoint
// ================================
void handleClear() {
  historyCount = 0;
  historyHead  = 0;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

// ================================
//   Parse Line from Arduino Mega
// ================================
void parseLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  if (line.indexOf("Waiting for object") >= 0) { systemStatus = "Waiting for object"; arduinoReady = true; return; }
  if (line.indexOf("Sensor states OK")   >= 0) { systemStatus = "Sensors OK -- ready"; arduinoReady = true; return; }
  if (line.indexOf("Timeout")            >= 0) { systemStatus = line.substring(line.indexOf("[Timeout]")); return; }
  if (line.indexOf("Starting")  >= 0 ||
      line.indexOf("Warming")   >= 0 ||
      line.indexOf("Checking")  >= 0)           { systemStatus = line; return; }
  if (line.indexOf("Speed:") < 0 || line.indexOf("Direction:") < 0) return;

  Reading r;
  int ts = line.indexOf("["),  te = line.indexOf("]");
  if (ts >= 0 && te > ts) r.timestamp = line.substring(ts + 1, te);
  int ns = line.indexOf("[#"), ne = line.indexOf("]", ns);
  if (ns >= 0 && ne > ns) r.count = line.substring(ns + 2, ne);
  int si = line.indexOf("Speed:") + 7, se = line.indexOf(" m/s");
  if (si > 7 && se > si) { r.speed_ms = line.substring(si, se); r.speed_ms.trim(); }
  int ki = line.indexOf("m/s  |  ") + 8, ke = line.indexOf(" km/h");
  if (ki > 8 && ke > ki) { r.speed_kmh = line.substring(ki, ke); r.speed_kmh.trim(); }
  int mi = line.indexOf("km/h  |  ") + 9, me = line.indexOf(" mph");
  if (mi > 9 && me > mi) { r.speed_mph = line.substring(mi, me); r.speed_mph.trim(); }
  int di = line.indexOf("Direction: ") + 11;
  if (di > 11) { r.direction = line.substring(di); r.direction.trim(); }
  if (r.speed_ms.length() == 0) return;

  history[historyHead] = r;
  historyHead = (historyHead + 1) % MAX_HISTORY;
  if (historyCount < MAX_HISTORY) historyCount++;
  systemStatus = "Last: " + r.speed_ms + " m/s -- " + r.direction;
  arduinoReady = true;
}

// ================================
//   Captive Portal Redirect
//   Catches OS detection probes from
//   Android, iOS, Windows, macOS
// ================================
void handleCaptivePortal() {
  server.sendHeader("Location", "http://4.3.2.1/", true);
  server.send(302, "text/plain", "");
}

// ================================
//   SETUP
// ================================
void setup() {
  Serial.begin(115200);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.softAPConfig(IPAddress(4,3,2,1), IPAddress(4,3,2,1), IPAddress(255,255,255,0));

  // DNS: redirect all domains to 4.3.2.1
  dnsServer.start(DNS_PORT, "*", IPAddress(4,3,2,1));

  // Main page
  server.on("/",      []() { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/data",  handleData);
  server.on("/clear", handleClear);

  // Captive portal probe URLs -- Android
  server.on("/generate_204",         handleCaptivePortal);
  server.on("/gen_204",              handleCaptivePortal);
  // Captive portal probe URLs -- iOS / macOS
  server.on("/hotspot-detect.html",  handleCaptivePortal);
  server.on("/library/test/success.html", handleCaptivePortal);
  // Captive portal probe URLs -- Windows
  server.on("/ncsi.txt",             handleCaptivePortal);
  server.on("/connecttest.txt",      handleCaptivePortal);
  server.on("/redirect",             handleCaptivePortal);
  // Catch-all for anything else
  server.onNotFound(handleCaptivePortal);

  server.begin();
}

// ================================
//   MAIN LOOP
// ================================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    parseLine(line);
  }
}