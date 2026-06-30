import socket
import time
import math
import urllib.request
import json
import threading

# Configuration
UDP_IP = "127.0.0.1"
UDP_PORT = 4445
HTTP_URL = "http://localhost:3000/api/biometrics"

print(f"UDP Target IP: {UDP_IP}")
print(f"UDP Target Port: {UDP_PORT}")
print(f"HTTP Biometrics URL: {HTTP_URL}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def simulate_telemetry():
    t = 0.0
    while True:
        # Simulate some dynamic telemetry
        rpm = 1500.0 + (math.sin(t) * 100.0)
        spd = 180.0 + (math.sin(t/2.0) * 10.0)
        alt = 5000.0 + (math.cos(t/5.0) * 50.0)
        fuel = max(0, 85.0 - (t / 10.0))
        hdg = (t * 5) % 360
        
        pitch = math.sin(t) * 5.0
        roll = math.cos(t) * 15.0
        g = 1.0 + math.sin(t*2.0) * 0.5
        
        stall = 1 if pitch > 4.5 else 0

        # Construct X-Plane C++ format string
        msg = f"RPM:{rpm:.1f};SPD:{spd:.2f};ALT:{alt:.0f};FUEL:{fuel:.1f};HDG:{hdg:.1f};"
        msg += f"PTCH:{pitch:.2f};ROLL:{roll:.2f};GN:{g:.2f};STALL:{stall};"
        msg += "WSPD:12.5;WDIR:270;TEMP:14.2;TURB:1.5;DMN:Udara;"
        
        sock.sendto(msg.encode(), (UDP_IP, UDP_PORT))
        
        t += 0.1
        time.sleep(0.1) # 10Hz

def simulate_biometrics():
    roles = ["Captain", "First Officer", "Flight Engineer", "Unknown"]
    t = 0
    while True:
        try:
            hr = int(75 + math.sin(t/3.0) * 20 + (t % 10))
            stress = "Normal"
            if hr > 90:
                stress = "HIGH"
            if hr > 100:
                stress = "CRITICAL"
                
            role = roles[(t // 15) % len(roles)] # Change role every 15 iterations

            payload = {
                "heartRate": hr,
                "stressLevel": stress,
                "activeRole": role
            }
            
            data = json.dumps(payload).encode('utf-8')
            req = urllib.request.Request(HTTP_URL, data=data, headers={'Content-Type': 'application/json'})
            urllib.request.urlopen(req)
            print(f"[Bio] Sent: HR={hr}, Stress={stress}, Role={role}")
        except Exception as e:
            print(f"[Bio] Failed to send: {e}")
        
        t += 1
        time.sleep(2.0) # 0.5Hz

if __name__ == "__main__":
    t1 = threading.Thread(target=simulate_telemetry)
    t2 = threading.Thread(target=simulate_biometrics)
    
    t1.daemon = True
    t2.daemon = True
    
    print("Starting simulation threads...")
    t1.start()
    t2.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Simulation stopped.")
