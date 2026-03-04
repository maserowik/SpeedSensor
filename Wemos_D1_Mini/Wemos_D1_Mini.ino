// =================================
//   Speed Sensor WiFi Bridge

//   Wemos D1 Mini (ESP8266)
//   Receives data from Arduino Mega
//   via Serial and serves a webpage
//   Phone connects to:
//   SSID: SpeedSensor
//   Pass: speedsensor
//   Then open browser: 4.3.2.1
// =================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

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
int historyHead  = 0;  // circular buffer head

String systemStatus = "Waiting for Arduino...";
bool   arduinoReady = false;

// ================================
//   Build HTML Page
// ================================
String buildPage() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="refresh" content="2">
  <title>Speed Sensor</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Rajdhani:wght@400;600;700&display=swap');
    :root {
      --bg:#0a0e1a; --panel:#111827; --border:#1e3a5f;
      --accent:#00d4ff; --green:#00ff88; --text:#c9d1d9; --dim:#4a5568;
      --font-mono:'Share Tech Mono',monospace; --font-ui:'Rajdhani',sans-serif;
    }
    *{box-sizing:border-box;margin:0;padding:0}
    body{background:var(--bg);color:var(--text);font-family:var(--font-ui);min-height:100vh;padding:16px}
    header{text-align:center;padding:20px 0 16px;border-bottom:1px solid var(--border);margin-bottom:20px}
    header h1{font-size:1.6rem;font-weight:700;letter-spacing:.15em;text-transform:uppercase;color:var(--accent)}
    header p{font-family:var(--font-mono);font-size:.7rem;color:var(--dim);margin-top:4px;letter-spacing:.1em}
    .status-bar{font-family:var(--font-mono);font-size:.72rem;color:var(--dim);text-align:center;margin-bottom:20px;letter-spacing:.05em}
    .dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--green);margin-right:6px;animation:pulse 1.5s infinite;vertical-align:middle}
    .dot.waiting{background:#ff6b35;animation:none}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
    .latest-card{background:var(--panel);border:1px solid var(--border);border-top:3px solid var(--accent);border-radius:8px;padding:20px;margin-bottom:20px}
    .card-label{font-size:.65rem;letter-spacing:.2em;text-transform:uppercase;color:var(--dim);margin-bottom:14px;font-family:var(--font-mono)}
    .speed-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-bottom:16px}
    .speed-item{background:rgba(0,212,255,.04);border:1px solid rgba(0,212,255,.1);border-radius:6px;padding:12px 8px;text-align:center}
    .speed-item .val{font-family:var(--font-mono);font-size:1.4rem;color:var(--accent);display:block;line-height:1}
    .speed-item .unit{font-size:.65rem;color:var(--dim);letter-spacing:.1em;text-transform:uppercase;margin-top:4px;display:block}
    .meta-row{display:flex;justify-content:space-between;align-items:center;padding-top:12px;border-top:1px solid var(--border)}
    .direction-badge{font-family:var(--font-mono);font-size:.85rem;padding:4px 12px;border-radius:4px;background:rgba(0,255,136,.1);border:1px solid rgba(0,255,136,.3);color:var(--green);letter-spacing:.05em}
    .meta-info{font-family:var(--font-mono);font-size:.7rem;color:var(--dim);text-align:right;line-height:1.6}
    .no-data{text-align:center;padding:30px;font-family:var(--font-mono);font-size:.8rem;color:var(--dim);letter-spacing:.05em}
    .history-section{background:var(--panel);border:1px solid var(--border);border-radius:8px;overflow:hidden}
    .history-header{padding:12px 16px;border-bottom:1px solid var(--border);display:flex;justify-content:space-between;align-items:center}
    .history-header span{font-size:.65rem;letter-spacing:.2em;text-transform:uppercase;color:var(--dim);font-family:var(--font-mono)}
    .count-badge{background:rgba(0,212,255,.1);border:1px solid rgba(0,212,255,.2);color:var(--accent);padding:2px 8px;border-radius:10px;font-size:.65rem;font-family:var(--font-mono)}
    .history-table{width:100%;border-collapse:collapse;font-family:var(--font-mono);font-size:.7rem}
    .history-table th{padding:8px 10px;text-align:left;color:var(--dim);letter-spacing:.1em;font-weight:400;border-bottom:1px solid var(--border);white-space:nowrap;font-size:.62rem}
    .history-table td{padding:8px 10px;border-bottom:1px solid rgba(30,58,95,.5);color:var(--text);white-space:nowrap}
    .history-table tr:last-child td{border-bottom:none}
    .history-table tr:first-child td{color:var(--accent)}
    .dir-cell{color:var(--green)!important}
    .empty-history{padding:24px;text-align:center;font-family:var(--font-mono);font-size:.72rem;color:var(--dim)}
  </style>
</head>
<body>
<header>
  <h1>Speed Sensor</h1>
  <p>KEYENCE LR-TB5000C x2 // ARDUINO MEGA</p>
