#ifndef WEBPAGE_H
#define WEBPAGE_H

// Stored in flash (PROGMEM) instead of RAM, since ESP8266 RAM is limited.
// Kept in its own file so the Arduino IDE's automatic prototype generator
// (which scans .ino files and gets confused by JavaScript inside raw
// string literals) never sees it.

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP8266 GPS Live Tracker</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <style>
    html, body, #map { height: 100%; margin: 0; padding: 0; font-family: Arial, sans-serif; }
    #info {
      position: absolute; top: 10px; left: 10px; z-index: 1000;
      background: white; padding: 12px 16px; border-radius: 8px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.3); font-size: 14px; line-height: 1.5;
    }
    #status { font-weight: bold; }
    .ok { color: green; }
    .bad { color: #cc0000; }
  </style>
</head>
<body>
  <div id="info">
    <b>ESP8266 GPS Tracker</b><br>
    Lat: <span id="lat">--</span><br>
    Lng: <span id="lng">--</span><br>
    Speed: <span id="speed">--</span> km/h<br>
    Satellites: <span id="sats">--</span><br>
    Altitude: <span id="alt">--</span><br>
    Status: <span id="status">Waiting...</span>
  </div>
  <div id="map"></div>

  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
    let map = L.map('map').setView([0, 0], 17);
    let marker = null;
    let hasFixedView = false;

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      maxZoom: 19,
      attribution: '&copy; OpenStreetMap contributors'
    }).addTo(map);

    function updateData() {
      fetch('/gps')
        .then(response => response.json())
        .then(data => {
          document.getElementById('lat').innerText = data.lat.toFixed(6);
          document.getElementById('lng').innerText = data.lng.toFixed(6);
          document.getElementById('speed').innerText = data.speed.toFixed(1);
          document.getElementById('sats').innerText = data.sats;
          document.getElementById('alt').innerText = Number(data.alt).toFixed(1);
          const statusEl = document.getElementById('status');
          if (data.valid) {
            statusEl.innerText = "GPS Fix OK";
            statusEl.className = "ok";
            const pos = [data.lat, data.lng];

            if (!marker) {
              marker = L.marker(pos).addTo(map);
            } else {
              marker.setLatLng(pos);
            }

            if (!hasFixedView) {
              map.setView(pos, 17);
              hasFixedView = true;
            } else {
              map.panTo(pos);
            }
          } else {
            statusEl.innerText = "Waiting for GPS fix...";
            statusEl.className = "bad";
          }
        })
        .catch(err => console.log(err));
    }

    updateData();
    setInterval(updateData, 2000);
  </script>
</body>
</html>
)rawliteral";

#endif
