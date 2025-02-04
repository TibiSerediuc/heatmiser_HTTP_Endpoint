#include <EEPROM.h>
#include "globals.h"

// Add this function to scan for networks
void handleScanNetworks()
{
    int n = WiFi.scanNetworks();
    String networks = "[";
    for (int i = 0; i < n; ++i)
    {
        if (i > 0)
            networks += ",";
        networks += "{\"ssid\":\"" + WiFi.SSID(i) + "\"}";
    }
    networks += "]";
    server.send(200, "application/json", networks);
}

void handleRoot()
{
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Heatmiser Neo Setup</title>
        <style>
            body {
                font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
                background-color: #000000;
                color: #f5f5f7;
                display: flex;
                justify-content: center;
                align-items: center;
                height: 100vh;
                margin: 0;
                padding: 0;
                overflow: hidden;
            }
            #particles-canvas {
                position: fixed;
                top: 0;
                left: 0;
                width: 100%;
                height: 100%;
                z-index: 1;
            }
            .container {
                background-color: rgba(28, 28, 30, 0.95);
                border-radius: 18px;
                padding: 30px;
                width: 320px;
                max-width: 100%;
                box-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
                position: relative;
                z-index: 2;
                backdrop-filter: blur(10px);
            }
            h1 {
                text-align: center;
                color: #ffffff;
                font-weight: 600;
                font-size: 24px;
                margin-bottom: 20px;
            }
            form {
                display: flex;
                flex-direction: column;
            }
            label {
                margin-top: 15px;
                color: #86868b;
                font-size: 14px;
            }
            input, select {
                padding: 12px;
                background-color: rgba(44, 44, 46, 0.8);
                border: none;
                border-radius: 8px;
                font-size: 16px;
                color: #ffffff;
                transition: background-color 0.3s;
                height: 45px;
                box-sizing: border-box;
            }
            input:focus, select:focus {
                background-color: rgba(58, 58, 60, 0.9);
                outline: none;
            }
            input::placeholder {
                color: #86868b;
            }
            /* Override browser validation styles */
            input:valid,
            input:invalid {
                background-color: rgba(44, 44, 46, 0.8) !important;
                color: #ffffff !important;
                box-shadow: none !important;
            }
            input:-webkit-autofill,
            input:-webkit-autofill:hover,
            input:-webkit-autofill:focus,
            input:-webkit-autofill:active {
                -webkit-box-shadow: 0 0 0 30px rgb(44, 44, 46) inset !important;
                -webkit-text-fill-color: #ffffff !important;
            }
            button {
                margin-top: 25px;
                padding: 12px 24px;
                background-color: #0071e3;
                color: #ffffff;
                border: none;
                border-radius: 8px;
                font-size: 16px;
                font-weight: 600;
                cursor: pointer;
                transition: all 0.3s;
                position: relative;
                overflow: hidden;
            }
            button:hover {
                background-color: #0077ed;
                transform: translateY(-1px);
                box-shadow: 0 4px 12px rgba(0, 113, 227, 0.3);
            }
            
            .submit-button {
                min-width: 150px;
                display: inline-flex;
                align-items: center;
                justify-content: center;
            }

            .submit-button.loading {
                background-color: #005bc1;
                cursor: wait;
                transform: none;
                box-shadow: none;
            }

            .submit-button.loading .button-text {
                visibility: hidden;
            }

            .submit-button.loading::after {
                content: "";
                position: absolute;
                width: 20px;
                height: 20px;
                border: 2px solid rgba(255, 255, 255, 0.3);
                border-radius: 50%;
                border-top-color: #fff;
                animation: submit-spinner 0.6s linear infinite;
            }

            @keyframes submit-spinner {
                to {
                    transform: rotate(360deg);
                }
            }
            .refresh-btn {
                background-color: rgba(44, 44, 46, 0.8);
                width: 45px;
                height: 45px;
                padding: 0;
                margin: 0;
                line-height: 0;
                display: flex;
                align-items: center;
                justify-content: center;
                border-radius: 8px;
                flex-shrink: 0;
                margin-top: 0;
            }
            .refresh-btn:hover {
                background-color: rgba(58, 58, 60, 0.9);
            }
            .network-row {
                display: flex;
                align-items: center;
                gap: 8px;
                margin-top: 5px;
            }
            #wifi_ssid {
                flex-grow: 1;
                margin: 0;
            }
            .status-message {
                display: flex;
                align-items: center;
                justify-content: center;
                color: #86868b;
                font-size: 14px;
                margin-top: 8px;
                height: 20px;
            }
            .icon {
                width: 10px;
                height: 10px;
                margin-left: 6px;
                display: inline-flex;
                align-items: center;
                justify-content: center;
            }
            .loading-icon {
                border: 1px solid #86868b;
                border-top: 1px solid #0071e3;
                border-radius: 50%;
                animation: spin 1s linear infinite;
            }
            .success-icon {
                color: #30d158;
                opacity: 0;
                transition: opacity 0.3s;
            }
            @keyframes spin {
                0% { transform: rotate(0deg); }
                100% { transform: rotate(360deg); }
            }
            @keyframes morph {
                0% { clip-path: circle(50%); }
                50% { clip-path: circle(0%); }
                51% { clip-path: none; }
                100% { clip-path: none; }
            }
        </style>
    </head>
    <body>
        <canvas id="particles-canvas"></canvas>
        <div class="container">
            <h1>Heatmiser Neo Setup</h1>
            <form action="/configure" method="post">
                <label for="wifi_ssid">WiFi Network:</label>
                <div class="network-row">
                    <select name="wifi_ssid" id="wifi_ssid" required>
                        <option value="">Select network...</option>
                    </select>
                    <button type="button" class="refresh-btn" onclick="scanNetworks()">
                        <svg width="16" height="16" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
                            <path d="M13.666 2.334A7.956 7.956 0 0 0 8 0C3.582 0 0 3.582 0 8s3.582 8 8 8c3.866 0 7.074-2.746 7.82-6.4h-2.154c-.686 2.374-2.848 4.1-5.666 4.1-3.128 0-5.666-2.538-5.666-5.7 0-3.162 2.538-5.7 5.666-5.7 1.566 0 2.986.634 4.02 1.66L9.334 7H16V.334l-2.334 2z" fill="currentColor"/>
                        </svg>
                    </button>
                </div>
                <div id="scanning" class="status-message" style="display: none;">
                    <span>Scanning networks</span>
                    <span class="icon loading-icon"></span>
                </div>
                
                <label for="wifi_password">WiFi Password:</label>
                <input type="password" id="wifi_password" name="wifi_password" required>
                
                <label for="heatmiser_ip">Heatmiser IP Address:</label>
                <input type="text" id="heatmiser_ip" name="heatmiser_ip" placeholder="e.g., 192.168.1.100" required>
                
                <label for="api_key">Heatmiser API Key:</label>
                <input type="text" id="api_key" name="api_key" required>
                
                <button type="submit" class="submit-button">
                    <span class="button-text">Save Configuration</span>
                </button>
            </form>
        </div>
        <script>
            // Particles animation
            const canvas = document.getElementById('particles-canvas');
            const ctx = canvas.getContext('2d');

            // Set canvas size
            function resizeCanvas() {
                canvas.width = window.innerWidth;
                canvas.height = window.innerHeight;
            }
            resizeCanvas();
            window.addEventListener('resize', resizeCanvas);

            // Particle class
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

                    if (this.life < 0 || 
                        this.x < 0 || 
                        this.x > canvas.width || 
                        this.y < 0 || 
                        this.y > canvas.height) {
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

            // Create particles
            const particles = Array.from({ length: 50 }, () => new Particle());

            // Animation loop
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

            // Original network scanning functionality
            function scanNetworks() {
                const select = document.getElementById('wifi_ssid');
                const scanning = document.getElementById('scanning');
                const icon = scanning.querySelector('.icon');
                
                icon.className = 'icon';
                icon.style.animation = '';
                icon.innerHTML = '';
                
                scanning.style.display = 'flex';
                icon.className = 'icon loading-icon';
                scanning.querySelector('span').textContent = 'Scanning networks';
                select.disabled = true;
                
                fetch('/scan')
                    .then(response => response.json())
                    .then(networks => {
                        while (select.options.length > 1) {
                            select.remove(1);
                        }
                        
                        networks.forEach(network => {
                            const option = new Option(network.ssid, network.ssid);
                            select.add(option);
                        });
                        
                        icon.style.animation = 'morph 0.3s linear forwards';
                        setTimeout(() => {
                            icon.className = 'icon success-icon';
                            icon.innerHTML = '✓';
                            icon.style.opacity = '1';
                            scanning.querySelector('span').textContent = 'Scan complete';
                        }, 300);
                        
                        setTimeout(() => {
                            scanning.style.display = 'none';
                        }, 2000);
                    })
                    .catch(error => {
                        console.error('Error scanning networks:', error);
                        scanning.querySelector('span').textContent = 'Scan failed';
                        icon.className = 'icon';
                        icon.innerHTML = '<span style="color: #ff453a;">✕</span>';
                    })
                    .finally(() => {
                        select.disabled = false;
                    });
            }
            
            // Form submission handling
            document.querySelector('form').addEventListener('submit', async function(e) {
                e.preventDefault();
                const submitButton = document.querySelector('.submit-button');
                submitButton.classList.add('loading');
                submitButton.querySelector('.button-text').textContent = 'Saving...';
                
                try {
                    const formData = new FormData(this);
                    const response = await fetch('/configure', {
                        method: 'POST',
                        body: formData
                    });
                    
                    const text = await response.text();
                    
                    if (!response.ok) {
                        throw new Error(text || 'Configuration failed');
                    }
                    
                    alert(text);
                    
                    // Show countdown
                    let countdown = 5;
                    const interval = setInterval(() => {
                        submitButton.querySelector('.button-text').textContent = `Restarting in ${countdown}s...`;
                        countdown--;
                        if (countdown < 0) {
                            clearInterval(interval);
                            window.location.href = '/';
                        }
                    }, 1000);
                    
                } catch (error) {
                    console.error('Error:', error);
                    alert(error.message);
                    submitButton.classList.remove('loading');
                    submitButton.querySelector('.button-text').textContent = 'Save Configuration';
                }
            });

            window.onload = scanNetworks;
        </script>
    </body>
    </html>)rawliteral";

    server.send(200, "text/html", html);
}

