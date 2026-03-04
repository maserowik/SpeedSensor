// =================================
//   Speed Sensor WiFi Bridge
//   Wemos D1 Mini (ESP8266)
//   Receives data from Arduino Mega
//   via Serial and serves a webpage
//   Phone connects to:
//   SSID: SpeedSensor
//   Pass: speedsensor
//   Then open browser: 4.3.2.1
//
//   Endpoints:
//   /       -- main HTML page (loads once)
//   /data   -- JSON data (polled every second by JS)
// =================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// ================================
//   WiFi Hotspot Config
// ================================
const char* AP_SSID = "SpeedSensor";
const char* AP_PASS = "speedsensor";

// ================================
//   Web Server
// ================================
ESP8266WebServer server(80);

// ================================
//   Data Storage
// ================================
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

// ================================
//   Main HTML Page
//   Loads once -- JS polls /data
// ================================
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
      --bg:       #1c2233;
      --panel:    #253048;
      --border:   #3a5a8a;
      --accent:   #29d4ff;
      --accent2:  #ff6b35;
      --green:    #00ff88;
      --text:     #eef2f8;
      --dim:      #7a90b0;
      --font-mono: 'Share Tech Mono', monospace;
      --font-ui:   'Rajdhani', sans-serif;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: var(--bg);
      color: var(--text);
      font-family: var(--font-ui);
      min-height: 100vh;
      padding: 16px;
    }

    header {
      text-align: center;
      padding: 20px 0 16px;
      border-bottom: 2px solid var(--border);
      margin-bottom: 8px;
    }

    header h1 {
      font-size: 1.8rem;
      font-weight: 700;
      letter-spacing: 0.15em;
      text-transform: uppercase;
      color: var(--accent);
    }

    header p {
      font-family: var(--font-mono);
      font-size: 0.72rem;
      color: var(--dim);
      margin-top: 4px;
      letter-spacing: 0.1em;
    }

    .conn-info {
      display: flex;
      justify-content: center;
      gap: 24px;
      padding: 8px 0 14px;
      font-family: var(--font-mono);
      font-size: 0.68rem;
      color: var(--dim);
      letter-spacing: 0.06em;
      border-bottom: 1px solid var(--border);
      margin-bottom: 16px;
      flex-wrap: wrap;
    }

    .conn-info span { color: var(--accent); }

    .status-bar {
      font-family: var(--font-mono);
      font-size: 0.78rem;
      text-align: center;
      margin-bottom: 20px;
      padding: 8px 16px;
      border-radius: 4px;
      letter-spacing: 0.05em;
      transition: background 0.3s, border 0.3s, color 0.3s;
    }

    .status-bar.armed {
      background: rgba(0,255,136,0.12);
      border: 1px solid rgba(0,255,136,0.4);
      color: var(--green);
    }

    .status-bar.waiting {
      background: rgba(255,107,53,0.12);
      border: 1px solid rgba(255,107,53,0.4);
      color: var(--accent2);
    }

    .status-bar .dot {
      display: inline-block;
      width: 9px; height: 9px;
      border-radius: 50%;
      margin-right: 7px;
      vertical-align: middle;
    }

    .armed .dot  { background: var(--green); animation: pulse 1.5s infinite; }
    .waiting .dot { background: var(--accent2); }

    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50%       { opacity: 0.3; }
    }

    .section-label {
      font-family: var(--font-mono);
      font-size: 0.65rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--dim);
      margin-bottom: 6px;
    }

    .latest-card {
      background: var(--panel);
      border: 1px solid var(--border);
      border-top: 3px solid var(--accent);
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 20px;
    }

    .card-label {
      font-size: 0.65rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--dim);
      margin-bottom: 14px;
      font-family: var(--font-mono);
    }

    .speed-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 12px;
      margin-bottom: 16px;
    }

    .speed-item {
      background: rgba(41,212,255,0.06);
      border: 1px solid rgba(41,212,255,0.2);
      border-radius: 6px;
      padding: 14px 8px;
      text-align: center;
    }

    .speed-item .unit-name {
      font-family: var(--font-mono);
      font-size: 0.62rem;
      color: var(--dim);
      letter-spacing: 0.1em;
      text-transform: uppercase;
      margin-bottom: 6px;
    }

    .speed-item .val {
      font-family: var(--font-mono);
      font-size: 1.6rem;
      color: var(--accent);
      display: block;
      line-height: 1;
      transition: color 0.2s;
    }

    .speed-item .unit {
      font-size: 0.62rem;
      color: var(--dim);
      letter-spacing: 0.1em;
      text-transform: uppercase;
      margin-top: 4px;
      display: block;
    }

    .val.flash { color: #ffffff; }

    .meta-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-top: 14px;
      border-top: 1px solid var(--border);
    }

    .dir-label {
      font-family: var(--font-mono);
      font-size: 0.62rem;
      color: var(--dim);
      letter-spacing: 0.1em;
      text-transform: uppercase;
      margin-bottom: 4px;
    }

    .direction-badge {
      font-family: var(--font-mono);
      font-size: 1rem;
      padding: 5px 14px;
      border-radius: 4px;
      background: rgba(0,255,136,0.12);
      border: 1px solid rgba(0,255,136,0.4);
      color: var(--green);
      letter-spacing: 0.05em;
      display: inline-block;
    }

    .meta-info {
      font-family: var(--font-mono);
      font-size: 0.72rem;
      color: var(--dim);
      text-align: right;
      line-height: 1.8;
    }

    .meta-info span { color: var(--text); }

    .no-data {
      text-align: center;
      padding: 30px;
      font-family: var(--font-mono);
      font-size: 0.82rem;
      color: var(--dim);
    }

    .no-data .hint {
      font-size: 0.68rem;
      margin-top: 6px;
      opacity: 0.7;
    }

    /* History */
    .history-section {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 8px;
      overflow: hidden;
      margin-bottom: 20px;
    }

    .history-header {
      padding: 12px 16px;
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: rgba(0,0,0,0.15);
    }

    .history-header .title {
      font-size: 0.65rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--dim);
      font-family: var(--font-mono);
    }

    .history-header .sub {
      font-size: 0.6rem;
      color: var(--dim);
      font-family: var(--font-mono);
      opacity: 0.7;
      margin-top: 2px;
    }

    .count-badge {
      background: rgba(41,212,255,0.12);
      border: 1px solid rgba(41,212,255,0.3);
      color: var(--accent);
      padding: 3px 10px;
      border-radius: 10px;
      font-size: 0.68rem;
      font-family: var(--font-mono);
    }

    .history-table {
      width: 100%;
      border-collapse: collapse;
      font-family: var(--font-mono);
      font-size: 0.72rem;
    }

    .history-table th {
      padding: 8px 12px;
      text-align: left;
      color: var(--dim);
      letter-spacing: 0.1em;
      font-weight: 400;
      border-bottom: 1px solid var(--border);
      white-space: nowrap;
      font-size: 0.63rem;
      text-transform: uppercase;
    }

    .history-table td {
      padding: 9px 12px;
      border-bottom: 1px solid rgba(58,90,138,0.4);
      color: var(--text);
      white-space: nowrap;
    }

    .history-table tr:last-child td { border-bottom: none; }
    .history-table tr:first-child td { color: var(--accent); background: rgba(41,212,255,0.05); }
    .dir-cell { color: var(--green) !important; }

    .empty-history {
      padding: 24px;
      text-align: center;
      font-family: var(--font-mono);
      font-size: 0.75rem;
      color: var(--dim);
    }

    /* Legend */
    .legend {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 16px 20px;
      margin-bottom: 20px;
    }

    .legend-title {
      font-family: var(--font-mono);
      font-size: 0.65rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      color: var(--accent);
      margin-bottom: 14px;
      padding-bottom: 8px;
      border-bottom: 1px solid var(--border);
    }

    .legend-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px 24px;
    }

    .legend-item {
      display: flex;
      gap: 10px;
      align-items: flex-start;
    }

    .legend-num {
      background: var(--accent);
      color: var(--bg);
      font-family: var(--font-mono);
      font-size: 0.65rem;
      font-weight: 700;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      display: flex;
      align-items: center;
      justify-content: center;
      flex-shrink: 0;
      margin-top: 1px;
    }

    .legend-text {
      font-family: var(--font-mono);
      font-size: 0.68rem;
      color: var(--text);
      line-height: 1.5;
    }

    .legend-text .lbl {
      color: var(--accent);
      display: block;
      font-size: 0.7rem;
    }

    footer {
      text-align: center;
      padding: 14px;
      font-family: var(--font-mono);
      font-size: 0.6rem;
      color: var(--dim);
      letter-spacing: 0.08em;
      opacity: 0.6;
    }
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
  Password: <span>speedsensor</span>
  &nbsp;|&nbsp;
  Address: <span>4.3.2.1</span>
  &nbsp;|&nbsp;
  Data update: <span>every 1 second</span>
