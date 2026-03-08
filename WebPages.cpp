#include <Arduino.h>
#include "WebPages.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Control Panel</title>
  <style>
    :root {
      --bg-color: #121212;
      --card-bg: #1e1e1e;
      --text-color: #e0e0e0;
      --accent-color: #00e5ff;
      --green: #00c853;
      --red: #ff1744;
      --blue: #2979ff;
      --yellow: #ffea00;
    }
    body {
      font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      text-align: center;
      background-color: var(--bg-color);
      color: var(--text-color);
      margin: 0;
      padding: 10px;
    }
    h1 { color: var(--accent-color); margin-bottom: 5px; font-weight: 300; letter-spacing: 1px; font-size: 24px;}
    h2 { font-size: 16px; color: #aaa; margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 10px; text-transform: uppercase; letter-spacing: 1px;}
    
    .container { max-width: 480px; margin: 0 auto; }
    
    .card {
      background: var(--card-bg);
      border-radius: 16px;
      padding: 15px;
      margin-bottom: 15px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
    }

    .data-grid {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 8px;
    }
    .data-item { background: #2c2c2c; padding: 8px; border-radius: 8px; }
    .data-label { display: block; font-size: 11px; color: #888; margin-bottom: 4px; }
    .data-val { font-size: 16px; font-weight: bold; color: var(--accent-color); }

    .status-bar {
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      color: #666;
      margin-bottom: 15px;
      padding: 0 5px;
    }

    .control-group { margin: 12px 0; display: flex; gap: 10px; justify-content: center; align-items: center;}
    
    input {
      background: #333;
      border: 1px solid #444;
      color: #fff;
      padding: 12px;
      border-radius: 8px;
      font-size: 18px;
      text-align: center;
      width: 80px;
      outline: none;
      transition: border-color 0.2s;
    }
    input:focus { border-color: var(--accent-color); }

    button {
      flex: 1;
      padding: 14px;
      font-size: 16px;
      font-weight: 600;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      color: #fff;
      background: #444;
      transition: transform 0.1s, opacity 0.2s;
      -webkit-tap-highlight-color: transparent;
    }
    button:active { transform: scale(0.96); opacity: 0.9; }
    
    .btn-action { background: var(--blue); }
    .btn-rotate { background: #333; border: 1px solid var(--accent-color); color: var(--accent-color); }
    .btn-green { background: var(--green); color: #000; }
    .btn-speed { flex: 0 0 40px; padding: 10px 0; background: #2c2c2c; border: 1px solid #444; color: #888; border-radius: 8px; font-size: 15px; font-weight: 600; }
    .btn-speed.active { border-color: var(--accent-color); color: var(--accent-color); background: #0a2a2a; }
    .btn-red { background: var(--red); }
    .btn-yellow { background: var(--yellow); color: #000; }
    
    .stop-btn { width: 100%; font-size: 18px; letter-spacing: 1px; margin-top: 5px; padding: 16px; }
    .wifi-link { color: #444; text-decoration: none; font-size: 12px; margin-top: 20px; display: inline-block; padding: 10px;}
  </style>
</head>
<body>

<div class="container">
  <h1>ACTUATOR</h1>
  
  <div class="status-bar">
    <span>IP: <span id="ip">...</span></span>
    <span>Compass: <span id="compassStatus">...</span></span>
  </div>

  <div class="card">
    <div class="data-grid">
      <div class="data-item"><span class="data-label">AZIMUTH</span><span id="yaw" class="data-val">---</span>°</div>
      <div class="data-item"><span class="data-label">PITCH</span><span id="pitch" class="data-val">---</span>°</div>
      <div class="data-item"><span class="data-label">ROLL</span><span id="roll" class="data-val">---</span>°</div>
    </div>
  </div>

  <div class="card">
    <h2>Stepper Motor</h2>
    <div class="control-group">
      <input type="number" id="deg" value="90" step="1" placeholder="Deg">
      <button class="btn-rotate" onclick="s('degree:'+document.getElementById('deg').value)">Rotate (+/-)</button>
    </div>
    <div class="control-group">
      <input type="number" id="az" value="0" min="0" max="359" placeholder="Az">
      <button class="btn-action" onclick="s('azimuth:'+document.getElementById('az').value)">Go to Azimuth</button>
    </div>
    <button class="btn-red stop-btn" onclick="s('stop')">STOP MOTOR</button>
  </div>

  <div class="card">
    <h2>Калібрування BNO08x</h2>
    <div class="control-group">
      <label style="color:#aaa; font-size:14px;">Циклів:</label>
      <input type="number" id="calCycles" value="1" min="1" max="10" style="width:60px">
      <button id="calBtn" class="btn-red" onclick="startCalibration()">КАЛІБРУВАТИ</button>
    </div>
    <div id="calStatus" style="font-size:13px; color:#aaa; margin-top:8px; min-height:18px;"></div>
  </div>

  <div class="card">
    <h2>Linear Actuator</h2>

    <div class="control-group">
      <label style="color:#aaa; font-size:14px;">Швидкість:</label>
      <div id="speedBtns" style="display:flex; gap:6px;">
        <button class="btn-speed" onclick="setLinLevel(1)">1</button>
        <button class="btn-speed" onclick="setLinLevel(2)">2</button>
        <button class="btn-speed active" onclick="setLinLevel(3)">3</button>
        <button class="btn-speed" onclick="setLinLevel(4)">4</button>
        <button class="btn-speed" onclick="setLinLevel(5)">5</button>
      </div>
    </div>

    <div class="control-group">
      <button class="btn-green" onclick="moveLinear('extend')">EXTEND</button>
      <button class="btn-action" onclick="moveLinear('retract')">RETRACT</button>
    </div>

    <div class="control-group">
      <button class="btn-red" onclick="s('brake')">BRAKE</button>
      <button class="btn-yellow" onclick="s('sleep')">SLEEP</button>
    </div>
  </div>

  <a href="/wifi" class="wifi-link">WiFi Setup</a>
</div>

<script>
var ws;
var pryTimer;

function connectWS() {
  ws = new WebSocket("ws://" + location.host + "/ws");

  ws.onopen = () => {
    document.getElementById("ip").innerHTML = location.hostname;
    clearInterval(pryTimer);
    pryTimer = setInterval(() => { if (ws.readyState === 1) ws.send("pry"); }, 250);
  };

  ws.onmessage = e => {
    if (e.data === "calibrate_started") {
      document.getElementById("calStatus").innerText = "Виконується...";
      document.getElementById("calBtn").disabled = true;
      document.getElementById("calBtn").style.opacity = "0.5";
      return;
    }
    if (e.data === "calibrate_done") {
      document.getElementById("calBtn").disabled = false;
      document.getElementById("calBtn").style.opacity = "1";
      document.getElementById("calStatus").innerText = "Завершено ✓";
      return;
    }
    try {
      let j = JSON.parse(e.data);
      if (j.yaw   !== undefined) document.getElementById("yaw").innerText   = Number(j.yaw).toFixed(1);
      if (j.pitch !== undefined) document.getElementById("pitch").innerText = Number(j.pitch).toFixed(1);
      if (j.roll  !== undefined) document.getElementById("roll").innerText  = Number(j.roll).toFixed(1);
      if (document.getElementById("compassStatus").innerText !== "OK") {
        document.getElementById("compassStatus").innerHTML = "OK";
        document.getElementById("compassStatus").style.color = "#00c853";
      }
    } catch(err) {}
  };

  ws.onclose = () => { setTimeout(connectWS, 2000); };
}

connectWS();

function s(cmd) {
  if (ws.readyState === 1) ws.send(cmd);
}

var linLevel = 3;

function setLinLevel(n) {
  linLevel = n;
  document.querySelectorAll('.btn-speed').forEach((b, i) => {
    b.classList.toggle('active', i + 1 === n);
  });
}

function moveLinear(action) {
  s("linear_speed:" + (action === 'extend' ? linLevel : -linLevel));
}

function startCalibration() {
  const cycles = parseInt(document.getElementById("calCycles").value) || 1;
  if (!confirm("УВАГА!\n\nАнтена рухатиметься " + cycles + " цикл(ів).\nПереконайтесь, що кабелі вільні і механізму нічого не заважає.\n\nПродовжити?")) return;
  document.getElementById("calStatus").innerText = "Запуск...";
  s("calibrate:" + cycles);
}
</script>
</body>
</html>
)rawliteral";

const char wifi_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><title>WiFi</title>
<style>body{background:#121212;color:#fff;text-align:center;padding:20px;font-family:sans-serif;}
input,button{width:100%;max-width:300px;padding:15px;margin:10px auto;font-size:18px;border-radius:10px;border:1px solid #333;background:#222;color:white;display:block;}
button{background:#00e5ff;color:black;border:none;cursor:pointer;font-weight:bold;}
</style>
</head><body>
<h2>WiFi Setup</h2>
<form action="/save" method="POST">
<input name="s" placeholder="SSID Name" required>
<input name="p" type="password" placeholder="Password" required>
<button type="submit">CONNECT</button>
</form><br><a href="/" style="color:#888;text-decoration:none;">&larr; Back</a>
</body></html>
)rawliteral";