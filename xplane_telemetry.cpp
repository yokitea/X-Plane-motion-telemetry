#include <winsock2.h>
#include <ws2tcpip.h>
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#pragma comment(lib, "ws2_32.lib")

// ============================================================
//  LUP Flight Simulator - X-Plane 12 Motion Telemetry Plugin
//  Port 4444 -> FlyPT Mover / Hexapod 6DOF (binary float)
//  Port 4445 -> SimHub / IOS Dashboard (ASCII string)
//
//  v2.0 - Tambah G-Force, Turbulence, AoA, Weather DataRefs
//         (Kompatibel X-Plane 12.4+)
// ============================================================

// ---- Motion Packet (Port 4444 → FlyPT Mover) ----
// 6 float: Pitch/Roll/Yaw + G-Force Longitudinal/Lateral/Vertical
// FlyPT setting:
//   Float[0] Pitch       → Angular position
//   Float[1] Roll        → Angular position
//   Float[2] Yaw         → Angular position
//   Float[3] Longitudinal → Acceleration without gravity (gforce_axil)
//   Float[4] Lateral      → Acceleration without gravity (gforce_side)
//   Float[5] Vertical     → Acceleration with gravity    (gforce_normal)
struct MotionPacket {
    float pitch;           // degrees
    float roll;            // degrees
    float yaw;             // degrees
    float longitudinal;    // G (akselerasi maju/mundur, tanpa gravitasi)
    float lateral;         // G (akselerasi kiri/kanan, tanpa gravitasi)
    float vertical;        // G (akselerasi naik/turun, dengan gravitasi)
};

// ---- DataRef Handles ----

// Attitude
XPLMDataRef phi_ref;        // Roll (deg)
XPLMDataRef theta_ref;      // Pitch (deg)
XPLMDataRef psi_ref;        // True Heading/Yaw (deg)

// G-Force (XP12.4 flightmodel2)
XPLMDataRef gforce_normal_ref;  // Vertical   G (dengan gravitasi)
XPLMDataRef gforce_axil_ref;    // Axial      G (tanpa gravitasi)
XPLMDataRef gforce_side_ref;    // Lateral    G (tanpa gravitasi)

// Flight Performance
XPLMDataRef alpha_ref;      // Angle of Attack (deg)
XPLMDataRef rpm_ref;        // Engine N1 (% array)
XPLMDataRef speed_ref;      // Groundspeed (m/s)
XPLMDataRef ias_ref;        // Indicated Airspeed (kts)
XPLMDataRef alt_ref;        // Indicated Altitude (ft/m)
XPLMDataRef vvi_ref;        // Vertical Speed (ft/min)
XPLMDataRef fuel_ref;       // Total Fuel Quantity (kg)

// Weather at Aircraft Position
XPLMDataRef turbulence_ref;   // Turbulence factor 0-10
XPLMDataRef wind_speed_ref;   // Wind speed (m/s)
XPLMDataRef wind_dir_ref;     // Wind direction (deg)
XPLMDataRef precip_ref;       // Precipitation ratio (0-1)
XPLMDataRef visibility_ref;   // Visibility (statute miles)
XPLMDataRef temp_ref;         // OAT - Outside Air Temp (degC)

// UDP
SOCKET udp_socket;
struct sockaddr_in flypt_addr;
struct sockaddr_in simhub_addr;

// ---- Flight Loop Callback (100Hz) ----
float MyFlightLoopCallback(float inElapsedSinceLastCall,
                           float inElapsedTimeSinceLastFlightLoop,
                           int inCounter, void *inRefcon)
{
    // == Attitude ==
    float roll  = XPLMGetDataf(phi_ref);
    float pitch = XPLMGetDataf(theta_ref);
    float yaw   = XPLMGetDataf(psi_ref);

    // == G-Force (langsung sebagai Longitudinal/Lateral/Vertical) ==
    // Dibagi 9.81 untuk konversi m/s² → G unit (1.0 = 1G)
    float gn = XPLMGetDataf(gforce_normal_ref) / 9.81f; // Vertical     (w/ gravity)
    float ga = XPLMGetDataf(gforce_axil_ref)   / 9.81f; // Longitudinal (w/o gravity)
    float gs = XPLMGetDataf(gforce_side_ref)   / 9.81f; // Lateral      (w/o gravity)

    // == Kirim ke FlyPT (6 float = 24 bytes) ==
    MotionPacket m_packet;
    m_packet.pitch        = pitch;
    m_packet.roll         = roll;
    m_packet.yaw          = yaw;
    m_packet.longitudinal = ga;  // Acceleration w/o gravity
    m_packet.lateral      = gs;  // Acceleration w/o gravity
    m_packet.vertical     = gn;  // Acceleration with gravity

    sendto(udp_socket, (const char*)&m_packet, sizeof(m_packet), 0,
           (struct sockaddr*)&flypt_addr, sizeof(flypt_addr));

    // == Instrument Data ==
    float rpm = 0.0f;
    XPLMGetDatavf(rpm_ref, &rpm, 0, 1);

    float speed_ms    = XPLMGetDataf(speed_ref);
    float speed_knots = speed_ms * 1.94384f;
    float ias         = XPLMGetDataf(ias_ref);
    float alt         = XPLMGetDataf(alt_ref);
    float vvi         = XPLMGetDataf(vvi_ref);
    float fuel        = XPLMGetDataf(fuel_ref);
    float aoa         = XPLMGetDataf(alpha_ref);

    // == Weather ==
    float turbulence  = XPLMGetDataf(turbulence_ref);  // 0-10
    float wind_speed  = XPLMGetDataf(wind_speed_ref);  // m/s
    float wind_dir    = XPLMGetDataf(wind_dir_ref);    // deg
    float precip      = XPLMGetDataf(precip_ref);      // 0-1
    float visibility  = XPLMGetDataf(visibility_ref);  // sm
    float temp_c      = XPLMGetDataf(temp_ref);        // degC

    // Deteksi stall: AoA > 15 derajat untuk SF50
    // (aoa dari alpha DataRef = sudut serang relatif thd aliran udara, -180 to 180)
    int stall = (fabsf(aoa) > 15.0f && fabsf(aoa) < 165.0f) ? 1 : 0;

    // == Format SimHub String ==
    char buf[512];
    sprintf(buf,
        "RPM:%.1f;SPD:%.2f;IAS:%.1f;ALT:%.0f;VVI:%.0f;"
        "FUEL:%.1f;HDG:%.1f;ENG:1;LGT:0;STALL:%d;"
        "ROLL:%.2f;PTCH:%.2f;"
        "AOA:%.2f;GN:%.3f;GA:%.3f;GS:%.3f;"
        "TURB:%.1f;WSPD:%.1f;WDIR:%.0f;"
        "PRCP:%.2f;VIS:%.1f;TEMP:%.1f;"
        "DMN:Udara;",
        rpm, speed_knots, ias, alt, vvi,
        fuel, yaw, stall,
        roll, pitch,
        aoa, gn, ga, gs,
        turbulence, wind_speed * 1.94384f, wind_dir,
        precip, visibility, temp_c
    );

    sendto(udp_socket, buf, strlen(buf), 0,
           (struct sockaddr*)&simhub_addr, sizeof(simhub_addr));

    return 0.01f; // 100Hz
}