</div>

<div id="statusBar" class="status-bar waiting">
  <span class="dot"></span>
  <span id="statusText">Waiting for Arduino...</span>
</div>

<div class="section-label">Latest Reading</div>
<div class="latest-card">
  <div class="card-label">Current Measurement</div>
  <div id="latestContent">
    <div class="no-data">
      No readings yet
      <div class="hint">Pass an object through both sensor beams to begin</div>
    </div>
  </div>
</div>

<div class="section-label">Reading History (most recent first)</div>
<div class="history-section">
  <div class="history-header">
    <div>
      <div class="title">All Measurements</div>
      <div class="sub">Newest at top -- oldest at bottom</div>
    </div>
    <span id="countBadge" class="count-badge">0 / 20</span>
  </div>
  <div id="historyContent">
    <div class="empty-history">No history yet -- waiting for first measurement</div>
  </div>
</div>

<div class="legend">
  <div class="legend-title">Field Reference</div>
  <div class="legend-grid">
    <div class="legend-item">
      <div class="legend-num">1</div>
      <div class="legend-text"><span class="lbl">Status Indicator</span>Green pulsing = system armed and receiving data. Orange = waiting for Arduino connection.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">2</div>
      <div class="legend-text"><span class="lbl">Meters per Second (m/s)</span>Raw speed value used for calculation. Most precise unit.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">3</div>
      <div class="legend-text"><span class="lbl">Kilometers per Hour (km/h)</span>Speed converted from m/s. Multiply m/s by 3.6.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">4</div>
      <div class="legend-text"><span class="lbl">Miles per Hour (mph)</span>Speed converted from m/s. Multiply m/s by 2.237.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">5</div>
      <div class="legend-text"><span class="lbl">Direction of Travel</span>Which sensor triggered first. Right to Left = object passed right sensor then left sensor. Left to Right = opposite.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">6</div>
      <div class="legend-text"><span class="lbl">Reading Number</span>Sequential count since Arduino last powered on. Resets to zero on power cycle or reset.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">7</div>
      <div class="legend-text"><span class="lbl">Timestamp</span>Minutes and seconds since Arduino powered on. Format MM:SS.</div>
    </div>
    <div class="legend-item">
      <div class="legend-num">8</div>
      <div class="legend-text"><span class="lbl">Reading History</span>Last 20 measurements stored. Oldest reading is dropped when 21st arrives. Lost on Wemos power cycle.</div>
    </div>
  </div>