void saveConfig()
{
    EEPROM.put(0, config);
    EEPROM.commit();
}

void loadConfig()
{
    EEPROM.get(0, config);

    Serial.println("Loaded configuration:");
    Serial.println("SSID: " + String(config.wifi_ssid));
    Serial.println("IP: " + String(config.heatmiser_ip));
    Serial.println("Is Configured: " + String(config.isConfigured));

    if (config.wifi_ssid[0] == 0xFF)
    {
        memset(&config, 0, sizeof(config));
        config.isConfigured = false;
        saveConfig();
    }
}

void handleConfigure()
{
    if (server.hasArg("wifi_ssid") && server.hasArg("wifi_password") &&
        server.hasArg("heatmiser_ip") && server.hasArg("api_key"))
    {

        strncpy(config.wifi_ssid, server.arg("wifi_ssid").c_str(), sizeof(config.wifi_ssid) - 1);
        strncpy(config.wifi_password, server.arg("wifi_password").c_str(), sizeof(config.wifi_password) - 1);
        strncpy(config.heatmiser_ip, server.arg("heatmiser_ip").c_str(), sizeof(config.heatmiser_ip) - 1);
        strncpy(config.api_key, server.arg("api_key").c_str(), sizeof(config.api_key) - 1);

        config.isConfigured = true;
        saveConfig();

        server.send(200, "text/plain", "Configuration saved. Device will restart...");
        delay(2000);
        ESP.restart();
    }
    else
    {
        String debug = "Missing parameters. Received:\n";
        for (int i = 0; i < server.args(); i++)
        {
            debug += server.argName(i) + ": " + server.arg(i) + "\n";
        }
        server.send(400, "text/plain", debug);
    }
}

void startConfigMode()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    // Configure captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/configure", HTTP_POST, handleConfigure);
    server.on("/scan", HTTP_GET, handleScanNetworks);

    // Add handler for captive portal
    server.onNotFound([]()
                      {
    server.sendHeader("Location", "http://192.168.4.1", true);
    server.send(302, "text/plain", ""); });

    server.begin();

    configModeStartTime = millis();
    Serial.println("Configuration mode started");
    Serial.println("Connect to WiFi network: " + String(AP_SSID));
    Serial.println("Then access: http://192.168.4.1");
}