// ---- Plugin Init ----
PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc)
{
    strcpy(outName, "LUP Motion Telemetry");
    strcpy(outSig,  "lup.telemetry.motion.v2");
    strcpy(outDesc, "Exports 6DOF + G-Force + Weather to FlyPT and SimHub | LUP Flight Simulator");

    // Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // FlyPT addr (port 4444)
    memset(&flypt_addr, 0, sizeof(flypt_addr));
    flypt_addr.sin_family      = AF_INET;
    flypt_addr.sin_port        = htons(4444);
    flypt_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    // SimHub addr (port 4445)
    memset(&simhub_addr, 0, sizeof(simhub_addr));
    simhub_addr.sin_family     = AF_INET;
    simhub_addr.sin_port       = htons(4445);
    simhub_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    // ---- DataRef Lookup ----

    // Attitude
    phi_ref   = XPLMFindDataRef("sim/flightmodel/position/phi");
    theta_ref = XPLMFindDataRef("sim/flightmodel/position/theta");
    psi_ref   = XPLMFindDataRef("sim/flightmodel/position/psi");

    // G-Force (XP12.4 flightmodel2)
    gforce_normal_ref = XPLMFindDataRef("sim/flightmodel2/misc/gforce_normal"); // Vertical
    gforce_axil_ref   = XPLMFindDataRef("sim/flightmodel2/misc/gforce_axil");   // Longitudinal
    gforce_side_ref   = XPLMFindDataRef("sim/flightmodel2/misc/gforce_side");   // Lateral

    // Flight Performance
    alpha_ref = XPLMFindDataRef("sim/flightmodel/position/alpha");
    rpm_ref   = XPLMFindDataRef("sim/flightmodel/engine/ENGN_N1_");
    speed_ref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    ias_ref   = XPLMFindDataRef("sim/cockpit2/gauges/indicators/airspeed_kts_pilot");
    alt_ref   = XPLMFindDataRef("sim/cockpit2/gauges/indicators/altitude_ft_pilot");
    vvi_ref   = XPLMFindDataRef("sim/cockpit2/gauges/indicators/vvi_fpm_pilot");
    fuel_ref  = XPLMFindDataRef("sim/cockpit2/gauges/indicators/fuel_quantity");

    // Weather (XP12 aircraft-relative)
    turbulence_ref = XPLMFindDataRef("sim/weather/aircraft/turbulence");
    wind_speed_ref = XPLMFindDataRef("sim/weather/aircraft/wind_now_speed_msc");
    wind_dir_ref   = XPLMFindDataRef("sim/weather/aircraft/wind_now_direction_degt");
    precip_ref     = XPLMFindDataRef("sim/weather/aircraft/precipitation_on_aircraft_ratio");
    visibility_ref = XPLMFindDataRef("sim/weather/aircraft/visibility_reported_sm");
    temp_ref       = XPLMFindDataRef("sim/weather/aircraft/temperature_ambient_deg_c");

    return 1;
}

PLUGIN_API void XPluginStop(void) {
    closesocket(udp_socket);
    WSACleanup();
}

PLUGIN_API void XPluginDisable(void) {
    XPLMUnregisterFlightLoopCallback(MyFlightLoopCallback, NULL);
}

PLUGIN_API int XPluginEnable(void) {
    XPLMRegisterFlightLoopCallback(MyFlightLoopCallback, 0.01f, NULL);
    return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void *inParam) { }
