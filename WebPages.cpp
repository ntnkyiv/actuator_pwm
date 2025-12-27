// WebPages.cpp — 100% робочий варіант (XIAO ESP32-C6)
#include <Arduino.h>
#include "WebPages.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Актуатор</title>
  <style>
    body {font-family: Arial; text-align: center; background:#111; color:#0f0; padding:10px; margin:0;}
    h1 {color:#0ff;}
    .info {background:#002; padding:12px; border-radius:10px; margin:10px 0; font-size:22px;}
    button {padding:14px; margin:8px; font-size:18px; border-radius:12px; width:90%; border:none; cursor:pointer;}
    .green{background:#0f0;color:#000;} .red{background:#f44;} .blue{background:#44f;} .gray{background:#666;}
    input[type=range] {width:90%; height:40px;}

    .control-row {
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 10px;
      margin: 12px 0;
    }
    .control-row input {
      width: 140px;
      padding: 14px;
      font-size: 18px;
      text-align: center;
      border-radius: 12px;
      border: none;
    }
    .control-row button {
      flex: 1;
      padding: 14px;
      font-size: 18px;
      border-radius: 12px;
      min-width: 140px;
    }
    .control-row.full {
      justify-content: stretch;
    }
    .control-row.full button {
      width: 100%;
      max-width: 400px;
    }
  </style>
</head>
<body>
  <h1>Актуатор</h1>
  
  <div class="info">
    IP: <span id="ip">завантаження...</span><br>
    Компас: <span id="compassStatus" style="color:#f44;">перевірка...</span>
  </div>

  <div class="info">
    Azimuth: <span id="yaw">?.?</span>°<br>
    Pitch: <span id="pitch">?.?</span>°<br>
    Roll:   <span id="roll">?.?</span>°
  </div>

  <!-- Кут повороту -->
  <div class="control-row">
    <input type="number" id="deg" value="90" step="1" placeholder="±градуси">
    <button onclick="s('degree:'+document.getElementById('deg').value)">Rotate</button>
  </div>

  <!-- Азимут -->
  <div class="control-row">
    <input type="number" id="az" value="0" min="0" max="359" step="1" placeholder="0–359">
    <button class="blue" onclick="s('azimuth:'+document.getElementById('az').value)">Azimuth →</button>
  </div>

  <!-- Стоп і калібрування -->
  <div class="control-row full">
    <button class="red" onclick="s('stop')">STOP</button>
  </div>
  <div class="control-row full">
    <button onclick="s('calibrate')">Калібрувати компас</button>
  </div>
  <h2>Лінійний актуатор</h2>
  <input type="range" min="-255" max="255" value="0" step="10" id="speedSlider" oninput="s('linear_speed:'+this.value)">
  <div style="font-size:20px;">Швидкість: <span id="speedVal">0</span></div>

  <button class="green" onclick="s('extend')">Extend</button>
  <button class="blue" onclick="s('retract')">Retract</button>
  <button class="red" onclick="s('brake')">Brake</button>
  <button class="yellow" onclick="s('sleep')">Sleep</button>
  <button onclick="s('linear_toggle')">Toggle</button>

  <button class="gray" onclick="location.href='/wifi'" style="margin-top:20px;">WiFi</button>

<script>
var ws = new WebSocket("ws://"+location.hostname+"/ws");
ws.onopen = () => {
  document.getElementById("ip").innerHTML = location.hostname + " ✓";
  setInterval(() => ws.send("pry"), 500);
};
ws.onmessage = e => {
  try {
    let j = JSON.parse(e.data);
    if (j.yaw !== undefined)   document.getElementById("yaw").innerText   = Number(j.yaw).toFixed(1);
    if (j.pitch !== undefined) document.getElementById("pitch").innerText = Number(j.pitch).toFixed(1);
    if (j.roll !== undefined)  document.getElementById("roll").innerText  = Number(j.roll).toFixed(1);
    
    document.getElementById("compassStatus").innerHTML = "ПРАЦЮЄ";
    document.getElementById("compassStatus").style.color = "#0f0";
  } catch(err) {}
};
function s(cmd) {
  console.log("→ " + cmd);
  ws.send(cmd);
}
document.getElementById("speedSlider").oninput = function() {
  document.getElementById("speedVal").innerText = this.value;
  s("linear_speed:" + this.value);
};
</script>
</body>
</html>
)rawliteral";

const char wifi_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><title>WiFi</title>
<style>body{background:#222;color:#fff;text-align:center;padding:20px;font-family:Arial;}
input,button{width:90%;padding:15px;margin:10px;font-size:18px;border-radius:10px;}</style>
</head><body>
<h1>WiFi налаштування</h1>
<form action="/save" method="POST">
<input name="s" placeholder="SSID" required><br>
<input name="p" type="password" placeholder="Пароль" required><br>
<button type="submit">Зберегти</button>
</form><br><a href="/">На головну</a>
</body></html>
)rawliteral";