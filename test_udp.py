"""
Test UDP Listener untuk X-Plane Motion Telemetry Plugin
Mendengarkan Port 4444 (FlyPT/Motion) dan Port 4445 (SimHub/Instrument)
Jalankan skrip ini SEBELUM/SETELAH membuka X-Plane 12.
"""

import socket
import struct
import threading
import sys
import os

# Fix encoding untuk terminal Windows
sys.stdout.reconfigure(encoding='utf-8', errors='replace') if hasattr(sys.stdout, 'reconfigure') else None

# Warna untuk terminal (ANSI)
GREEN  = "\033[92m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RED    = "\033[91m"
RESET  = "\033[0m"
BOLD   = "\033[1m"

received_4444 = False
received_4445 = False

def listen_motion(port=4444):
    """Mendengarkan data Motion (FlyPT) di port 4444 - format 6 float binary"""
    global received_4444
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(1.0)
    print(f"{CYAN}[PORT {port}]{RESET} Mendengarkan data Motion (FlyPT)... {YELLOW}Tunggu X-Plane dibuka!{RESET}")
    
    count = 0
    while True:
        try:
            data, addr = sock.recvfrom(4096)
            if not received_4444:
                print(f"\n{GREEN}{BOLD}[OK] [PORT {port}] Koneksi berhasil! Data diterima dari {addr}{RESET}")
                received_4444 = True
            
            # Parse 9 floats: Pitch, Roll, Yaw, Surge, Sway, Heave, GN, GA, GS
            if len(data) >= 36:
                vals = struct.unpack('9f', data[:36])
                labels = ['Pitch', 'Roll', 'Yaw', 'Surge', 'Sway', 'Heave', 'GN', 'GA', 'GS']
                count += 1
                if count % 50 == 0:
                    print(f"{CYAN}[MOTION]{RESET}  ", end="")
                    for l, v in zip(labels, vals):
                        print(f"{l}:{YELLOW}{v:6.2f}{RESET}", end="  ")
                    print()
        except socket.timeout:
            continue
        except Exception as e:
            print(f"{RED}[PORT {port}] Error: {e}{RESET}")
            break

def listen_simhub(port=4445):
    """Mendengarkan data Instrument (SimHub) di port 4445 - format string ASCII"""
    global received_4445
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    sock.settimeout(1.0)
    print(f"{CYAN}[PORT {port}]{RESET} Mendengarkan data Instrument (SimHub)... {YELLOW}Tunggu X-Plane dibuka!{RESET}")
    
    count = 0
    while True:
        try:
            data, addr = sock.recvfrom(4096)
            if not received_4445:
                print(f"\n{GREEN}{BOLD}[OK] [PORT {port}] Koneksi berhasil! Data diterima dari {addr}{RESET}")
                received_4445 = True
            
            count += 1
            if count % 50 == 0:  # Print setiap 50 packet (~0.5 detik)
                msg = data.decode('ascii', errors='ignore')
                print(f"{GREEN}[SIMHUB]{RESET}  {msg}")
        except socket.timeout:
            continue
        except Exception as e:
            print(f"{RED}[PORT {port}] Error: {e}{RESET}")
            break

if __name__ == '__main__':
    os.system('')  # Enable ANSI colors on Windows
    
    print(f"""
{BOLD}{'='*60}{RESET}
{BOLD}  X-Plane 12 - UDP Telemetry Test Listener{RESET}
{BOLD}{'='*60}{RESET}
  Port 4444 -> Motion Data   (FlyPT Mover / Hexapod)
  Port 4445 -> Instrument Data (SimHub / Dashboard IOS)
{BOLD}{'='*60}{RESET}
  Pesawat: Cirrus Vision SF50
  Plugin : MotionTelemetry (win.xpl)
{BOLD}{'='*60}{RESET}
""")
    
    # Jalankan dua thread listener secara paralel
    t1 = threading.Thread(target=listen_motion, args=(4444,), daemon=True)
    t2 = threading.Thread(target=listen_simhub, args=(4445,), daemon=True)
    
    t1.start()
    t2.start()
    
    print(f"\n{YELLOW}[INSTRUKSI]{RESET}")
    print(f"  1. Buka X-Plane 12")
    print(f"  2. Load pesawat Cirrus Vision SF50")
    print(f"  3. Data akan muncul di sini secara otomatis")
    print(f"  4. Tekan Ctrl+C untuk keluar\n")
    
    try:
        t1.join()
        t2.join()
    except KeyboardInterrupt:
        print(f"\n{YELLOW}[INFO] Test dihentikan.{RESET}")
        if received_4444 and received_4445:
            print(f"{GREEN}{BOLD}[SUKSES] Kedua port berfungsi normal.{RESET}")
        elif received_4444:
            print(f"{YELLOW}[WARN] Hanya port 4444 yang menerima data. Cek VSNGPlugin di SimHub.{RESET}")
        elif received_4445:
            print(f"{YELLOW}[WARN] Hanya port 4445 yang menerima data. Cek koneksi FlyPT.{RESET}")
        else:
            print(f"{RED}[GAGAL] Tidak ada data diterima. Pastikan:{RESET}")
            print(f"   - Plugin win.xpl ada di folder plugins X-Plane")
            print(f"   - X-Plane sudah dibuka dan pesawat sudah di-load")
            print(f"   - Tidak ada firewall yang memblokir UDP")