</header>
)rawhtml";

  // Status bar
  html += "<div class='status-bar'>";
  if (arduinoReady) {
    html += "<span class='dot'></span>SYSTEM ARMED -- " + systemStatus;
  } else {
    html += "<span class='dot waiting'></span>" + systemStatus;
  }
  html += "</div>";

  // Latest reading card
  html += "<div class='latest-card'><div class='card-label'>Latest Reading</div>";
  if (historyCount == 0) {
    html += "<div class='no-data'>No readings yet -- waiting for object...</div>";
  } else {
    int latest = (historyHead - 1 + MAX_HISTORY) % MAX_HISTORY;
    Reading& r = history[latest];
    html += "<div class='speed-grid'>";
    html += "<div class='speed-item'><span class='val'>" + r.speed_ms + "</span><span class='unit'>m/s</span></div>";
    html += "<div class='speed-item'><span class='val'>" + r.speed_kmh + "</span><span class='unit'>km/h</span></div>";
    html += "<div class='speed-item'><span class='val'>" + r.speed_mph + "</span><span class='unit'>mph</span></div>";
    html += "</div>";
    html += "<div class='meta-row'>";
    html += "<span class='direction-badge'>" + r.direction + "</span>";
    html += "<div class='meta-info'># " + r.count + "<br>" + r.timestamp + "</div>";
    html += "</div>";
  }
  html += "</div>";

  // History table
  html += "<div class='history-section'><div class='history-header'>";
  html += "<span>Reading History</span>";
  html += "<span class='count-badge'>" + String(historyCount) + " / " + String(MAX_HISTORY) + "</span>";
  html += "</div>";
  if (historyCount == 0) {
    html += "<div class='empty-history'>No history yet</div>";
  } else {
    html += "<table class='history-table'><thead><tr><th>#</th><th>m/s</th><th>km/h</th><th>mph</th><th>Direction</th><th>Time</th></tr></thead><tbody>";
    for (int i = 0; i < historyCount; i++) {
      int idx = (historyHead - 1 - i + MAX_HISTORY) % MAX_HISTORY;
      Reading& r = history[idx];
      html += "<tr><td>" + r.count + "</td><td>" + r.speed_ms + "</td><td>" + r.speed_kmh + "</td><td>" + r.speed_mph + "</td><td class='dir-cell'>" + r.direction + "</td><td>" + r.timestamp + "</td></tr>";
    }
    html += "</tbody></table>";
  }
  html += "</div></body></html>";
  return html;
}

// ================================
//   Parse Line from Arduino Mega
//   Format:
//   [MM:SS]  [#N]  Speed: X.XXX m/s  |  X.XX km/h  |  X.XX mph  |  Direction: ...
// ================================
void parseLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  // System status lines
  if (line.indexOf("Waiting for object") >= 0) { systemStatus = "Waiting for object"; arduinoReady = true; return; }
  if (line.indexOf("Sensor states OK")   >= 0) { systemStatus = "Sensors OK"; arduinoReady = true; return; }
  if (line.indexOf("Timeout")            >= 0) { systemStatus = "Timeout -- one sensor only"; return; }
  if (line.indexOf("Starting")  >= 0 ||
      line.indexOf("Warming")   >= 0 ||
      line.indexOf("Checking")  >= 0)           { systemStatus = line; return; }

  // Speed line -- must contain both Speed: and Direction:
  if (line.indexOf("Speed:") < 0 || line.indexOf("Direction:") < 0) return;

  Reading r;

  // Timestamp [MM:SS]
  int ts = line.indexOf("["),  te = line.indexOf("]");
  if (ts >= 0 && te > ts) r.timestamp = line.substring(ts + 1, te);

  // Measurement number [#N]
  int ns = line.indexOf("[#"), ne = line.indexOf("]", ns);
  if (ns >= 0 && ne > ns) r.count = line.substring(ns + 2, ne);

  // m/s
  int si = line.indexOf("Speed:") + 7, se = line.indexOf(" m/s");
  if (si > 7 && se > si) { r.speed_ms = line.substring(si, se); r.speed_ms.trim(); }

  // km/h
  int ki = line.indexOf("m/s  |  ") + 8, ke = line.indexOf(" km/h");
  if (ki > 8 && ke > ki) { r.speed_kmh = line.substring(ki, ke); r.speed_kmh.trim(); }

  // mph
  int mi = line.indexOf("km/h  |  ") + 9, me = line.indexOf(" mph");
  if (mi > 9 && me > mi) { r.speed_mph = line.substring(mi, me); r.speed_mph.trim(); }

  // Direction
  int di = line.indexOf("Direction: ") + 11;
  if (di > 11) { r.direction = line.substring(di); r.direction.trim(); }

  if (r.speed_ms.length() == 0) return;

  // Store in circular buffer
  history[historyHead] = r;
  historyHead = (historyHead + 1) % MAX_HISTORY;
  if (historyCount < MAX_HISTORY) historyCount++;

  systemStatus = "Last: " + r.speed_ms + " m/s  " + r.direction;
  arduinoReady = true;
}

// ================================
//   SETUP
// ================================
void setup() {
  Serial.begin(115200);

  // Start WiFi access point with custom IP
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.softAPConfig(
    IPAddress(4, 3, 2, 1),      // AP IP address
    IPAddress(4, 3, 2, 1),      // Gateway
    IPAddress(255, 255, 255, 0) // Subnet mask
  );

  // Serve main page
  server.on("/", []() {
    server.send(200, "text/html", buildPage());
  });

  server.begin();
}

// ================================
//   MAIN LOOP
// ================================
void loop() {
  server.handleClient();

  // Read lines from Arduino Mega
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    parseLine(line);
  }
}
