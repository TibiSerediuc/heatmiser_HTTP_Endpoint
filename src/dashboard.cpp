#include "Arduino.h"
#include "ArduinoJson.h"
#include "globals.h"

void handleDashboard() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Heatmiser Neo Dashboard</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            background-color: #000000;
            color: #f5f5f7;
            margin: 0;
            padding: 20px;
            min-height: 100vh;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            position: relative;
            z-index: 1;
        }

        #particles-canvas {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            z-index: 0;
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }

        .card {
            background-color: rgba(28, 28, 30, 0.95);
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 20px;
            backdrop-filter: blur(10px);
        }

        .zone-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
        }

        .status-indicator {
            display: inline-flex;
            align-items: center;
            gap: 8px;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
        }

        .status-dot.online { background-color: #4ade80; }
        .status-dot.offline { background-color: #ef4444; }

        .tabs {
            display: flex;
            gap: 20px;
            margin-bottom: 20px;
        }

        .tab {
            padding: 8px 16px;
            background: none;
            border: none;
            color: #f5f5f7;
            cursor: pointer;
            opacity: 0.7;
            transition: opacity 0.3s;
        }

        .tab.active {
            opacity: 1;
            border-bottom: 2px solid #0071e3;
        }

        .system-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
        }

        .progress-bar {
            width: 100%;
            height: 4px;
            background-color: #374151;
            border-radius: 2px;
            overflow: hidden;
        }

        .progress-value {
            height: 100%;
            background-color: #0071e3;
            transition: width 0.3s ease;
        }

        @media (max-width: 768px) {
            .zone-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <canvas id="particles-canvas"></canvas>
    <div class="container">
        <div class="header">
            <h1>Heatmiser Neo Dashboard</h1>
            <div class="status-indicator">
                <span>System Status</span>
                <div class="status-dot online"></div>
            </div>
        </div>

        <div class="tabs">
            <button class="tab active" data-tab="zones">Zones</button>
            <button class="tab" data-tab="system">System</button>
            <button class="tab" data-tab="settings">Settings</button>
        </div>

        <div id="zones-content" class="tab-content">
            <div class="zone-grid" id="zones-grid">
                <!-- Zones will be populated here -->
            </div>
        </div>

        <div id="system-content" class="tab-content" style="display: none;">
            <div class="card">
                <h2>System Status</h2>
                <div class="system-grid">
                    <div>
                        <h3>WiFi</h3>
                        <div class="status-indicator">
                            <span id="wifi-status">Connected</span>
                            <div class="status-dot online"></div>
                        </div>
                    </div>
                    <div>
                        <h3>WebSocket</h3>
                        <div class="status-indicator">
                            <span id="ws-status">Connected</span>
                            <div class="status-dot online"></div>
                        </div>
                    </div>
                    <div>
                        <h3>Memory Usage</h3>
                        <div class="progress-bar">
                            <div class="progress-value" id="memory-usage" style="width: 45%"></div>
                        </div>
                    </div>
                    <div>
                        <h3>Uptime</h3>
                        <span id="uptime">0d 0h 0m</span>
                    </div>
                </div>
            </div>
        </div>

        <div id="settings-content" class="tab-content" style="display: none;">
            <div class="card">
                <h2>Settings</h2>
                <form id="settings-form">
                    <!-- Settings form will be added here -->
                </form>
            </div>
        </div>
    </div>

    <script>
        // Particles animation
        const canvas = document.getElementById('particles-canvas');
        const ctx = canvas.getContext('2d');

        function resizeCanvas() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
        }

        window.addEventListener('resize', resizeCanvas);
        resizeCanvas();

        class Particle {
            constructor() {
                this.reset();
            }

            reset() {
                this.x = Math.random() * canvas.width;
                this.y = Math.random() * canvas.height;
                this.vx = (Math.random() - 0.5) * 0.5;
                this.vy = (Math.random() - 0.5) * 0.5;
                this.life = Math.random() * 100 + 100;
                this.maxLife = this.life;
                this.trail = [];
                this.trailLength = 20;
            }

            update() {
                this.trail.push({ x: this.x, y: this.y });
                if (this.trail.length > this.trailLength) {
                    this.trail.shift();
                }

                this.x += this.vx;
                this.y += this.vy;
                this.life--;

                if (this.life < 0 || this.x < 0 || this.x > canvas.width || this.y < 0 || this.y > canvas.height) {
                    this.reset();
                }
            }

            draw() {
                if (this.trail.length < 2) return;
                
                ctx.beginPath();
                ctx.moveTo(this.trail[0].x, this.trail[0].y);
                
                for (let i = 1; i < this.trail.length; i++) {
                    ctx.lineTo(this.trail[i].x, this.trail[i].y);
                }

                const gradient = ctx.createLinearGradient(
                    this.trail[0].x, this.trail[0].y,
                    this.trail[this.trail.length - 1].x, this.trail[this.trail.length - 1].y
                );

                const alpha = this.life / this.maxLife;
                gradient.addColorStop(0, `rgba(0, 113, 227, 0)`);
                gradient.addColorStop(1, `rgba(0, 113, 227, ${alpha * 0.3})`);
                
                ctx.strokeStyle = gradient;
                ctx.lineWidth = 2;
                ctx.lineCap = 'round';
                ctx.stroke();
            }
        }

        const particles = Array.from({ length: 50 }, () => new Particle());

        function animate() {
            ctx.fillStyle = 'rgba(0, 0, 0, 0.05)';
            ctx.fillRect(0, 0, canvas.width, canvas.height);

            particles.forEach(particle => {
                particle.update();
                particle.draw();
            });

            requestAnimationFrame(animate);
        }

        animate();

        // Dashboard functionality
        class Dashboard {
            constructor() {
                this.zones = new Map();
                this.systemStatus = {};
                this.setupEventListeners();
                this.startPolling();
            }

            setupEventListeners() {
                document.querySelectorAll('.tab').forEach(tab => {
                    tab.addEventListener('click', () => this.switchTab(tab.dataset.tab));
                });
            }

            switchTab(tabName) {
                document.querySelectorAll('.tab').forEach(tab => {
                    tab.classList.toggle('active', tab.dataset.tab === tabName);
                });
                
                document.querySelectorAll('.tab-content').forEach(content => {
                    content.style.display = content.id === `${tabName}-content` ? 'block' : 'none';
                });
            }

            createZoneCard(zone) {
                const card = document.createElement('div');
                card.className = 'card';
                card.innerHTML = `
                    <div style="display: flex; justify-content: space-between; align-items: center;">
                        <h2>${zone.name}</h2>
                        <div class="status-indicator">
                            <div class="status-dot ${zone.standby ? 'offline' : 'online'}"></div>
                        </div>
                    </div>
                    <div style="margin-top: 20px;">
                        <div style="font-size: 2em;">${zone.temperature.toFixed(1)}°C</div>
                        <div style="color: #666; font-size: 0.8em;">Last update: ${new Date().toLocaleTimeString()}</div>
                    </div>
                `;
                return card;
            }

            updateZones(zones) {
                const container = document.getElementById('zones-grid');
                container.innerHTML = '';
                zones.forEach(zone => {
                    container.appendChild(this.createZoneCard(zone));
                });
            }

            updateSystemStatus(status) {
                document.getElementById('wifi-status').textContent = status.wifi ? 'Connected' : 'Disconnected';
                document.getElementById('ws-status').textContent = status.websocket ? 'Connected' : 'Disconnected';
                document.getElementById('memory-usage').style.width = status.memory + '%';
                document.getElementById('uptime').textContent = this.formatUptime(status.uptime);
            }

            formatUptime(seconds) {
                const days = Math.floor(seconds / 86400);
                const hours = Math.floor((seconds % 86400) / 3600);
                const minutes = Math.floor((seconds % 3600) / 60);
                return `${days}d ${hours}h ${minutes}m`;
            }

            async fetchData() {
                try {
                    const [zonesResponse, statusResponse] = await Promise.all([
                        fetch('/api/zones'),
                        fetch('/api/status')
                    ]);

                    const zones = await zonesResponse.json();
                    const status = await statusResponse.json();

                    this.updateZones(zones);
                    this.updateSystemStatus(status);
                } catch (error) {
                    console.error('Error fetching data:', error);
                }
            }

            startPolling() {
                this.fetchData();
                setInterval(() => this.fetchData(), 30000);
            }
        }

        // Initialize dashboard
        const dashboard = new Dashboard();
    </script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

void handleGetZones() {
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& temp : temperatures) {
        JsonObject zone = array.createNestedObject();
        zone["name"] = temp.first;
        zone["temperature"] = temp.second;
        zone["standby"] = false; // You'll need to track this state
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetStatus() {
    DynamicJsonDocument doc(256);
    
    doc["wifi"] = WiFi.status() == WL_CONNECTED;
    doc["websocket"] = webSocket.isConnected();
    doc["memory"] = (ESP.getHeapSize() - ESP.getFreeHeap()) * 100 / ESP.getHeapSize();
    doc["uptime"] = millis() / 1000;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}
