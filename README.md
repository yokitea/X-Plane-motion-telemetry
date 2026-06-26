# X-Plane Motion Telemetry Plugin

Plugin C++ untuk **X-Plane 12** yang mengekstrak data telemetri penerbangan secara *real-time* dan mengirimkannya via UDP ke **FlyPT Mover** (platform 6DOF) dan **SimHub** (instrument dashboard / IOS).

> Dibuat untuk **LUP Flight Simulator** — Proyek Simulator Penerbangan Komersial.

---

## Arsitektur Data

```
X-Plane 12 (Cirrus Vision SF50, dll)
    │
    └── Plugin: MotionTelemetry (win.xpl)
            │
            ├── UDP Port 4444 ──► FlyPT Mover ──► Thanos AMC ──► Hexapod 6DOF
            │   [6 float binary: Pitch, Roll, Yaw, Surge, Sway, Heave]
            │
            └── UDP Port 4445 ──► VSNGPlugin.dll (SimHub) ──► WebSocket ──► IOS Server
                [ASCII string: RPM, Speed, Fuel, Heading, dll]
```

---

## File & Struktur

| File | Deskripsi |
|---|---|
| `xplane_telemetry.cpp` | Source code plugin C++ (XPLM API) |
| `build.bat` | Skrip kompilasi ke `win.xpl` |
| `win.xpl` | Plugin yang sudah dikompilasi (siap pakai) |
| `test_udp.py` | Skrip Python untuk tes penerimaan data UDP |
| `SDK/` | X-Plane Plugin SDK (CHeaders + Libraries) |

---

## Cara Install Plugin ke X-Plane 12

1. Buat folder plugin di X-Plane:
   ```
   E:\Games\XP12\Resources\plugins\MotionTelemetry\64\
   ```
2. Copy `win.xpl` ke dalam folder tersebut.
3. **Atau** jalankan `build.bat` — script ini akan otomatis compile & copy plugin ke lokasi yang benar.
4. Buka X-Plane 12. Plugin aktif otomatis begitu pesawat di-load.

---

## Data yang Dikirim

### Port 4444 — Motion Data (FlyPT Mover)
Format: **6 x float binary (24 bytes, Little Endian)**

| Offset | Nama | DataRef X-Plane | Satuan |
|---|---|---|---|
| 0 | Pitch | `sim/flightmodel/position/theta` | Derajat |
| 4 | Roll | `sim/flightmodel/position/phi` | Derajat |
| 8 | Yaw | `sim/flightmodel/position/psi` | Derajat |
| 12 | Surge | `sim/flightmodel/position/local_vz` | m/s |
| 16 | Sway | `sim/flightmodel/position/local_vx` | m/s |
| 20 | Heave | `sim/flightmodel/position/local_vy` | m/s |

### Port 4445 — Instrument Data (SimHub / IOS)
Format: **ASCII String**
```
RPM:1500.0;SPD:180.50;FUEL:85.0;HDG:270.0;ENG:1;LGT:0;STALL:0;ROLL:2.5;PTCH:-1.2;HEAV:0.3;DMN:Udara;
```

| Key | Nama | Keterangan |
|---|---|---|
| `RPM` | Engine N1 | Persen (%) |
| `SPD` | Groundspeed | Knots |
| `FUEL` | Fuel Quantity | Kg |
| `HDG` | Heading | Derajat (0-360) |
| `ENG` | Engine On | 1=Hidup, 0=Mati |
| `LGT` | Light On | 1=Nyala, 0=Mati |
| `STALL` | Stall | 1=Stall, 0=Normal |
| `ROLL` | Roll | Derajat |
| `PTCH` | Pitch | Derajat |
| `HEAV` | Heave | m/s |
| `DMN` | Domain | `Udara` (fixed) |

---

## Setting FlyPT Mover

1. Tambah **Source → UDP Custom**
   - IP: `127.0.0.1`, Port: `4444`
   - Data Type: `Float (4 bytes)`, Little Endian
2. Mapping Pose Axis:
   - Pitch → Float[0], Roll → Float[1], Yaw → Float[2]
   - Surge → Float[3], Sway → Float[4], Heave → Float[5]
3. Tuning yang disarankan untuk **Cirrus SF50**:
   - Low Pass Filter: `0.1 – 0.3`
   - Gain Motion: mulai dari `1.0` (Pitch/Roll), `0.5` (Surge/Sway/Heave)

---

## Setting SimHub

SimHub menggunakan **VSNGPlugin.dll** yang di-reuse dari proyek VSNG (Multi-domain: Laut & Udara).

Properti yang tersedia di SimHub:
- `VSNGPlugin RPM`, `VSNGPlugin Speed_Knots`, `VSNGPlugin Fuel`
- `VSNGPlugin Heading`, `VSNGPlugin Roll`, `VSNGPlugin Pitch`, `VSNGPlugin Heave`
- `VSNGPlugin Domain` → bernilai `Udara` saat terhubung dari X-Plane
- `VSNGPlugin Connected` → `1` jika data aktif mengalir

---

## Cara Build (Kompilasi Ulang)

Pastikan [LLVM/MinGW](https://github.com/mstorsjo/llvm-mingw) sudah terinstall, lalu jalankan:

```batch
cd E:\X-Plane-motion-telemetry
.\build.bat
```

Output: `win.xpl` → otomatis di-copy ke `E:\Games\XP12\Resources\plugins\MotionTelemetry\64\`

---

## Tes Koneksi

Jalankan skrip listener sebelum/sesudah membuka X-Plane:

```bash
python test_udp.py
```

Kalau data masuk → terminal akan menampilkan nilai Pitch, Roll, Yaw, dll secara live.

---

## Dependencies

- X-Plane 12.4+
- X-Plane Plugin SDK 4.0 (sudah di-clone di folder `SDK/`)
- LLVM-MinGW (compiler C++)
- Python 3.x (hanya untuk `test_udp.py`)

---

## Pesawat yang Telah Diuji

| Pesawat | Status |
|---|---|
| Cirrus Vision SF50 (bawaan XP12) | ✅ Terverifikasi |

---

*Bagian dari proyek **LUP Simulator** — Flight, Marine & Ground Vehicle Telemetry Platform.*
