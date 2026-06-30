const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const dgram = require('dgram');
const path = require('path');
const cors = require('cors');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
    cors: {
        origin: "*",
        methods: ["GET", "POST"]
    }
});

// Middleware
app.use(cors());
app.use(express.json());

// Serve static frontend files
app.use(express.static(path.join(__dirname, '../frontend')));

// --- State Variables ---
let currentTelemetry = {};
let currentBiometrics = {
    heartRate: 75,
    stressLevel: 'Normal',
    activeRole: 'Unknown'
};
let activeSabotages = [];

// --- Windshear State ---
let windshear = {
    active: false,
    intensity: 0,
    maxIntensity: 10,
    interval: null,
    step: 0.5, // Naik 0.5 poin biar lebih perlahan
    minAltCAPS: 2500,
    capsAuthorized: false,
    capsAvailableEmitted: false
};

// --- X-Plane UDP Client ---
const XP_IP = '127.0.0.1';
const XP_PORT = 49000;

function sendDref(dataref, value) {
    const buf = Buffer.alloc(509);
    buf.write('DREF\0', 0, 5, 'ascii');
    buf.writeFloatLE(value, 5);
    buf.write(dataref, 9, 'ascii');
    udpServer.send(buf, 0, buf.length, XP_PORT, XP_IP);
}

function sendCmnd(command) {
    const buf = Buffer.alloc(5 + command.length + 1);
    buf.write('CMND\0', 0, 5, 'ascii');
    buf.write(command, 5, 'ascii');
    udpServer.send(buf, 0, buf.length, XP_PORT, XP_IP);
}

// --- WebSocket (Socket.IO) Setup ---
io.on('connection', (socket) => {
    console.log(`[WS] Client connected: ${socket.id}`);
    
    // Send initial state
    socket.emit('telemetry_update', currentTelemetry);
    socket.emit('biometrics_update', currentBiometrics);
    socket.emit('sabotage_update', activeSabotages);

    // Listen for Sabotage events from IOS
    socket.on('trigger_sabotage', (data) => {
        console.log(`[SABOTAGE] Triggered: ${data.type}`);
        if (!activeSabotages.includes(data.type)) {
            activeSabotages.push(data.type);
            io.emit('sabotage_update', activeSabotages);
            
            if (data.type === 'WINDSHEAR') {
                windshear.active = true;
                windshear.intensity = 0;
                windshear.capsAuthorized = false;
                windshear.capsAvailableEmitted = false;
                if (windshear.interval) clearInterval(windshear.interval);
                
                // Delay 5s before starting
                setTimeout(() => {
                    if (!windshear.active) return;
                    windshear.interval = setInterval(() => {
                        if (windshear.intensity < windshear.maxIntensity) {
                            windshear.intensity += windshear.step;
                            if (windshear.intensity > windshear.maxIntensity) windshear.intensity = windshear.maxIntensity;
                            
                            // Send to X-Plane (Turbulence & Shear)
                            sendDref('sim/weather/region/turbulence[0]', windshear.intensity);
                            sendDref('sim/weather/region/turbulence[1]', windshear.intensity);
                            sendDref('sim/weather/region/turbulence[2]', windshear.intensity);
                            sendDref('sim/weather/region/shear_speed_msc[0]', windshear.intensity * 3); // up to 30 m/s
                            
                            // Broadcast intensity to IOS
                            io.emit('windshear_intensity', windshear.intensity);
                        }
                    }, 5000); // Eksekusi setiap 5 detik (mengurangi beban GPU rendering awan)
                }, 5000);
            }
        }
    });

    socket.on('clear_sabotage', (data) => {
        console.log(`[SABOTAGE] Cleared: ${data.type}`);
        activeSabotages = activeSabotages.filter(s => s !== data.type);
        io.emit('sabotage_update', activeSabotages);
        
        if (data.type === 'WINDSHEAR') {
            windshear.active = false;
            windshear.intensity = 0;
            windshear.capsAuthorized = false;
            windshear.capsAvailableEmitted = false;
            if (windshear.interval) clearInterval(windshear.interval);
            
            // Reset X-Plane weather
            sendDref('sim/weather/region/turbulence[0]', 0.0);
            sendDref('sim/weather/region/turbulence[1]', 0.0);
            sendDref('sim/weather/region/turbulence[2]', 0.0);
            sendDref('sim/weather/region/shear_speed_msc[0]', 0.0);
            
            io.emit('windshear_intensity', 0);
            // Reset CAPS custom slider
            sendDref('sim/cockpit2/switches/custom_slider_1', 0.0);
        }
    });

    socket.on('authorize_caps', () => {
        console.log(`[CAPS] Deployment Authorized by IOS`);
        windshear.capsAuthorized = true;
        // Write to custom dataref so SimHub can trigger CAPS button
        sendDref('sim/cockpit2/switches/custom_slider_1', 1.0);
        io.emit('caps_authorized_status', true);
    });

    socket.on('disconnect', () => {
        console.log(`[WS] Client disconnected: ${socket.id}`);
    });
});

// --- REST API for Biometrics (Jetson Orin NX) ---
app.post('/api/biometrics', (req, res) => {
    const data = req.body;
    console.log(`[REST] Received biometrics:`, data);
    
    if (data.heartRate) currentBiometrics.heartRate = data.heartRate;
    if (data.stressLevel) currentBiometrics.stressLevel = data.stressLevel;
    if (data.activeRole) currentBiometrics.activeRole = data.activeRole;

    // Broadcast to dashboards
    io.emit('biometrics_update', currentBiometrics);
    res.status(200).json({ status: 'success', message: 'Biometrics updated' });
});

// --- UDP Listener for Telemetry ---
const udpServer = dgram.createSocket('udp4');
const UDP_PORT = 4445;

udpServer.on('error', (err) => {
    console.error(`[UDP] Server error:\n${err.stack}`);
    udpServer.close();
});

udpServer.on('message', (msg, rinfo) => {
    // Parse ASCII string from X-Plane C++ plugin
    // Format: RPM:1500.0;SPD:180.50;FUEL:85.0;...
    const strMsg = msg.toString('utf-8');
    const pairs = strMsg.split(';');
    
    let parsedData = {};
    pairs.forEach(pair => {
        if (pair) {
            const [key, value] = pair.split(':');
            if (key && value !== undefined) {
                // Try to parse as float if possible, otherwise keep string
                const numVal = parseFloat(value);
                parsedData[key] = isNaN(numVal) ? value : numVal;
            }
        }
    });

    currentTelemetry = parsedData;
    
    // Broadcast via WebSockets
    io.emit('telemetry_update', currentTelemetry);
    
    // Check CAPS condition
    if (windshear.active && currentTelemetry.ALT < windshear.minAltCAPS && !windshear.capsAuthorized && !windshear.capsAvailableEmitted) {
        windshear.capsAvailableEmitted = true;
        // Emit caps available to IOS
        io.emit('caps_available', true);
    }
});

udpServer.on('listening', () => {
    const address = udpServer.address();
    console.log(`[UDP] Listening for telemetry on ${address.address}:${address.port}`);
});

udpServer.bind(UDP_PORT);

// --- Start HTTP Server ---
const HTTP_PORT = process.env.PORT || 3000;
server.listen(HTTP_PORT, () => {
    console.log(`[HTTP] Server running on http://localhost:${HTTP_PORT}`);
});