</div>

<footer>SPEED SENSOR SYSTEM // KEYENCE LR-TB5000C x2 // ARDUINO MEGA 2560 // WEMOS D1 MINI</footer>

<script>
function updateData() {
  fetch('/data')
    .then(r => r.json())
    .then(d => {
      // Status bar
      var bar = document.getElementById('statusBar');
      var txt = document.getElementById('statusText');
      bar.className = 'status-bar ' + (d.armed ? 'armed' : 'waiting');
      txt.textContent = (d.armed ? 'SYSTEM ARMED -- ' : 'WAITING -- ') + d.status;

      // Latest reading
      var lc = document.getElementById('latestContent');
      if (d.count === 0) {
        lc.innerHTML = '<div class="no-data">No readings yet<div class="hint">Pass an object through both sensor beams to begin</div></div>';
      } else {
        var r = d.latest;
        lc.innerHTML =
          '<div class="speed-grid">' +
            '<div class="speed-item"><div class="unit-name">Meters per Second</div><span class="val" id="v1">' + r.ms + '</span><span class="unit">m/s</span></div>' +
            '<div class="speed-item"><div class="unit-name">Kilometers per Hour</div><span class="val" id="v2">' + r.kmh + '</span><span class="unit">km/h</span></div>' +
            '<div class="speed-item"><div class="unit-name">Miles per Hour</div><span class="val" id="v3">' + r.mph + '</span><span class="unit">mph</span></div>' +
          '</div>' +
          '<div class="meta-row">' +
            '<div><div class="dir-label">Direction of Travel</div><span class="direction-badge">' + r.dir + '</span></div>' +
            '<div class="meta-info">Reading #: <span>' + r.num + '</span><br>Timestamp: <span>' + r.ts + '</span><br>Total: <span>' + d.count + ' readings</span></div>' +
          '</div>';
      }

      // Count badge
      document.getElementById('countBadge').textContent = d.count + ' / 20';

      // History table
      var hc = document.getElementById('historyContent');
      if (d.history.length === 0) {
        hc.innerHTML = '<div class="empty-history">No history yet -- waiting for first measurement</div>';
      } else {
        var rows = '';
        for (var i = 0; i < d.history.length; i++) {
          var h = d.history[i];
          rows += '<tr><td>' + h.num + '</td><td>' + h.ms + '</td><td>' + h.kmh + '</td><td>' + h.mph + '</td><td class="dir-cell">' + h.dir + '</td><td>' + h.ts + '</td></tr>';
        }
        hc.innerHTML =
          '<table class="history-table">' +
            '<thead><tr><th>Reading #</th><th>m/s</th><th>km/h</th><th>mph</th><th>Direction</th><th>Timestamp</th></tr></thead>' +
            '<tbody>' + rows + '</tbody>' +
          '</table>';
      }
    })
    .catch(function() {
      // Silently ignore fetch errors -- will retry next interval
    });
}

// Poll every 1 second
updateData();
setInterval(updateData, 1000);
</script>

</body>
</html>
)rawhtml";

// ================================
//   JSON Data Endpoint
//   Called by JavaScript every 1s
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
//   Parse Line from Arduino Mega
// ================================
void parseLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.indexOf("Waiting for object") >= 0) { systemStatus = "Waiting for object"; arduinoReady = true; return; }
  if (line.indexOf("Sensor states OK")   >= 0) { systemStatus = "Sensors OK -- ready"; arduinoReady = true; return; }
  if (line.indexOf("Timeout")            >= 0) { systemStatus = "Timeout -- one sensor only"; return; }
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
//   SETUP
// ================================
void setup() {
  Serial.begin(115200);

  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.softAPConfig(
    IPAddress(4, 3, 2, 1),
    IPAddress(4, 3, 2, 1),
    IPAddress(255, 255, 255, 0)
  );

  // Main page -- loads once
  server.on("/", []() {
    server.send_P(200, "text/html", PAGE_HTML);
  });

  // Data endpoint -- polled every second by JS
  server.on("/data", handleData);

  server.begin();
}

// ================================
//   MAIN LOOP
// ================================
void loop() {
  server.handleClient();

  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    parseLine(line);
  }
}
