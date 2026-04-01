// ============= USER MANUAL =============
// General
// - Esc        : Quit
// - Space      : Full reset (character, camera, lighting, FX, states)
// - '/'        : Toggle world-axes overlay (helper for building)
// - Window     : Viewport auto-resizes with window size
// - 'M'        : Toggle User Manual
//
// View / Camera
// - 'P'        : Toggle Perspective / Orthographic (Default: Perspective)
// - Mouse L-Drag: Orbit (yaw/pitch)
// - Mouse R-Drag: Pan (moves eye & target)
// - Mouse Wheel : Zoom in/out
// - '+' / '-'  : Zoom in/out  (Hold Shift = 2× step)
//
// Lighting
// - 'J'        : Toggle ambient light
// - 'K'        : Toggle diffuse light
// - 'L'        : Toggle specular light
// - ';'        : Orbit light left   (Hold Shift = bigger step)
// - '\''       : Orbit light right  (Hold Shift = bigger step)
//
// Environment
// - '5'        : Skydome: Soft rose-gold / purple pastel (Default)
// - '6'        : Skydome: Warm golden/orange “sunset”
// - '7'        : Skydome: White/gold oak trees & mountain
// - 'H'        : Toggle background music
// 
// Style & Appearance
// - '1'        : Armor/texture theme: Chinese Gold (Default)
// - '2'        : Armor/texture theme: Silver Armor
// - '3'        : Armor/texture theme: Blue Ceremonial
// - '4'        : Armor/texture theme: Orange Patterned
// - '8'        : Yellow-brown iris; toggle third eye OPEN
// - '9'        : Blue iris; third eye CLOSED (Default)
//
// Animation
// - 'Z'        : Toggle walking
// - 'V'        : Toggle running
// - 'X'        : Toggle wave (right hand)
// - 'C'        : Toggle combat/target stance
// - 'B'        : Raise & spin weapon, then hold
// - 'N'        : Toggle attack
// - 'A'        : Toggle head-ribbon wind movement
//
// Body Movement (Flexible Posture)
// - 'Q'        : Third-eye opening hand pose (Hold Shift = LEFT side; otherwise RIGHT)
// - 'W'        : Thumbs-up pose (Hold Shift = LEFT side; otherwise RIGHT)
// - 'E'        : Upper arm lift — 3 levels (Hold Shift = LEFT side; otherwise RIGHT)
// - 'R'        : Lower arm lift — 3 levels (Hold Shift = LEFT side; otherwise RIGHT)
// - 'T'        : Upper leg lift — 3 levels (Hold Shift = LEFT leg)
// - 'Y'        : Lower leg (knee bend) — 3 levels (Hold Shift = LEFT leg)
// - 'U'        : Foot tip bend — 3 levels (Hold Shift = LEFT leg)
// - 'I'        : Head shake
// - 'O'        : Head nod
//
// Special Effects
// - 'S'        : Toggle electrical weapon effect
// - 'D'        : Toggle thunder/lightning aura
// - 'F'        : Toggle triangle-sigil mode
//
// Locomotion (when Walking ['Z'] or Running ['V'])
// - ↑ / ↓      : Move forward / backward (camera-relative)
// - → / ←      : Move right / left (camera-relative)
//
// ======================================

#include <Windows.h>
#include <windowsx.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <mmsystem.h> 

#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLU32.lib")
#pragma comment(lib, "winmm.lib")

#define WINDOW_TITLE "OpenGL Window"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =================================================
// ===============      Globals       ==============
// =================================================

// ======  Camera Angles and Mouse State  =====
float angleX = 0.0f;  // Rotation around Y axis (left/right)
float angleY = 0.0f;  // Rotation around X axis (up/down)

// --- Mouse interaction state ---
static bool  gRotating = false;  // LMB
static bool  gPanning = false;   // RMB
static int   gLastX = 0, gLastY = 0;

// Sensitivities
static const float kRotateDegPerPixel = 0.25f; // degrees per pixel

// Show/hide world axes (toggle with '/')
static bool gShowAxes = false;

// Forward declarations 
void drawCharacter();
void display();
void drawWeapon();


// ===== Character Locomotion & World Limits =====
static float gCharX = 0.0f, gCharZ = 0.0f;
static float gCharYawDeg = 0.0f;
static const float kWalkSpeed = 3.0f;
static const float kRunSpeed = 6.5f;

// Turn speed toward movement heading (deg/sec)
static const float kFaceTurnWalk = 240.0f;
static const float kFaceTurnRun = 360.0f;

// Movement pivot (input accumulation)
static float kPivotX = 0.0f;   // left(-) / right(+)
static float kPivotZ = 0.0f;   // back(-) / forward(+)

// Shortest signed angle delta in degrees (-180..180)
static inline float AngleDeltaDeg(float a, float b) {
    float d = fmodf(b - a + 540.0f, 360.0f) - 180.0f;
    return d;
}

// World bounds (rectangular arena)
static const float kWorldMinX = -15.0f, kWorldMaxX = +15.0f;
static const float kWorldMinZ = -20.0f, kWorldMaxZ = +20.0f;


// ===========     Animation Globals     ===========
// ---------  Walking  ---------
float walkPhase = 0.0f;     // increases over time
float walkSpeed = 0.45f;    // cycles per second
bool  walking = false;      // toggle with 'Z'

// -----  Wave Hand ("Hi")  ------
float wavePhase = 0.0f;     // increases when waving
bool  waving = false;    // toggle with 'X'

// ------  Fighting Combat  -----
static bool  combatTarget = false;   // what we want when user presses 'C'
static float combatBlend = 0.0f;    // 0 = idle/walk, 1 = combat pose
static float combatBobPh = 0.0f;

static const float kCombatBlendSpeed = 3.5f; // higher = snappier blend
static const float kCombatBobHz = 1.2f; // bob frequency (Hz)
static const float kCombatBobAmp = 0.12f; // bob amplitude (world units)

// Small math helpers
static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float Clamp01(float t) { return (t < 0.f) ? 0.f : ((t > 1.f) ? 1.f : t); }
static inline float SmoothStep01(float t) { t = Clamp01(t); return t * t * (3.f - 2.f * t); }

// --------    Running    --------
bool running = false;              // toggle with 'V'
static const float kRunSpeedMul = 1.3f;  // ~1.8× faster than walk
static const float kRunAmpMul = 1.90f; // ~25% larger angles

// --------- Weapon “B” animation --------- 
enum WeaponState { WPN_IDLE = 0, WPN_RAISE_SPIN, WPN_MOVE_TO_BACK, WPN_HOLD_BACK };
static WeaponState weaponState = WPN_IDLE;
static float weaponTimer = 0.0f;     // time inside current state
static float weaponSpin = 0.0f;      // spin angle (deg), spin about Z

static const float kSpinHz = 2.5f;   // spin speed while raising
static const float kRaiseDur = 1.5f; // seconds to spin in front
static const float kMoveDur = 0.8f; // seconds to move to back

// sockets (positions/orientations) used by drawWeaponDisplay()
struct Socket {
    float tx, ty, tz;   // translate
    float rx, ry, rz;   // rotate order: X, Y, Z
};
static const Socket kHandSock = { -1.55f, 4.07f, 0.00f,  26.5f, -53.0f, -85.0f };
static const Socket kBackSock = { -0.20f, 4.90f,-1.15f,  10.0f, -25.0f, -115.0f };
// back: a bit higher & behind, rolled so the blade points up-left

// ---------- Attack 'N' animation ----------
enum AttackState { ATK_IDLE = 0, ATK_WINDUP, ATK_SWEEP, ATK_RECOVER };
static AttackState attackState = ATK_IDLE;
static float attackTimer = 0.0f;
static float attackSwingDeg = 0.0f;

// timings (seconds)
static const float kAtkWindup = 0.18f;  // quick half-squat windup
static const float kAtkSweep = 0.30f;  // forward sweep
static const float kAtkRecover = 0.32f;  // blend back to neutral


// ============== Body Movement Globals : HAND / ARM / LEG / HEAD ==============
// --------- Hand pose system ---------
enum HandPose { HANDPOSE_NONE = 0, HANDPOSE_PEACE = 1, HANDPOSE_THUMBSUP = 2 };

static HandPose gLeftHandPose = HANDPOSE_NONE;
static HandPose gRightHandPose = HANDPOSE_NONE;

static float gLeftHandPoseBlend = 0.0f;   // 0..1, smoothed each frame
static float gRightHandPoseBlend = 0.0f;   // 0..1, smoothed each frame
static const float kHandPoseBlendSpeed = 10.0f; // higher = snappier

// Which hand is currently calling drawHand()  (-1 = left, +1 = right)
static int gWhichHand = 0;

// --------- Arm lift cycles (E = upper/shoulder, R = lower/elbow) -------
static int gUpperLiftLevelL = 0, gUpperLiftLevelR = 0;  // left/right shoulders
static int gLowerLiftLevelL = 0, gLowerLiftLevelR = 0;  // left/right elbows

// Degrees added on top of the existing animation (idle/walk/combat/attack)
static const float kUpperLiftDeg[4] = { 0.0f, 15.0f, 35.0f, 60.0f }; // shoulder pitch offset
static const float kLowerLiftDeg[4] = { 0.0f, 20.0f, 45.0f, 70.0f }; // elbow flex offset

// ----- Leg lift cycles (T = upper/hip, Y = lower/knee, U = foot/ankle) -----
static int gUpperLegLevelL = 0, gUpperLegLevelR = 0;  // hip pitch (upper leg)
static int gLowerLegLevelL = 0, gLowerLegLevelR = 0;  // knee flex (lower leg)
static int gFootTipLevelL = 0, gFootTipLevelR = 0;  // ankle/toe pitch

// Degrees added on top of the existing animation (idle/walk/combat/attack)
static const float kHipLiftDeg[4] = { 0.0f, 12.0f, 26.0f, 42.0f }; // hip pitch
static const float kKneeFlexDeg[4] = { 0.0f, 15.0f, 35.0f, 60.0f }; // knee flex
static const float kFootTipDeg[4] = { 0.0f,  8.0f, 18.0f, 32.0f }; // ankle/toe tip

// ----- Head gesture (I=shake, O=nod) -----
enum HeadAnim { HEAD_IDLE = 0, HEAD_SHAKE, HEAD_NOD };
static HeadAnim gHeadAnim = HEAD_IDLE;
static float gHeadTimer = 0.0f;         // seconds
static float gHeadYawDeg = 0.0f;        // left/right (shake)
static float gHeadPitchDeg = 0.0f;      // up/down (nod)

static const float kHeadShakeHz = 2.4f;   // how fast to shake
static const float kHeadNodHz = 2.0f;   // how fast to nod
static const float kHeadShakeAmp = 18.0f;  // degrees peak (yaw)
static const float kHeadNodAmp = 14.0f;  // degrees peak (pitch)
static const int   kHeadCycles = 2;      // exactly two cycles per trigger


// ================ View/Camera Globals ===============
static int   gWidth = 800, gHeight = 600;
static bool  gPerspective = true;      // default -> perspective (Toggle with 'P')

// make default view a bit tighter 
static float gFovY = 42.0f;
static float gNearZ = 0.1f, gFarZ = 200.0f;

// and a tighter ortho too
static float gOrthoHeight = 9.0f;

// Eye & target
static float gCamX = 0.0f, gCamY = 4.0f, gCamZ = 18.0f; // eye
static float gCamTargetX = 0.0f, gCamTargetY = 3.0f, gCamTargetZ = 0.0f; // look-at
static float gCamSpeed = 0.5f; // movement step


// ================ Texture Wear Globals ================
static int gTexTheme = 0; // 0=Chinese (default), 1=Armor, 2=Blue, 3=Orange

static const char* gTexSets[4][3] = {
    {"chinesearmor.bmp", "chinesearmor2.bmp", "chinesearmor3.bmp"},
    {"armor.bmp",        "armor2.bmp",        "armor3.bmp"},
    {"blue.bmp",         "blue2.bmp",         "blue3.bmp"},
    {"orange3.bmp",      "orange.bmp",        "orange2.bmp"}
};

// Convenience: pick current texture filename by slot (0/1/2)
static inline const char* CurrentTex(int slot) {
    if (slot < 0 || slot > 2) slot = 0;
    return gTexSets[gTexTheme][slot];
}

// =============== Body color theme ===============
int gBodyColorTheme = 0;

static float BODY_COLORS[4][3] = {
    {0.82f, 0.565f, 0.122f},   // gold/brown Chinese
    {0.69f, 0.737f, 0.749f},   // blueish Armor
    {0.306f, 0.408f, 0.651f},  // ocean blue
    {0.969f, 0.624f, 0.012f}   // orange
};

// ================ Lighting ================
bool gAmbientOn = true;
bool gDiffuseOn = true;
bool gSpecularOn = true;

// ---- Orbiting light (controlled by ; and ') ----
static float gLightYawDeg = 0.0f;  // angle around the character (left/right)
static float gLightRadius = 9.5f;  // distance from the character on XZ plane
static float gLightHeight = 6.0f;  // height above ground
static float gLightCutoff = 35.0f; // spotlight cone (degrees)

// ================  Shadow  ================
/* Plane equation: Ax + By + Cz + D = 0
   For ground plane y = gGroundY, it is (0,1,0,-gGroundY). */
static void makeShadowMatrix_Plane(const GLfloat light[4], const GLfloat plane[4], GLfloat out[16])
{
    GLfloat dot = plane[0] * light[0] + plane[1] * light[1] +
        plane[2] * light[2] + plane[3] * light[3];

    out[0] = dot - light[0] * plane[0];
    out[4] = -light[0] * plane[1];
    out[8] = -light[0] * plane[2];
    out[12] = -light[0] * plane[3];

    out[1] = -light[1] * plane[0];
    out[5] = dot - light[1] * plane[1];
    out[9] = -light[1] * plane[2];
    out[13] = -light[1] * plane[3];

    out[2] = -light[2] * plane[0];
    out[6] = -light[2] * plane[1];
    out[10] = dot - light[2] * plane[2];
    out[14] = -light[2] * plane[3];

    out[3] = -light[3] * plane[0];
    out[7] = -light[3] * plane[1];
    out[11] = -light[3] * plane[2];
    out[15] = dot - light[3] * plane[3];
}

// ============== Knee Bend (HALF-SQUAT) ==============
static float AttackLowerBodyBlend() {
    if (walking || running)      return 0.0f; // only when standing
    if (attackState == ATK_IDLE) return 0.0f; // only during N animation
    if (attackState == ATK_WINDUP)  return SmoothStep01(attackTimer / kAtkWindup);
    if (attackState == ATK_SWEEP)   return 1.0f;
    if (attackState == ATK_RECOVER) return 1.0f - SmoothStep01(attackTimer / kAtkRecover);
    return 0.0f;
}

// Current lower-body combat factor: max(combatBlend, attack-overlay)
static float CurrentLowerBodyCombatT() {
    float t = combatBlend;
    t = fmaxf(t, AttackLowerBodyBlend());
    return t;
}

// ============ Special Effect : Weapon Lighting ===========
static bool  gFXEnabled = false;
static float WPN_BLADE_HEAD_FROM_GRIP_Y = 13.6f;

// ---- FX clock ----
static float fxTime = 0.0f;

// ---- Additive-blend helpers ----
struct FXState { GLboolean lighting, blend, tex2d; GLboolean depthMask; };

static void FX_BeginAdditive(FXState& st) {
    st.lighting = glIsEnabled(GL_LIGHTING);
    st.blend = glIsEnabled(GL_BLEND);
    st.tex2d = glIsEnabled(GL_TEXTURE_2D);
    st.depthMask = GL_TRUE; // assume true, restore later
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);             // don't write depth
}
static void FX_EndAdditive(const FXState& st) {
    if (st.lighting) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (st.blend)    glEnable(GL_BLEND);    else glDisable(GL_BLEND);
    if (st.tex2d)    glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
}

// ---- Simple hash/noise ----
static inline float nrand(float x) {
    // cheap deterministic noise in [-1,1]
    float s = sinf(x * 12.9898f) * 43758.5453f;
    return 2.0f * (s - floorf(s)) - 1.0f;
}

// ---- Electric ring: billboarded in XY plane of weapon-local space ----
static void FX_DrawElectricRing(float R = 0.75f, float thickness = 0.08f, int seg = 48, float speed = 2.0f) {
    const float t = fxTime * speed;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= seg; ++i) {
        float u = (float)i / (float)seg;
        float th = 6.2831853f * u;
        float wig = 0.12f * sinf(6.0f * th + t) + 0.05f * nrand(u + t);
        float r0 = R * (1.0f + wig);
        float r1 = (R + thickness) * (1.0f + 0.6f * wig);
        float a = 0.65f + 0.35f * sinf(14.0f * u + t); // crackly fade
        glColor4f(0.55f, 0.75f, 1.0f, a);                 // core
        glVertex3f(r0 * cosf(th), r0 * sinf(th), 0.0f);
        glColor4f(0.20f, 0.55f, 1.0f, 0.45f * a);
        glVertex3f(r1 * cosf(th), r1 * sinf(th), 0.0f);
    }
    glEnd();
}

// ---- Lightning arc around weapon ----
static void FX_DrawLightningArc(float R0 = 0.65f, float R1 = 0.95f, int kinks = 9, float phase = 0.0f) {
    glLineWidth(2.2f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < kinks; ++i) {
        float u = (float)i / (float)(kinks - 1);
        float th = 6.2831853f * (u + phase);
        float r = Lerp(R0, R1, u) * (1.0f + 0.10f * nrand(7.0f * u + 3.0f * fxTime));
        float z = 0.04f * nrand(11.0f * u - 2.0f * fxTime);
        float a = 0.70f * (0.3f + 0.7f * (1.0f - fabsf(2.0f * u - 1.0f)));
        glColor4f(0.75f, 0.90f, 1.0f, a);
        glVertex3f(r * cosf(th), r * sinf(th), z);
    }
    glEnd();
    glLineWidth(1.0f);
}

// ---- Shock pulse & fireworks helpers ----
static void FX_DrawShockPulse(float baseR, float speed, float thickness, float gain) {
    float p = fmodf(fxTime * speed, 1.0f); // 0 → 1
    float r0 = baseR + 1.25f * p;
    float a = (1.0f - p) * 0.9f * gain;
    glPushMatrix();
    glRotatef(90.0f, 1, 0, 0);               // encircle Y axis
    glColor4f(0.85f, 0.95f, 1.0f, a);
    FX_DrawElectricRing(r0, thickness, 72, 2.4f);
    glPopMatrix();
}

static void FX_DrawFireworks(int rays, float Rmin, float Rmax, float gain) {
    glLineWidth(2.6f);
    glBegin(GL_LINES);
    for (int i = 0; i < rays; ++i) {
        float u = (i + 0.37f) / (float)rays;
        float ang = 6.2831853f * (u + 0.13f * sinf(fxTime * 0.7f + 7.0f * u));
        float life = fmodf(fxTime * 1.15f + u * 3.0f, 1.0f);
        float r0 = Rmin + (Rmax - Rmin) * 0.1f * life;
        float r1 = Rmin + (Rmax - Rmin) * (0.6f + 0.4f * life);
        float wob = 0.15f * nrand(9.0f * u + fxTime);
        float c = cosf(ang + wob), s = sinf(ang + wob);
        float a0 = (0.35f + 0.65f * life) * 0.9f * gain;
        float a1 = (0.15f + 0.85f * (1.0f - life)) * 0.9f * gain;
        glColor4f(0.95f, 1.00f, 1.00f, a0); glVertex3f(r0 * c, r0 * s, 0.00f);
        glColor4f(0.55f, 0.80f, 1.00f, a1); glVertex3f(r1 * c, r1 * s, 0.06f * nrand(u * 17.0f));
    }
    glEnd();
    glLineWidth(1.0f);
}

// ---- Grip-relative thunder bundle (centered at blade head) ----
static void FX_DrawWeaponThunderAt(float centerFromGripY, float intensity /*0..~1.6*/) {
    if (intensity <= 0.01f) return;
    FXState st; FX_BeginAdditive(st);
    glPushMatrix();
    glTranslatef(0.0f, centerFromGripY, 0.0f); // move from grip to head

    // core rings
    glPushMatrix(); glRotatef(fxTime * 140.0f, 0, 0, 1);
    FX_DrawElectricRing(1.45f * intensity, 0.24f, 84, 2.6f);
    glPopMatrix();

    glPushMatrix(); glRotatef(-fxTime * 90.0f, 0, 0, 1);
    FX_DrawElectricRing(1.05f * intensity, 0.18f, 72, 2.0f);
    glPopMatrix();

    // fat lightning arcs
    FX_DrawLightningArc(1.10f * intensity, 1.75f * intensity, 13, 0.02f * sinf(fxTime));
    FX_DrawLightningArc(1.00f * intensity, 1.60f * intensity, 12, 0.27f + 0.05f * cosf(1.7f * fxTime));
    FX_DrawLightningArc(1.20f * intensity, 1.85f * intensity, 14, 0.55f + 0.03f * sinf(2.1f * fxTime));

    // fireworks + shock pulse
    FX_DrawFireworks(28, 0.6f * intensity, 2.2f * intensity, 1.0f);
    FX_DrawShockPulse(0.35f * intensity, 1.15f, 0.20f, 1.0f);

    glPopMatrix();
    FX_EndAdditive(st);
}

// ============= Special Effect : Thunderstorm ===============
struct _ThState {
    bool  on = false; // toggle
    bool  tri = false; // triangle mode
    DWORD prev = 0;     // tick
    float t = 0.0f;  // time (sec)
    float a = 0.0f;  // blend 0..1
};
static _ThState& _th() { static _ThState S; return S; } // function-scoped static

// small utils
static unsigned _h32(unsigned x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
static float    _r01(unsigned s) { return (_h32(s) & 0x00FFFFFF) / 16777216.0f; }

// local FX state for this module
struct _FXState { GLboolean lighting, blend, tex2d, depthMask; };

static void _FXBegin(_FXState& st) {
    st.lighting = glIsEnabled(GL_LIGHTING);
    st.blend = glIsEnabled(GL_BLEND);
    st.tex2d = glIsEnabled(GL_TEXTURE_2D);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &st.depthMask);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive
    glDepthMask(GL_FALSE);
}
static void _FXEnd(const _FXState& st) {
    glDepthMask(st.depthMask);
    if (st.tex2d)    glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    if (st.blend)    glEnable(GL_BLEND);      else glDisable(GL_BLEND);
    if (st.lighting) glEnable(GL_LIGHTING);   else glDisable(GL_LIGHTING);
    glLineWidth(1.0f);
    glDisable(GL_LINE_STIPPLE);
}

static void _glowBillboard(float size, float a, float r, float g, float b) {
    glColor4f(r, g, b, a);
    float s = 0.5f * size;
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(-s, -s, 0); glVertex3f(s, -s, 0);
    glVertex3f(-s, s, 0); glVertex3f(s, s, 0);
    glEnd();
}

// Lightning bolt (yellow, bigger, less-solid glow, clearer lines)
static void _drawBolt(unsigned seedBase, float baseX,
    float topY, float botY, float zBack,
    float spreadX, float jitterXZ, int segments,
    float alpha, float coreW, float glowW)
{
    float dy = (botY - topY) / segments;

    // halo (stippled)
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x3F3F);
    glLineWidth(glowW);
    glColor4f(1.0f, 0.90f, 0.50f, alpha * 0.22f); // warm yellow
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        unsigned s = seedBase + 31u * i;
        float jx = (_r01(s + 1u) - 0.5f) * 2.0f * jitterXZ * (0.6f + 0.4f * t);
        float jz = (_r01(s + 2u) - 0.5f) * 2.0f * jitterXZ * (0.6f + 0.4f * t);
        float x = baseX + (_r01(s) - 0.5f) * 2.0f * spreadX * (0.2f + 0.8f * (1.0f - fabsf(0.5f - t) * 2.0f));
        float y = topY + dy * i;
        glVertex3f(x + jx, y, zBack + jz);
    }
    glEnd();
    glDisable(GL_LINE_STIPPLE);

    // core (thin & bright)
    glLineWidth(coreW);
    glColor4f(1.0f, 0.98f, 0.75f, alpha * 0.95f); // pale yellow
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float t = (float)i / (float)segments;
        unsigned s = seedBase + 131u * i + 17u;
        float jx = (_r01(s + 11u) - 0.5f) * 2.0f * jitterXZ * (0.5f + 0.5f * t);
        float jz = (_r01(s + 12u) - 0.5f) * 2.0f * jitterXZ * (0.5f + 0.5f * t);
        float x = baseX + (_r01(s) - 0.5f) * 2.0f * spreadX * (0.1f + 0.9f * (1.0f - t));
        float y = topY + dy * i;
        glVertex3f(x + jx, y, zBack + jz);
    }
    glEnd();
    glLineWidth(1.0f);
}

// Triangle edges (apex TOP → “top has 2 edges, bottom 1 edge”)
static void _drawTriangleEdges(float width, float height, float alpha, float coreW, float glowW)
{
    const float halfW = 0.5f * width;
    const float topY = +0.5f * height;
    const float botY = -0.5f * height;

    const float ax = 0.0f, ay = topY;     // apex (top center)
    const float blx = -halfW, bly = botY; // bottom-left
    const float brx = +halfW, bly2 = botY;// bottom-right

    // halo
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x3F3F);
    glLineWidth(glowW);
    glColor4f(1.0f, 0.90f, 0.50f, alpha * 0.25f);
    glBegin(GL_LINE_STRIP); glVertex3f(ax, ay, 0); glVertex3f(blx, bly, 0); glEnd();
    glBegin(GL_LINE_STRIP); glVertex3f(ax, ay, 0); glVertex3f(brx, bly2, 0); glEnd();
    glBegin(GL_LINE_STRIP); glVertex3f(blx, bly, 0); glVertex3f(brx, bly2, 0); glEnd();
    glDisable(GL_LINE_STIPPLE);

    // core
    glLineWidth(coreW);
    glColor4f(1.0f, 0.98f, 0.75f, alpha * 0.95f);
    glBegin(GL_LINE_STRIP); glVertex3f(ax, ay, 0); glVertex3f(blx, bly, 0); glEnd();
    glBegin(GL_LINE_STRIP); glVertex3f(ax, ay, 0); glVertex3f(brx, bly2, 0); glEnd();
    glBegin(GL_LINE_STRIP); glVertex3f(blx, bly, 0); glVertex3f(brx, bly2, 0); glEnd();
}

// Call once per frame from inside drawCharacter() AFTER root transform
void FX_Thunder_DrawBehind_NoGlobals()
{
    auto& s = _th();

    // clock
    DWORD now = GetTickCount();
    if (s.prev == 0) s.prev = now;
    float dt = (now - s.prev) * 0.001f;
    s.prev = now; s.t += dt;

    // smooth in/out
    const float kBlendSpeed = 6.0f;
    s.a = s.on ? fminf(1.0f, s.a + kBlendSpeed * dt)
        : fmaxf(0.0f, s.a - kBlendSpeed * dt);
    if (s.a <= 0.001f) return;

    // Bigger/warmer settings
    const int   kBoltCount = 8;
    const int   kSegments = 12;
    const float kBehindDist = 0.65f;
    const float kTopY = 9.0f;
    const float kBotY = -0.7f;
    const float kSpreadX = 2.30f;
    const float kJitterXZ = 0.08f;
    const float kGlowSize = 2.6f;
    const float kCoreWidth = 3.0f;
    const float kGlowWidth = 5.0f;
    const float kStrikeHz = 2.2f;

    float pulse = powf(fabsf(sinf(s.t * 3.1415926f * kStrikeHz)), 12.0f);
    float alpha = s.a * (0.65f + 0.35f * pulse);

    _FXState st; _FXBegin(st);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -kBehindDist); // behind back

    // back glow (warm yellow)
    glPushMatrix();
    glTranslatef(0.0f, kTopY * 0.60f, 0.0f);
    _glowBillboard(kGlowSize * 1.30f, alpha * 0.18f, 1.00f, 0.96f, 0.60f);
    _glowBillboard(kGlowSize * 0.85f, alpha * 0.12f, 1.00f, 0.93f, 0.55f);
    glPopMatrix();

    if (!s.tri) {
        // Lightning mode
        for (int b = 0; b < kBoltCount; ++b) {
            unsigned seed = 911u * (unsigned)b + (unsigned)(s.t * 1000.0f);
            float bx = (_r01(seed) - 0.5f) * 2.0f * (kSpreadX * 0.65f);
            _drawBolt(seed, bx, kTopY, kBotY, 0.0f, kSpreadX, kJitterXZ, kSegments, alpha, kCoreWidth, kGlowWidth);
        }
    }
    else {
        // Triangle mode
        const float triW = 1.6f, triH = 1.8f;
        _drawTriangleEdges(triW, triH, alpha, /*coreW=*/3.0f, /*glowW=*/6.0f);
        _glowBillboard(0.22f, alpha * 0.25f, 1.0f, 0.95f, 0.60f);
        glPushMatrix(); glTranslatef(-0.8f, -0.9f, 0.0f);
        _glowBillboard(0.18f, alpha * 0.18f, 1.0f, 0.95f, 0.60f); glPopMatrix();
        glPushMatrix(); glTranslatef(0.8f, -0.9f, 0.0f);
        _glowBillboard(0.18f, alpha * 0.18f, 1.0f, 0.95f, 0.60f); glPopMatrix();
    }

    glPopMatrix();
    _FXEnd(st);
}


// ========== ENVIRONMENT (PLANE + SKYDOME) ===========
// Backgrounds: 5=background1, 6=background2, 7=background3
static const char* gSkyBGFiles[3] = { 
    "background1.bmp", 
    "background2.bmp", 
    "background3.bmp" };
static GLuint gSkyTexes[3] = { 0, 0, 0 };
static int    gSkyBGIndex = 0;   // default background1.bmp

// constants (avoid M_PI issues)
static constexpr float kPif = 3.14159265358979323846f;
static float gSkyRadius = 86.0f;      // dome radius
static int   gSkySlices = 64, gSkyStacks = 32;
static float gSkyUTiles = 2.5f;       // texture tiles across U
static float gSkyVTiles = 2.5f;       // texture tiles across V

// textures & settings
static GLuint gSkyTex = 0, gGroundTex = 0;
static bool   gSkyFlipU = false;      // set true if panorama looks mirrored
static float  gGroundY = -1.8f;      // plane height (lower = further down)

// helpers for UV/aspect
static float gSkyUClamp = 1.0f, gSkyVClamp = 1.0f;
static float gGroundVTiling = 1.0f;   // keep non-square ground images from stretching

// single-load BMP helper (24/32-bit BI_RGB only)
static bool LoadBMPTextureOnce(const char* filename, GLuint& outTex, int* outW = nullptr, int* outH = nullptr)
{
    if (outTex) { return true; } // already loaded

    BITMAP bmp{};
    HBITMAP hBmp = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), filename,
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hBmp) return false;
    if (GetObject(hBmp, sizeof(bmp), &bmp) != sizeof(bmp) || !bmp.bmBits ||
        (bmp.bmBitsPixel != 24 && bmp.bmBitsPixel != 32)) {
        DeleteObject(hBmp); return false;
    }

    if (outW) *outW = bmp.bmWidth;
    if (outH) *outH = bmp.bmHeight;

    GLenum src = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
    GLint  fmt = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

    glGenTextures(1, &outTex);
    glBindTexture(GL_TEXTURE_2D, outTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, bmp.bmWidth, bmp.bmHeight, 0, src, GL_UNSIGNED_BYTE, bmp.bmBits);
    glBindTexture(GL_TEXTURE_2D, 0);
    DeleteObject(hBmp);
    return true;
}

// ============= Head Globals ===============
// ----------- Eyes / third-eye ------------
// Iris color (default blue)
float irisR = 0.15f, irisG = 0.45f, irisB = 0.95f;

// Third-eye toggle
bool gThirdEyeOpen = false;

// Third-eye size/position knobs (tweak these)
float gThirdEyeYOffset = 0.08f; // relative to yEyes
float gThirdEyeZLift = 0.003f;
float gThirdEyeHalfW = 0.01f; // diamond half-width
float gThirdEyeHalfH = 0.029f; // diamond half-height
float gThirdEyeLineHalf = 0.013f; // closed-line half-length

// ---------- mouth  ------------
// Mouth curvature: + = edges down (smile), - = edges up (frown), 0 = flat
float gMouthCurv = -0.01f;

// ----------- ribbon ------------
static void drawHelmetRibbons();
static void drawRibbonChain(float basePhase, float sideSign);

// Globals / Tunables
static bool  gRibbonWind = false; // toggled by 'F'
static float gRibbonTime = 0.0f;  // seconds (advances only while wind ON)
static float gRibbonBlend = 0.0f;  // 0..1 smooth strength

// Geometry look
static const int   kSegs = 4;      // 4 parts per ribbon
static const float kRibbonLen = 0.5f;   // length of each segment
static const float kRibbonThk = 0.015f; // segment box thickness

// Motion feel
static const float kBaseSagDeg = 18.0f; // gravity sag base (per seg, tapers)
static const float kAmpDeg = 22.0f; // top swing amplitude (deg)
static const float kDampPerSeg = 0.70f; // amplitude multiplier deeper in chain
static const float kWindHz = 0.5f;  // cycles/second

// Spacing / placement (relative to head local space)
static const float kSideSpread = 0.01f;  // spacing between two ribbons per side

// Anchors relative to head pivot (tune to model)
static const float kAnchorY = 0.05f; // near ring height
static const float kAnchorZ = 0.03f; // slightly in front
static const float kAnchorX_L = -0.25f; // left anchor
static const float kAnchorX_R = 0.25f; // right anchor


// =============== User Manual Overlay 'M' ===============
static bool  gShowManual = false;

// Simple Win32/OpenGL bitmap font (built on first use)
static GLuint gFontBase = 0;
static HFONT  gHudFont = nullptr;
// HUD font (you already have gFontBase / gHudFont)
static int gHudCharW = 8;   // average glyph width (fallback)
static int gHudCharH = 16;  // glyph height (fallback)

static void BuildGLHudFontIfNeeded()
{
    if (gFontBase) return;

    HGLRC rc = wglGetCurrentContext();
    HDC   dc = wglGetCurrentDC();
    if (!rc || !dc) { OutputDebugStringA("HUD: no current GL RC/DC\n"); return; }

    gHudFont = CreateFontA(
        -13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");

    if (!gHudFont) { OutputDebugStringA("HUD: CreateFont failed\n"); return; }

    HFONT old = (HFONT)SelectObject(dc, gHudFont);

    gFontBase = glGenLists(96);
    if (!gFontBase) {
        OutputDebugStringA("HUD: glGenLists failed\n");
        SelectObject(dc, old);
        return;
    }

    BOOL ok = wglUseFontBitmapsA(dc, 32, 96, gFontBase);
    SelectObject(dc, old);
    if (!ok) {
        OutputDebugStringA("HUD: wglUseFontBitmaps failed\n");
        glDeleteLists(gFontBase, 96);
        gFontBase = 0;
    }
}

static void GLPrint(float x, float y, const char* text)
{
    if (!text || !*text) return;
    BuildGLHudFontIfNeeded();
    if (!gFontBase) return;  // font build failed; bail gracefully

    // Ortho top-left
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, gWidth, gHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    glColor4f(1, 1, 1, 1);         // ensure visible
    glRasterPos2f(x, y);
    glListBase(gFontBase - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);

    glEnable(GL_DEPTH_TEST);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Small on-screen hint (bottom-left)
static void DrawManualHint()
{
    if (gShowManual) return; // don't show when the manual is open

    const char* hint = "Press [M] for User Manual";

    // Draw a subtle drop shadow then the bright text
    glColor3f(0.0f, 0.0f, 0.0f);  GLPrint(12.0f, (float)gHeight - 14.0f, hint);
    glColor3f(1.0f, 1.0f, 1.0f);  GLPrint(11.0f, (float)gHeight - 15.0f, hint);
}

// Draw a "key : value" line with hanging wrap (no <string> needed).
// - x,y  : top-left of column
// - colW : column width in pixels
// - keyW : reserved width for the key text (value wraps in the rest)
// - lh   : line height in pixels
static void HUDLineKV(float x, float& y, float colW,
    const char* key, const char* val,
    float lh, float keyW)
{
    if (!key || !val) return;

    // draw key
    glColor3f(0.92f, 0.95f, 0.98f);
    glRasterPos2f(x, y);
    glListBase(gFontBase - 32);
    glCallLists((GLsizei)strlen(key), GL_UNSIGNED_BYTE, key);

    // wrap value into (colW - keyW) and indent under key
    const float vx = x + keyW;
    const float vWidth = (colW > keyW) ? (colW - keyW) : 0.0f;
    const unsigned char* p = (const unsigned char*)val;

    while (*p) {
        const unsigned char* lineStart = p;
        const unsigned char* lastSpace = nullptr;
        int w = 0;
        // estimate width using average glyph width
        int cw = (gHudCharW > 0) ? gHudCharW : 8;

        while (*p && (w + cw) <= (int)vWidth) {
            if (*p == ' ') lastSpace = p;
            w += cw;
            ++p;
        }

        const unsigned char* lineEnd = p;
        if (*p && lastSpace) lineEnd = lastSpace; // wrap at last space

        glRasterPos2f(vx, y);
        glListBase(gFontBase - 32);
        glCallLists((GLsizei)(lineEnd - lineStart), GL_UNSIGNED_BYTE, lineStart);
        y += lh;

        p = (*p && lastSpace) ? lastSpace + 1 : p;
        while (*p == ' ') ++p; // skip extra spaces
    }
}

// Draws a modal, semi-transparent panel with key bindings.
static void DrawUserManualOverlay()
{
    // ---------- Setup 2D ----------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, gWidth, gHeight, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---------- Backdrop ----------
    glColor4f(0.02f, 0.02f, 0.04f, 0.66f); // fullscreen dim
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f((GLfloat)gWidth, 0);
    glVertex2f((GLfloat)gWidth, (GLfloat)gHeight); glVertex2f(0, (GLfloat)gHeight);
    glEnd();

    const float pad = 16.0f;                                // tighter pad
    const float panelW = (float)gWidth - pad * 2.0f;
    const float panelH = (float)gHeight - pad * 2.0f;

    glColor4f(0.10f, 0.12f, 0.16f, 0.92f);                  // panel
    glBegin(GL_QUADS);
    glVertex2f(pad, pad);
    glVertex2f(pad + panelW, pad);
    glVertex2f(pad + panelW, pad + panelH);
    glVertex2f(pad, pad + panelH);
    glEnd();

    glLineWidth(2.0f);                                      // border
    glColor4f(0.65f, 0.75f, 1.0f, 0.95f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(pad, pad);
    glVertex2f(pad + panelW, pad);
    glVertex2f(pad + panelW, pad + panelH);
    glVertex2f(pad, pad + panelH);
    glEnd();
    glLineWidth(1.0f);

    // ---------- Title ----------
    float x = pad + 14.0f;
    float y = pad + 28.0f;

    // slightly tighter line height based on window height
    const float lh = (gHeight >= 900) ? 18.0f : (gHeight >= 720 ? 16.0f : 14.0f);

    glColor3f(1.0f, 1.0f, 1.0f);
    GLPrint(x, y, "USER MANUAL  (press 'M' to close)");
    y += lh * 1.25f;

    // ---------- Columns ----------
    const float gap = 22.0f;                 // space between columns
    const float colW = (panelW - gap - 28.0f) * 0.5f; // leave a hair of side margin

    float keyW = colW * 0.42f;
    if (keyW < 120.0f) keyW = 120.0f;
    if (keyW > 220.0f) keyW = 220.0f;

    const float col1X = x;
    const float col2X = x + colW + gap;
    float y1 = y, y2 = y;

    // vertical column divider (subtle)
    glColor4f(0.55f, 0.65f, 0.90f, 0.35f);
    glBegin(GL_LINES);
    glVertex2f(col2X - gap * 0.5f, pad + 12.0f);
    glVertex2f(col2X - gap * 0.5f, pad + panelH - 12.0f);
    glEnd();

    auto Section = [&](float& xx, float& yy, const char* s) {
        glColor3f(0.80f, 0.88f, 1.0f);
        GLPrint(xx, yy, s); yy += lh;
        };
    auto Line = [&](float& xx, float& yy, const char* s) {
        glColor3f(0.92f, 0.95f, 0.98f);
        GLPrint(xx, yy, s); yy += lh;
        };
    auto Spacer = [&](float& yy, float mul = 0.35f) { yy += lh * mul; };

    // ===== LEFT COLUMN =====
    Section(x, y1, "// General");
    HUDLineKV(x, y1, colW, "- Space", "Reset all", lh, keyW);
    HUDLineKV(x, y1, colW, "- '/'", "Toggle world axes", lh, keyW);
    HUDLineKV(x, y1, colW, "- Window", "View resizes to fit", lh, keyW);
    Spacer(y1);

    Section(x, y1, "// View / Camera");
    HUDLineKV(x, y1, colW, "- 'P'", "Perspective / Ortho", lh, keyW);
    HUDLineKV(x, y1, colW, "- LMB drag", "Orbit (yaw/pitch)", lh, keyW);
    HUDLineKV(x, y1, colW, "- RMB drag", "Pan (move eye & target)", lh, keyW);
    HUDLineKV(x, y1, colW, "- Wheel / +/-", "Zoom  (Shift = 2x)", lh, keyW);
    Spacer(y1);

    Section(x, y1, "// Lighting");
    HUDLineKV(x, y1, colW, "- J / K / L", "Ambient / Diffuse / Specular", lh, keyW);
    HUDLineKV(x, y1, colW, "- ';' / '''", "Orbit light left / right (Shift = bigger step)", lh, keyW);
    Spacer(y1);

    Section(x, y1, "// Environment");
    HUDLineKV(x, y1, colW, "- '5' '6' '7'", "Skydome 1 / 2 / 3", lh, keyW);
    HUDLineKV(x, y1, colW, "- 'H'", "Background Music", lh, keyW);
    Spacer(y1);

    Section(x, y1, "// Style & Appearance");
    HUDLineKV(x, y1, colW, "- '1'..'4'", "Themes: Gold / Silver / Blue / Orange", lh, keyW);
    HUDLineKV(x, y1, colW, "- '8'", "Yellow iris + open 3rd eye", lh, keyW);
    HUDLineKV(x, y1, colW, "- '9'", "Blue iris + close 3rd eye", lh, keyW);

    // ===== RIGHT COLUMN =====
    x = col2X;

    Section(x, y2, "// Animation");
    HUDLineKV(x, y2, colW, "- 'Z'", "Walk", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'V'", "Run", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'X'", "Wave (right hand)", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'C'", "Combat stance", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'B'", "Raise & spin weapon, then hold", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'N'", "Attack", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'A'", "Ribbon wind", lh, keyW);
    Spacer(y2);

    Section(x, y2, "// Body / Head");
    HUDLineKV(x, y2, colW, "- 'Q'", "Third-eye hand (Shift = LEFT, else RIGHT)", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'W'", "Thumbs-up (Shift = LEFT, else RIGHT)", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'E'/'R'", "Upper/Lower arm — 3 levels", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'T'/'Y'/'U'", "Hip/Knee/Foot — 3 levels (Shift = LEFT)", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'I'/'O'", "Head shake / Nod", lh, keyW);
    Spacer(y2);

    Section(x, y2, "// FX");
    HUDLineKV(x, y2, colW, "- 'S'", "Electric weapon", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'D'", "Thunder aura", lh, keyW);
    HUDLineKV(x, y2, colW, "- 'F'", "Triangle sigil", lh, keyW);
    Spacer(y2);

    Section(x, y2, "// Locomotion (when Walk/Run)");
    HUDLineKV(x, y2, colW, "- Arrows", "Camera-relative move", lh, keyW);

    // ---------- Restore ----------
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ======= Audio (background music) =======
static bool gMusicOn = false;

static void Music_Stop() {
    mciSendStringA("stop bgm", 0, 0, 0);
    mciSendStringA("close bgm", 0, 0, 0);
    gMusicOn = false;
}

static bool Music_StartMp3(const char* path) {
    Music_Stop();
    char cmd[512];
    wsprintfA(cmd, "open \"%s\" type mpegvideo alias bgm", path);
    if (mciSendStringA(cmd, 0, 0, 0) != 0) {
        OutputDebugStringA("MCI open failed (check path/codec)\n");
        return false;
    }
    mciSendStringA("play bgm repeat", 0, 0, 0);
    gMusicOn = true;
    return true;
}

// ==================================================

static void ApplyProjection()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const float aspect = (gHeight > 0) ? (float)gWidth / (float)gHeight : 1.0f;

    if (gPerspective) {
        // Perspective projection
        gluPerspective(gFovY, aspect, gNearZ, gFarZ);
    }
    else {
        // Orthographic: preserve aspect so scale is uniform
        float halfH = 0.5f * gOrthoHeight;
        float halfW = halfH * aspect;
        glOrtho(-halfW, halfW, -halfH, halfH, -gFarZ, gFarZ);
    }

    glMatrixMode(GL_MODELVIEW);
}

static void OnResize(int w, int h)
{
    gWidth = (w > 1) ? w : 1;
    gHeight = (h > 1) ? h : 1;

    glViewport(0, 0, gWidth, gHeight);
    ApplyProjection();
}

static void UpdateAnimation(float dt)
{ 
    fxTime += dt; // Thunderstorm Effect

    // --- Walking & Running---
    if (walking) {
        float gaitHz = walkSpeed * (running ? kRunSpeedMul : 1.0f);
        walkPhase += 6.2831853f * gaitHz * dt;
    }

    // --- Waving ---
    if (waving) {
        wavePhase += 6.2831853f * 0.7 * dt;
    }

    // --- Combat animation ---
    float target = combatTarget ? 1.0f : 0.0f;
    float k = 1.0f - expf(-kCombatBlendSpeed * dt); // exponential smoothing
    combatBlend += (target - combatBlend) * k;

    // Drive a subtle bob when near/in combat
    if (combatBlend > 0.01f) {
        combatBobPh += 6.2831853f * kCombatBobHz * dt;
    }

    // ---- Weapon “B” animation driver ----
    if (weaponState != WPN_IDLE) {
        weaponTimer += dt;

        if (weaponState == WPN_RAISE_SPIN) {
            weaponSpin += 360.0f * kSpinHz * dt;           // spin about Z
            if (weaponTimer >= kRaiseDur) {
                weaponState = WPN_MOVE_TO_BACK;
                weaponTimer = 0.0f;
            }
        }
        else if (weaponState == WPN_MOVE_TO_BACK) {
            // no spin during move
            if (weaponTimer >= kMoveDur) {
                weaponState = WPN_HOLD_BACK;               // arrive at back
                weaponTimer = 0.0f;
            }
        }
        // WPN_HOLD_BACK stays until you press C/Space etc.
    }

    // ---- Attack 'N' (one-shot) driver ----
    if (attackState != ATK_IDLE) {
        attackTimer += dt;

        if (attackState == ATK_WINDUP) {
            // crouch/prime; no swing yet
            attackSwingDeg = 0.0f;
            if (attackTimer >= kAtkWindup) {
                attackState = ATK_SWEEP;
                attackTimer = 0.0f;
            }
        }
        else if (attackState == ATK_SWEEP) {
            // sweep weapon forward about Z (hand frame)
            float u = SmoothStep01(attackTimer / kAtkSweep);
            attackSwingDeg = 115.0f * u;   // ~0 → 115°
            if (attackTimer >= kAtkSweep) {
                attackState = ATK_RECOVER;
                attackTimer = 0.0f;
            }
        }
        else if (attackState == ATK_RECOVER) {
            // bring swing back to neutral
            float u = SmoothStep01(attackTimer / kAtkRecover);
            attackSwingDeg = 115.0f * (1.0f - u);
            if (attackTimer >= kAtkRecover) {
                attackState = ATK_IDLE;
                attackTimer = 0.0f;
                attackSwingDeg = 0.0f;
            }
        }
    }

    // ========== BODY MOVEMENT ==========
    // --- Hand-pose blending ---
    auto drive = [&](HandPose p, float& blend) {
        float target = (p == HANDPOSE_NONE) ? 0.0f : 1.0f;
        float k = 1.0f - expf(-kHandPoseBlendSpeed * dt);
        blend += (target - blend) * k;
        };
    drive(gLeftHandPose, gLeftHandPoseBlend);
    drive(gRightHandPose, gRightHandPoseBlend);

    // --- Head gestures (I=shake, O=nod) ---
    if (gHeadAnim != HEAD_IDLE) {
        gHeadTimer += dt;
        const float hz = (gHeadAnim == HEAD_SHAKE) ? kHeadShakeHz : kHeadNodHz;
        const float dur = (float)kHeadCycles / hz;           // total time for 2 cycles
        const float u = (dur > 0.0f) ? fminf(gHeadTimer / dur, 1.0f) : 1.0f;

        // Simple smooth fade-out envelope so the motion settles nicely
        const float env = 0.5f * (cosf(3.1415926f * u) + 1.0f); // 1 -> 0

        if (gHeadAnim == HEAD_SHAKE) {
            gHeadYawDeg = kHeadShakeAmp * sinf(6.2831853f * hz * gHeadTimer) * env;
            gHeadPitchDeg = 0.0f;
        }
        else { // HEAD_NOD
            gHeadPitchDeg = kHeadNodAmp * sinf(6.2831853f * hz * gHeadTimer) * env;
            gHeadYawDeg = 0.0f;
        }

        if (gHeadTimer >= dur) {
            gHeadAnim = HEAD_IDLE;
            gHeadTimer = 0.0f;
            gHeadYawDeg = gHeadPitchDeg = 0.0f;
        }
    }

    // ==== Character locomotion: camera-relative style ====
    if (walking) {
        // 1) Read arrow keys → desired move axes: Up/Down = forward, Left/Right = strafe
        int up = (GetAsyncKeyState(VK_UP) & 0x8000) ? 1 : 0;
        int down = (GetAsyncKeyState(VK_DOWN) & 0x8000) ? 1 : 0;
        int left = (GetAsyncKeyState(VK_LEFT) & 0x8000) ? 1 : 0;
        int right = (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 1 : 0;

        float ax = (float)right - (float)left;   // strafe axis  (+ = right)
        float ay = (float)up - (float)down;   // forward axis (+ = forward)

        // 2) If no input, do nothing (no drift)
        if (ax != 0.0f || ay != 0.0f) {
            // Normalize input (diagonals not faster)
            float len = sqrtf(ax * ax + ay * ay);
            ax /= len; ay /= len;

            // 3) Camera-relative basis on XZ plane (use your view yaw 'angleX')
            float camYawRad = angleX * 3.1415926f / 180.0f;
            float s = sinf(camYawRad), c = cosf(camYawRad);

            // Forward (camera look projected to XZ) and Right vectors
            float fwdX = s, fwdZ = c;
            float rgtX = c, rgtZ = -s;

            // 4) Desired world move direction
            float dx = rgtX * ax + fwdX * ay;
            float dz = rgtZ * ax + fwdZ * ay;

            // Normalize world move dir (already unit because basis is orthonormal + normalized input)
            float speed = running ? kRunSpeed : kWalkSpeed;

            // 5) Integrate position with boundaries
            gCharX += dx * speed * dt;
            gCharZ += dz * speed * dt;
            if (gCharX < kWorldMinX) gCharX = kWorldMinX;
            if (gCharX > kWorldMaxX) gCharX = kWorldMaxX;
            if (gCharZ < kWorldMinZ) gCharZ = kWorldMinZ;
            if (gCharZ > kWorldMaxZ) gCharZ = kWorldMaxZ;

            // 6) Auto-face the movement heading (smoothly)
            float targetYaw = atan2f(dx, dz) * 180.0f / 3.1415926f; // atan2(x,z)
            float maxTurn = (running ? kFaceTurnRun : kFaceTurnWalk) * dt;
            float dYaw = AngleDeltaDeg(gCharYawDeg, targetYaw);
            if (dYaw > maxTurn) dYaw = maxTurn;
            if (dYaw < -maxTurn) dYaw = -maxTurn;
            gCharYawDeg += dYaw;
        }

        // Keep gait phase running while walking is enabled
        float gaitHz = walkSpeed * (running ? kRunSpeedMul : 1.0f);
        walkPhase += 6.2831853f * gaitHz * dt;
    }
}

// ------------------------------------------------------------
// Small reset: arms/legs back to neutral (pre-animation angles)
// ------------------------------------------------------------
inline void ResetArmsAndLegsToNeutral()
{
    // Arm manual lifts (E/R)
    gUpperLiftLevelL = gUpperLiftLevelR = 0;
    gLowerLiftLevelL = gLowerLiftLevelR = 0;

    // Leg manual lifts (T/Y/U)
    gUpperLegLevelL = gUpperLegLevelR = 0;
    gLowerLegLevelL = gLowerLegLevelR = 0;
    gFootTipLevelL = gFootTipLevelR = 0;
}

// ------------------------------------------------------------
// Big reset: mostly all body movement/animation state
// (keeps camera/lighting unless you also reset them outside)
// ------------------------------------------------------------
inline void ResetCharacterMovementState()
{
    // Gait / state machines
    walking = false;
    running = false;
    waving = false;

    combatTarget = false;
    combatBlend = 0.0f;
    combatBobPh = 0.0f;

    // Weapon
    weaponState = WPN_IDLE;
    weaponTimer = 0.0f;
    weaponSpin = 0.0f;

    // Attack
    attackState = ATK_IDLE;
    attackTimer = 0.0f;
    attackSwingDeg = 0.0f;

    // FX
    gFXEnabled = false;

    // Phases
    walkPhase = 0.0f;
    wavePhase = 0.0f;

    // Manual limb overrides & hand poses
    ResetArmsAndLegsToNeutral();
}


// =================================================
// =============      WINDOW PROC       ============
// =================================================
LRESULT WINAPI WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        Music_Stop();
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        // -------- User Manual Page --------
        if (wParam == 'M') { gShowManual = !gShowManual; return 0; }
        if (gShowManual) {
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;
        }

        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);

        // --- Projection Toggle (Perspective / Orthographic) ---
        else if (wParam == 'P') {
            gPerspective = !gPerspective;
            ApplyProjection();
        }

        // --- Reset --- 
        else if (wParam == VK_SPACE) {
            // Body/animation reset
            ResetCharacterMovementState();

            // Reset view angles
            angleX = 0.0f;
            angleY = 0.0f;

            // Reset character placement
            gCharX = 0.0f; gCharZ = 0.0f; gCharYawDeg = 0.0f;

            // Reset hand pose
            gLeftHandPose = HANDPOSE_NONE;
            gRightHandPose = HANDPOSE_NONE;
            gLeftHandPoseBlend = 0.0f;   // kill any in-flight blend
            gRightHandPoseBlend = 0.0f;

            // Reset Lighting
            gLightYawDeg = 0.0f;
            gAmbientOn = true;
            gDiffuseOn = true;
            gSpecularOn = true;

            // Reset Thunder Effect
            _th().on = false;     // turn it off
            _th().tri = false;    // default to normal mode

            // Reset camera
            gCamX = 0.0f; gCamY = 4.0f; gCamZ = 18.0f;
            gCamTargetX = 0.0f; gCamTargetY = 3.0f; gCamTargetZ = 0.0f;
            gPerspective = true;
            ApplyProjection();
            Music_Stop();
        }

        // ========= Animation ==========
        // --- Walking ---
        else if (wParam == 'Z') {
            ResetArmsAndLegsToNeutral();
            walking = !walking;
            gRibbonWind = true;
        }

        // --- Wave hand (say Hi) ---
        else if (wParam == 'X')
            waving = !waving;

        // --- Fighting pose (target) ---
        else if (wParam == 'C') {
            ResetArmsAndLegsToNeutral();
            combatTarget = !combatTarget;
            if (combatTarget) {
                walking = false;
                waving = false;
                running = false;
            }
        }
        // --- Running ---
        else if (wParam == 'V') {
            ResetArmsAndLegsToNeutral();
            running = !running;
            if (running) {
                walking = true;    // run piggybacks on walking gait phase
                gRibbonWind = true;
                waving = false;   // don't wave while running
                combatTarget = false;
                combatBlend = 0.0f;
                combatBobPh = 0.0f;
            }
        }
        // --- Weapon rotate and hold ---
        else if (wParam == 'B') {
            ResetArmsAndLegsToNeutral();
            // start at front, spinning
            weaponState = WPN_RAISE_SPIN;
            weaponTimer = 0.0f;
            weaponSpin = 0.0f;

            // make sure we’re not running conflicting anims
            waving = false;
            // running = false;
            // you can leave walking on/off; this sequence overrides the left arm below
        }
        // --- Attack ---
        else if (wParam == 'N') {
            ResetArmsAndLegsToNeutral();
            attackState = ATK_WINDUP;
            attackTimer = 0.0f;
            attackSwingDeg = 0.0f;

            // avoid conflicting loops while attacking
            waving = false;
            // running = false;
            // walking = false;
        }

        // =============== Body Movement ================
        // --- RIGHT/LEFT hand: Third-Eye Pose (Q) / Thumbs Up (W) ---
        else if (wParam == 'Q') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            HandPose& hp = left ? gLeftHandPose : gRightHandPose;
            hp = (hp == HANDPOSE_PEACE) ? HANDPOSE_NONE : HANDPOSE_PEACE;
            waving = false; // avoid conflict with right-hand waving
        }
        else if (wParam == 'W') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            HandPose& hp = left ? gLeftHandPose : gRightHandPose;
            hp = (hp == HANDPOSE_THUMBSUP) ? HANDPOSE_NONE : HANDPOSE_THUMBSUP;
            waving = false;
        }
        // --- Upper arm (shoulder) lift cycle on 'E' ---
        else if (wParam == 'E') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int& lvl = left ? gUpperLiftLevelL : gUpperLiftLevelR;
            lvl = (lvl + 1) % 4; // 0→1→2→3→0
            char buf[64]; wsprintfA(buf, left ? "UpperLift L: %d\n" : "UpperLift R: %d\n", lvl);
            OutputDebugStringA(buf);
        }
        // --- Lower arm (elbow) lift cycle on 'R' ---
        else if (wParam == 'R') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int& lvl = left ? gLowerLiftLevelL : gLowerLiftLevelR;
            lvl = (lvl + 1) % 4; // 0→1→2→3→0
            char buf[64]; wsprintfA(buf, left ? "LowerLift L: %d\n" : "LowerLift R: %d\n", lvl);
            OutputDebugStringA(buf);
        }
        // --- Upper leg (hip) lift cycle on 'T' ---
        else if (wParam == 'T') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int& lvl = left ? gUpperLegLevelL : gUpperLegLevelR;
            lvl = (lvl + 1) % 4;
            char buf[64]; wsprintfA(buf, left ? "Hip L: %d\n" : "Hip R: %d\n", lvl);
            OutputDebugStringA(buf);
        }
        // --- Lower leg (knee) lift cycle on 'Y' ---
        else if (wParam == 'Y') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int& lvl = left ? gLowerLegLevelL : gLowerLegLevelR;
            lvl = (lvl + 1) % 4;
            char buf[64]; wsprintfA(buf, left ? "Knee L: %d\n" : "Knee R: %d\n", lvl);
            OutputDebugStringA(buf);
        }
        // --- Foot tip (ankle/toe) cycle on 'U' ---
        else if (wParam == 'U') {
            bool left = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            int& lvl = left ? gFootTipLevelL : gFootTipLevelR;
            lvl = (lvl + 1) % 4;
            char buf[64]; wsprintfA(buf, left ? "FootTip L: %d\n" : "FootTip R: %d\n", lvl);
            OutputDebugStringA(buf);
        }
        // --- Shake head ---
        else if (wParam == 'I') {  // shake twice
            gHeadAnim = HEAD_SHAKE;
            gHeadTimer = 0.0f;
        }
        // --- Nod head ---
        else if (wParam == 'O') {  // nod twice
            gHeadAnim = HEAD_NOD;
            gHeadTimer = 0.0f;
        }

        // ========= Head (Ribbon) ==========
        else if (wParam == 'A') {
            // ribbon movement
            gRibbonWind = !gRibbonWind;
            if (!gRibbonWind) gRibbonWind = 0.0f;
            return 0;
        }

        // ========= Head (Eyes Color) ==========
        // ---- iris color ----
        else if (wParam == '8') {
            // yellow-brown
            irisR = 0.95f;  irisG = 0.55f;  irisB = 0.10f;
            gThirdEyeOpen = !gThirdEyeOpen;
            InvalidateRect(hWnd, nullptr, FALSE);  // request redraw
            gMouthCurv = 0.0f;
        }
        else if (wParam == '9') {
            // back to blue (optional)
            irisR = 0.15f;  irisG = 0.45f;  irisB = 0.95f;
            gThirdEyeOpen = false;
            InvalidateRect(hWnd, nullptr, FALSE);
            gMouthCurv = -0.01f;
        }

        // ========= Special Effect (Electrical) =======
        else if (wParam == 'S') {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                gCamY -= gCamSpeed;
                gCamTargetY -= gCamSpeed;
            }
            else {
                gFXEnabled = !gFXEnabled;            // <-- toggle FX
                // Optional: quick console/debug note
                OutputDebugStringA(gFXEnabled ? "FX: ON\n" : "FX: OFF\n");
            }
        }
        // ========== Special Effect (Thunder) ==========
        // toggles a “thunder/lightning aura” visual effect
        else if (wParam == 'D') {  
            auto& s = _th();
            s.on = !s.on;
            if (!s.on) s.a = 0.0f;           // instant stop
            return 0;
        }
        // triangle-shaped glowing sigil
        else if (wParam == 'F') {
            _th().tri = !_th().tri;
            return 0;
        }

        // ========== Lighting Setup ==========
        else if (wParam == 'J') gAmbientOn = !gAmbientOn;
        else if (wParam == 'K') gDiffuseOn = !gDiffuseOn;
        else if (wParam == 'L') gSpecularOn = !gSpecularOn;

        // --- Light orbit left/right ---
        // ';' (colon) : rotate light LEFT around the character
        else if (wParam == VK_OEM_1) {
            float step = (GetKeyState(VK_SHIFT) & 0x8000) ? 18.0f : 8.0f; // Shift = bigger step
            gLightYawDeg -= step;
            if (gLightYawDeg < -360.0f) gLightYawDeg += 360.0f;
        }
        // ''' (single quote): rotate light RIGHT around the character
        else if (wParam == VK_OEM_7) {
            float step = (GetKeyState(VK_SHIFT) & 0x8000) ? 18.0f : 8.0f;
            gLightYawDeg += step;
            if (gLightYawDeg > 360.0f) gLightYawDeg -= 360.0f;
        }

        // ==== Texture Wear ====
        else if (wParam == '1') {
            gTexTheme = 0;   // Chinese armor set
            gBodyColorTheme = 0;
        }
        else if (wParam == '2') {
            gTexTheme = 1;   // Armor set
            gBodyColorTheme = 1;
        }
        else if (wParam == '3') {
            gTexTheme = 2;          // Blue set
            gBodyColorTheme = 2;
        }
        else if (wParam == '4') {  // Orange set
            gTexTheme = 3;
            gBodyColorTheme = 3;
        }
        // ==== Texture Background ====
        else if (wParam == '5') { gSkyBGIndex = 0; /* background1 */ }
        else if (wParam == '6') { gSkyBGIndex = 1; /* background2 */ }
        else if (wParam == '7') { gSkyBGIndex = 2; /* background3 */ }

        // ============ Background Music ============
        else if (wParam == 'H') { // H = background music toggle
            if (gMusicOn) Music_Stop();
            else          Music_StartMp3("bgm.mp3"); 
        }

        // ---- Smooth zoom with +/- (main keyboard and numpad) ----
        else if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            float step = (GetKeyState(VK_SHIFT) & 0x8000) ? 2.0f * gCamSpeed : gCamSpeed;
            gCamZ -= step;
            gCamTargetZ -= step;
        }
        else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            float step = (GetKeyState(VK_SHIFT) & 0x8000) ? 2.0f * gCamSpeed : gCamSpeed;
            gCamZ += step;
            gCamTargetZ += step;
        }
        // 
        else if (wParam == VK_OEM_2) {  // '/' key OR '?' key
            gShowAxes = !gShowAxes;
        }
        break;

    case WM_SIZE: {      // <— RESIZE WINDOW TAB flexible viewport
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        OnResize(w, h);
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam); // +120 up, -120 down
        float step = ((delta < 0) ? +1.0f : -1.0f) * (2.0f * gCamSpeed);
        gCamZ += step;
        gCamTargetZ += step;
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (gShowManual) { gShowManual = false; return 0; }
        gRotating = true;
        gLastX = GET_X_LPARAM(lParam);
        gLastY = GET_Y_LPARAM(lParam);
        SetCapture(hWnd);                 // capture mouse while dragging
        return 0;
    }
    case WM_RBUTTONDOWN: {
        if (gShowManual) { gShowManual = false; return 0; }
        gPanning = true;
        gLastX = GET_X_LPARAM(lParam);
        gLastY = GET_Y_LPARAM(lParam);
        SetCapture(hWnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        gRotating = false;
        if (!gPanning) ReleaseCapture();
        return 0;
    }
    case WM_RBUTTONUP: {
        gPanning = false;
        if (!gRotating) ReleaseCapture();
        return 0;
    }

    // ---- Camera movement with left click ----
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int dx = x - gLastX;
        const int dy = y - gLastY;

        // Camera direction with right click
        if (gRotating) {
            // Left drag = rotate the view
            angleX += dx * kRotateDegPerPixel;   // yaw (left/right)
            angleY += dy * kRotateDegPerPixel;   // pitch (up/down)
        }

        if (gPanning) {
            // Right drag = move the camera (pan)
            // Compute world-units per pixel from perspective frustum
            float aspect = (gHeight > 0) ? (float)gWidth / (float)gHeight : 1.0f;
            float dist = fabsf(gCamZ - gCamTargetZ);            // eye→target distance
            float halfH = (float)tan(0.5f * gFovY * 3.1415926f / 180.0f) * dist;
            float halfW = halfH * aspect;
            float viewW = 2.0f * halfW;                          // width at target depth
            float viewH = 2.0f * halfH;

            // Convert pixels → world; "grab the scene": camera moves opposite to the mouse
            float panX = -(dx / (float)gWidth) * viewW;
            float panY = (dy / (float)gHeight) * viewH;

            gCamX += panX;  gCamTargetX += panX;
            gCamY += panY;  gCamTargetY += panY;
        }

        gLastX = x; gLastY = y;
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
//--------------------------------------------------------------------

bool initPixelFormat(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

    pfd.cAlphaBits = 8;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;

    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;

    // choose pixel format returns the number most similar pixel format available
    int n = ChoosePixelFormat(hdc, &pfd);

    // set pixel format returns whether it successfully set the pixel format
    if (SetPixelFormat(hdc, n, &pfd))
    {
        return true;
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------
// === Core Primitives and Joints ===
void drawJoint(float radius = 0.15f)
{
    GLUquadric* joint = gluNewQuadric();
    glColor3f(0.965f, 0.776f, 0.584f); // Red joint
    gluSphere(joint, radius, 16, 16);
    gluDeleteQuadric(joint);
}

void drawPolygonPlate(GLfloat vertices[][2], int vertexCount,
    float depth,
    float frontColor[3],
    float backColor[3],
    float sideColor[3])
{
    // Front face
    glColor3fv(frontColor);
    glBegin(GL_POLYGON);
    for (int i = 0; i < vertexCount; i++)
        glVertex3f(vertices[i][0], vertices[i][1], 0.0f);
    glEnd();

    // Back face
    glColor3fv(backColor);
    glBegin(GL_POLYGON);
    for (int i = 0; i < vertexCount; i++)
        glVertex3f(vertices[i][0], vertices[i][1], -depth);
    glEnd();

    // Side faces
    glColor3fv(sideColor);
    glBegin(GL_QUADS);
    for (int i = 0; i < vertexCount; i++) {
        int next = (i + 1) % vertexCount;
        glVertex3f(vertices[i][0], vertices[i][1], 0.0f);
        glVertex3f(vertices[next][0], vertices[next][1], 0.0f);
        glVertex3f(vertices[next][0], vertices[next][1], -depth);
        glVertex3f(vertices[i][0], vertices[i][1], -depth);
    }
    glEnd();
}

void drawCube(float size)
{
    float half = size / 2.0f;

    glBegin(GL_QUADS);
    // Front face
    glVertex3f(-half, -half, half);
    glVertex3f(half, -half, half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);

    // Back face
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, -half, -half);

    // Top face
    glVertex3f(-half, half, -half);
    glVertex3f(-half, half, half);
    glVertex3f(half, half, half);
    glVertex3f(half, half, -half);

    // Bottom face
    glVertex3f(-half, -half, -half);
    glVertex3f(half, -half, -half);
    glVertex3f(half, -half, half);
    glVertex3f(-half, -half, half);

    // Right face
    glVertex3f(half, -half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, half, half);
    glVertex3f(half, -half, half);

    // Left face
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, -half, half);
    glVertex3f(-half, half, half);
    glVertex3f(-half, half, -half);
    glEnd();
}

void drawRing(float innerRadius, float outerRadius, float thickness, int segments = 32)
{
    float step = 2.0f * 3.1415926f / segments;

    for (int i = 0; i < segments; i++)
    {
        float theta = i * step;
        float nextTheta = (i + 1) * step;

        float cosT = cos(theta), sinT = sin(theta);
        float cosNext = cos(nextTheta), sinNext = sin(nextTheta);

        glBegin(GL_QUADS);
        // Outer wall
        glVertex3f(outerRadius * cosT, outerRadius * sinT, 0.0f);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, 0.0f);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, thickness);
        glVertex3f(outerRadius * cosT, outerRadius * sinT, thickness);

        // Inner wall (flip order)
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, 0.0f);
        glVertex3f(innerRadius * cosT, innerRadius * sinT, 0.0f);
        glVertex3f(innerRadius * cosT, innerRadius * sinT, thickness);
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, thickness);

        // Top face
        glVertex3f(innerRadius * cosT, innerRadius * sinT, thickness);
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, thickness);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, thickness);
        glVertex3f(outerRadius * cosT, outerRadius * sinT, thickness);

        // Bottom face
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, 0.0f);
        glVertex3f(innerRadius * cosT, innerRadius * sinT, 0.0f);
        glVertex3f(outerRadius * cosT, outerRadius * sinT, 0.0f);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, 0.0f);
        glEnd();
    }
}

// ================= Environment ==================
// ---------------------- SKYDOME ----------------------
static void drawSkyDome(float radius = 90.0f, int slices = 64, int stacks = 32)
{
    int w = 0, h = 0;
    GLuint& skyTex = gSkyTexes[gSkyBGIndex];
    LoadBMPTextureOnce(gSkyBGFiles[gSkyBGIndex], skyTex, &w, &h);

    // Clamp U to avoid a 1-pixel seam when width isn't perfect 2:1
    if (w > 0) gSkyUClamp = (w - 1) / float(w);
    gSkyVClamp = 1.0f;

    // Save state
    GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    GLboolean wasTex = glIsEnabled(GL_TEXTURE_2D);
    GLboolean wasLight = glIsEnabled(GL_LIGHTING);
    GLboolean depthWrite; glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);

    // Backdrop states
    glDisable(GL_LIGHTING);
    if (skyTex) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, skyTex);
    GLint prevEnvMode; glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &prevEnvMode);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); // ignore glColor
    glDisable(GL_CULL_FACE);     // draw both sides so inside is visible
    glDepthMask(GL_FALSE);       // don't write depth
    glColor3f(1, 1, 1);            // ensure no tint

    for (int i = 0; i < stacks; ++i) {
        float v0 = (float)i / stacks;
        float v1 = (float)(i + 1) / stacks;
        float th0 = v0 * kPif - 0.5f * kPif;   // [-pi/2, +pi/2]
        float th1 = v1 * kPif - 0.5f * kPif;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float u = (float)j / slices;
            if (gSkyFlipU) u = 1.0f - u;

            float ph = u * (2.0f * kPif);

            float c0 = cosf(th0), s0 = sinf(th0);
            float c1 = cosf(th1), s1 = sinf(th1);
            float cp = cosf(ph), sp = sinf(ph);

            float x0 = c0 * sp, y0 = s0, z0 = c0 * cp;
            float x1 = c1 * sp, y1 = s1, z1 = c1 * cp;

            glTexCoord2f(u * gSkyUTiles * gSkyUClamp, v0 * gSkyVTiles * gSkyVClamp);
            glVertex3f(radius * x0, radius * y0, radius * z0);

            glTexCoord2f(u * gSkyUTiles * gSkyUClamp, v1 * gSkyVTiles * gSkyVClamp);
            glVertex3f(radius * x1, radius * y1, radius * z1);
        }
        glEnd();
    }

    // Restore state
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, prevEnvMode);
    glDepthMask(depthWrite);
    if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (wasTex)  glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    if (wasLight)glEnable(GL_LIGHTING);    else glDisable(GL_LIGHTING);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Fallback color if texture missing (optional visual cue)
    if (!skyTex) { glDisable(GL_TEXTURE_2D); glColor3f(0.60f, 0.70f, 0.90f); }
}

// ---------------------- GROUND PLANE ----------------------
static void drawGroundPlane(float halfSize = 60.0f, float tileU = 24.0f, bool showGrid = false)
{
    int gw = 0, gh = 0;
    LoadBMPTextureOnce("ground.bmp", gGroundTex, &gw, &gh);
    if (gw > 0 && gh > 0) gGroundVTiling = (float)gh / (float)gw;  // aspect-aware tiling

    GLboolean wasTex = glIsEnabled(GL_TEXTURE_2D);
    GLboolean wasLight = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);

    if (gGroundTex) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, gGroundTex); glColor3f(1, 1, 1); }
    else { glDisable(GL_TEXTURE_2D); glColor3f(0.22f, 0.24f, 0.26f); }

    float tileV = tileU * gGroundVTiling;

    // solid quad
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);  glVertex3f(-halfSize, gGroundY, -halfSize);
    glTexCoord2f(tileU, 0.0f);  glVertex3f(+halfSize, gGroundY, -halfSize);
    glTexCoord2f(tileU, tileV); glVertex3f(+halfSize, gGroundY, +halfSize);
    glTexCoord2f(0.0f, tileV); glVertex3f(-halfSize, gGroundY, +halfSize);
    glEnd();

    // (optional) faint grid — OFF by default
    if (showGrid) {
        glDisable(GL_TEXTURE_2D);
        glLineWidth(1.0f);
        glColor3f(0.08f, 0.10f, 0.12f);
        const float step = 2.0f, gridLift = 0.002f;
        glBegin(GL_LINES);
        for (float x = -halfSize; x <= halfSize; x += step) {
            glVertex3f(x, gGroundY + gridLift, -halfSize);
            glVertex3f(x, gGroundY + gridLift, +halfSize);
        }
        for (float z = -halfSize; z <= halfSize; z += step) {
            glVertex3f(-halfSize, gGroundY + gridLift, z);
            glVertex3f(+halfSize, gGroundY + gridLift, z);
        }
        glEnd();
    }

    if (gGroundTex) glBindTexture(GL_TEXTURE_2D, 0);
    if (wasLight) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (wasTex)   glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
}

static void DrawEnvironment()
{
    // Ground settings (match what you pass to drawGroundPlane)
    const float halfSize = 60.0f;
    const float tileU = 4.0f;

    // Minimal radius that still encloses the ground corners:
    // sqrt(half^2 + half^2 + gGroundY^2) + small margin
    const float minRadius = sqrtf(halfSize * halfSize * 2.0f + gGroundY * gGroundY) + 1.0f;

    // Use the smaller user-picked radius, but never below what's needed to enclose the ground
    const float domeR = (gSkyRadius > minRadius) ? gSkyRadius : minRadius;

    // Draw sky dome without lighting
    glDisable(GL_LIGHTING);
    drawSkyDome(domeR, gSkySlices, gSkyStacks);   // your function that draws the sphere
    glEnable(GL_LIGHTING);

    drawGroundPlane(halfSize, tileU);
}


// (optional) call at shutdown if you want to free textures explicitly
static void Env_FreeTextures()
{
    for (int i = 0; i < 3; ++i) {
        if (gSkyTexes[i]) { glDeleteTextures(1, &gSkyTexes[i]); gSkyTexes[i] = 0; }
    }
    if (gGroundTex) { glDeleteTextures(1, &gGroundTex); gGroundTex = 0; }
}
// ============ Head Parts ============
// ========     Eyes Helper    =========
// ----------------EYES---------------------
static void drawEyePoly3D(const float (*v)[3], int n)
{
    const float GREY_TOP = 0.55f;  // 0.80–0.92 (higher = lighter grey)

    // find Y range to locate middle
    float minY = v[0][1], maxY = v[0][1];
    for (int i = 1; i < n; ++i) {
        if (v[i][1] < minY) minY = v[i][1];
        if (v[i][1] > maxY) maxY = v[i][1];
    }
    float midY = 0.55f * (minY + maxY);

    glBegin(GL_POLYGON);
    for (int i = 0; i < n; ++i) {
        float y = v[i][1];

        float r, g, b;
        if (y <= midY) {
            // entire bottom half: pure white
            r = g = b = 1.0f;
        }
        else {
            // upper half: middle grey -> top white
            float t = (y - midY) / (maxY - midY);      // 0 at middle, 1 at top
            if (t < 0) t = 0; if (t > 1) t = 1;
            float wGrey = (1.0f - t);                  // 1 at middle, 0 at top
            float c = wGrey * GREY_TOP + (1.0f - wGrey) * 1.0f;
            r = g = b = c;
        }

        glColor3f(r, g, b);
        glVertex3f(v[i][0], v[i][1], v[i][2]);
    }
    glEnd();
    // outline (black)
    glLineWidth(2);
    glColor3f(0, 0, 0);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < n; ++i)
        glVertex3f(v[i][0], v[i][1], v[i][2]);
    glEnd();
    glLineWidth(1);
}

// Left eye vertices (counter-clockwise)
static float EYE_L[6][3] = {
    {-0.25f, 0.02f,-0.05f},
    {-0.20f,  0.12f, -0.05f},  // near brow-side corner
    { 0.05f,  0.04f,0.0f},
    { 0.10f,  -0.06f,0.0f},
    { 0.05f, -0.16f,0.0f},
    {-0.19f, -0.10f,-0.05f}
};

// Right eye vertices 
static float EYE_R3D[6][3] = {
    { 0.19f, -0.10f, -0.05f},
    {-0.05f, -0.16f,  0.00f},
    {-0.10f, -0.06f,  0.00f},
    {-0.05f,  0.04f,  0.00f},
    { 0.20f,  0.12f, -0.05f},
    { 0.25f,  0.02f, -0.05f}
};

//----------------EYEBROW---------------
static void drawEyebrow(const float (*v)[3])
{
    glColor3f(0.1f, 0.05f, 0.02f);   // dark brown/black
    glBegin(GL_TRIANGLES);
    glVertex3f(v[0][0], v[0][1], v[0][2]);
    glVertex3f(v[1][0], v[1][1], v[1][2]);
    glVertex3f(v[2][0], v[2][1], v[2][2]);
    glEnd();
}

// Left eyebrow (above EYE_L)
static float BROW_L[3][3] = {
    {-0.21f, 0.18f, -0.05f},  // left tip (outer)
    { 0.12f, 0.08f,  0.00f},  // right tip (inner, nose side)
    {-0.15f, 0.20f, -0.05f}   // middle peak (arched)
};

// Right eyebrow (mirror of left)
static float BROW_R[3][3] = {
    { 0.21f, 0.18f, -0.05f},  // right tip (outer)
    {-0.12f, 0.08f,  0.00f},  // left tip (inner, nose side)
    { 0.15f, 0.20f, -0.05f}   // middle peak
};

// ------------------- IRIS (semi-ellipse) ----------------------
// Semicircle (flat chord on top, arc down).
// raiseSide: -1 = raise/push LEFT edge, +1 = raise/push RIGHT edge, 0 = none
static void drawSemiDiscLR(float rx, float ry, float yOfs, float zLift,
    int segs,
    float r, float g, float b,
    float tiltAmt, float backAmt, int raiseSide)
{
    const float PI = 3.1415926535f;

    // Chord endpoints (apply lift/back on chosen side)
    float yLeft = yOfs + ((raiseSide < 0) ? tiltAmt : 0.0f);
    float yRight = yOfs + ((raiseSide > 0) ? tiltAmt : 0.0f);
    float zLeft = zLift + ((raiseSide < 0) ? -backAmt : 0.0f);
    float zRight = zLift + ((raiseSide > 0) ? -backAmt : 0.0f);

    // ---------- FILL ----------
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    // chord
    glVertex3f(-rx, yLeft, zLeft);
    glVertex3f(+rx, yRight, zRight);

    // arc 0° → 180° (downward semi)
    for (int i = 0; i <= segs; ++i) {
        float t = (float)i / (float)segs;
        float ang = PI * t;
        float x = rx * cosf(ang);
        float y = yOfs - ry * sinf(ang);
        float z = zLift;

        if (raiseSide < 0 && x < 0.0f) { float f = (-x / rx); y += f * tiltAmt; z -= f * backAmt; }
        if (raiseSide > 0 && x > 0.0f) { float f = (x / rx); y += f * tiltAmt; z -= f * backAmt; }

        glVertex3f(x, y, z);
    }
    glEnd();

    glLineWidth(1.5f);
    glColor3f(0.05f, 0.15f, 0.35f);
    glBegin(GL_LINE_STRIP);
    // chord (same Y/Z as fill)
    glVertex3f(-rx, yLeft, zLeft + 0.0001f);
    glVertex3f(+rx, yRight, zRight + 0.0001f);

    // arc (same math as fill)
    for (int i = 0; i <= segs; ++i) {
        float t = (float)i / (float)segs;
        float ang = PI * t;
        float x = rx * cosf(ang);
        float y = yOfs - ry * sinf(ang);
        float z = zLift;

        if (raiseSide < 0 && x < 0.0f) { float f = (-x / rx); y += f * tiltAmt; z -= f * backAmt; }
        if (raiseSide > 0 && x > 0.0f) { float f = (x / rx); y += f * tiltAmt; z -= f * backAmt; }

        glVertex3f(x, y, z + 0.0001f);   // tiny lift to avoid z-fighting
    }
    glEnd();
    glLineWidth(1.0f);
}

// ----------- HIGHLIGHT-----------------
static void drawHighlightRect(float w, float h, float xOfs, float yOfs, float zLift)
{
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(xOfs - w * 0.5f, yOfs + h * 0.5f, zLift);
    glVertex3f(xOfs + w * 0.5f, yOfs + h * 0.5f, zLift);
    glVertex3f(xOfs + w * 0.5f, yOfs - h * 0.5f, zLift);
    glVertex3f(xOfs - w * 0.5f, yOfs - h * 0.5f, zLift);
    glEnd();
}
// ------------ THIRDEYE 天眼 ------------------
static void DrawThirdEye(float yEyes, float zFace)
{
    glPushMatrix();
    glTranslatef(0.0f, yEyes + gThirdEyeYOffset, zFace + gThirdEyeZLift);
    glColor3f(0.55f, 0.0f, 0.0f);   // dark red

    if (!gThirdEyeOpen) {
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(0.0f, 0.028, 0.015f); // top
        glVertex3f(0.0015, 0.0f, 0.015f); // right
        glVertex3f(0.0f, -0.028, 0.015f); // bottom
        glVertex3f(-0.0015, 0.0f, 0.015f); // left
        glEnd();
        glLineWidth(1.0f);
    }
    else {
        // open: diamond outline
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(0.0f, +gThirdEyeHalfH, 0.015f); // top
        glVertex3f(+gThirdEyeHalfW, 0.0f, 0.015f); // right
        glVertex3f(0.0f, -gThirdEyeHalfH, 0.015f); // bottom
        glVertex3f(-gThirdEyeHalfW, 0.0f, 0.015f); // left
        glEnd();
        glLineWidth(1.0f);

        // 2) Inner red pupil (small filled circle)
        float rx = 0.014f; // radius x
        float ry = 0.024f; // radius y
        glColor3f(1.0f, 0.0f, 0.0f); // bright red fill
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, 0.0f, 0.015f); // center
        for (int i = 0; i <= 36; ++i) {
            float ang = (2.0f * 3.1415926f * i) / 36;
            float x = rx * cosf(ang);
            float y = ry * sinf(ang);
            glVertex3f(x, y, 0.001f);
        }
        glEnd();
    }
    glPopMatrix();
}

// ========     Mouth Helper    =========
static void drawMouthLine(float halfW, float curv, float zLift, int segs,
    float r, float g, float b)
{
    glColor3f(r, g, b);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segs; ++i) {
        float t = (float)i / (float)segs;           // 0..1
        float x = -halfW + (2.0f * halfW) * t;      // -halfW..+halfW
        float s = (t - 0.5f) * 2.0f;                // -1..+1
        float y = curv * (1.0f - s * s);            // parabola (smile/frown)
        glVertex3f(x, y, zLift);
    }
    glEnd();
    glLineWidth(1.0f);
}

// ========     Helmet Helper    =========
// 6-edge polygon helmet with tapered sides (top smaller, bottom bigger)
static void drawHelmetHexTaper(float bottomR, float topR, float height)
{
    const int N = 6;
    const float PI = 3.1415926535f;

    float bx[N], bz[N]; // bottom ring
    float tx[N], tz[N]; // top ring

    for (int i = 0; i < N; ++i) {
        float ang = (2.0f * PI * i) / N + PI / 2.0f; // forward-facing
        bx[i] = bottomR * cosf(ang);
        bz[i] = bottomR * sinf(ang);
        tx[i] = topR * cosf(ang);
        tz[i] = topR * sinf(ang);
    }

    // --- Side walls ---
    glColor3f(0.25f, 0.25f, 0.28f); // metal gray
    glBegin(GL_QUADS);
    for (int i = 0; i < N; ++i) {
        int j = (i + 1) % N;
        glVertex3f(bx[i], 0.0f, bz[i]);        // bottom
        glVertex3f(bx[j], 0.0f, bz[j]);
        glVertex3f(tx[j], height, tz[j]);      // top
        glVertex3f(tx[i], height, tz[i]);
    }
    glEnd();

    // --- Top cap (smaller hex) ---
    glColor3f(0.30f, 0.30f, 0.33f);
    glBegin(GL_POLYGON);
    for (int i = 0; i < N; ++i)
        glVertex3f(tx[i], height, tz[i]);
    glEnd();

    // --- Outline ---
    glColor3f(0, 0, 0);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP); // bottom
    for (int i = 0; i < N; ++i)
        glVertex3f(bx[i], 0.0f, bz[i]);
    glEnd();
    glBegin(GL_LINE_LOOP); // top
    for (int i = 0; i < N; ++i)
        glVertex3f(tx[i], height, tz[i]);
    glEnd();
    glLineWidth(1.0f);
}

static void drawPoly3D(const float (*v)[3], int n, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    for (int i = 0; i < n; ++i) glVertex3f(v[i][0], v[i][1], v[i][2]);
    glEnd();

    glLineWidth(2);
    glColor3f(0, 0, 0);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < n; ++i) glVertex3f(v[i][0], v[i][1], v[i][2]);
    glEnd();
    glLineWidth(1);
}

// TOP (5 edges) 
static float HELM_TOP5[5][3] = {
    {-0.10f, 0.02f, -0.05f},
    {-0.05f, 0.04f, 0.00f},
    {0.0f, 0.005f, 0.0f},
    {0.0f, -0.18f, 0.0f},
    {-0.11f, -0.06f, -0.05f}
};

// MIDDLE (4 edges)
static float HELM_MID4[4][3] = {
    {-0.12f, -0.02f, -0.05f},
    {0.0f, -0.15f, 0.00f},
    {0.0f,-0.17f, 0.00f},
    {-0.12f,-0.09f, -0.05f}
};

// BOTTOM (5 edges)
static float HELM_BOT5[5][3] = {
    {-0.14f, -0.12f, -0.05f},
    {0.0f, -0.20f, 0.0f},
    {0.0f, -0.22f, 0.0f},
    {-0.1f,-0.21f, -0.05f},
    {-0.145f,-0.16f, -0.05f}
};

// BACK VIEW OF HELMET (TO COVER THE FACT HE IS BOTAK)
static float HELM_BACK[4][3] = {
    {-0.245f, -0.25f, -0.00f},
    {0.245f, -0.25f, 0.00f},
    {0.245f,-0.65f, -0.14f},
    {-0.245f,-0.65f, -0.14f}
};

// Draw a flat diamond (4 points) centered at origin
static void drawDiamond(float w, float h, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    glVertex3f(0.0f, +h, 0.0f);  // top
    glVertex3f(+w, 0.0f, 0.0f);   // right
    glVertex3f(0.0f, -h, 0.0f);  // bottom
    glVertex3f(-w, 0.0f, 0.0f);   // left
    glEnd();

    // outline
    glColor3f(0, 0, 0);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0.0f, +h, 0.001f);
    glVertex3f(+w, 0.0f, 0.001f);
    glVertex3f(0.0f, -h, 0.001f);
    glVertex3f(-w, 0.0f, 0.001f);
    glEnd();
    glLineWidth(2);
}
// Crystal shard (pentagon): base-left, base-right, neck-right, tip, neck-left
// wBase: base width, hBody: height to neck, hTip: tip height
// neckFrac: 0..1 (neck width as a fraction of base) -> try 0.55..0.75
// skewX: small left/right slant, zLift: tiny lift to sit above panel
static void drawCrystalShardEx(float wBase, float hBody, float hTip,
    float neckFrac, float skewX, float zLift,
    float r, float g, float b)
{
    float halfB = 0.5f * wBase;
    float halfN = 0.5f * (wBase * neckFrac);

    // vertices (CCW)
    struct V { float x, y, z; } v[5] = {
        {-halfB, 0.0f, zLift},                  // base L
        {+halfB, 0.0f, zLift},                  // base R
        {+halfN + skewX, hBody, zLift},         // neck R (wider now)
        {0.0f + skewX, hBody + hTip, zLift},  // tip
        {-halfN + skewX, hBody, zLift}          // neck L
    };

    glColor3f(r, g, b);
    glBegin(GL_POLYGON); for (int i = 0; i < 5; ++i) glVertex3f(v[i].x, v[i].y, v[i].z); glEnd();

    glColor3f(0, 0, 0);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP); for (int i = 0; i < 5; ++i) glVertex3f(v[i].x, v[i].y, v[i].z + 0.0001f); glEnd();
    glLineWidth(1);
}
// Draw an N-vertex polygon stuck to the YZ plane at X = xPos.
// v[i][0] = Y, v[i][1] = Z  (simple 2D points)
// Tip: give the points CCW order when viewed from +X.
static void drawSidePlateYZ(const float (*v)[2], int n, float xPos,
    float r, float g, float b)
{
    // avoid issues if your winding flips
    GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
    if (wasCull) glDisable(GL_CULL_FACE);

    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    for (int i = 0; i < n; ++i)
        glVertex3f(xPos, v[i][0], v[i][1]);
    glEnd();

    glColor3f(0, 0, 0);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < n; ++i)
        glVertex3f(xPos, v[i][0], v[i][1] + 0.0001f); // tiny lift to avoid z-fight
    glEnd();
    glLineWidth(1.0f);

    if (wasCull) glEnable(GL_CULL_FACE);
}

// YZ points only (edit these freely)
// Order them CCW when looking from the character's RIGHT (+X) side.
static float SIDE7_YZ[7][2] = {
    { 0.23f, 0.33f},
    { 0.25f, 0.0f},
    { -0.14f, -0.14f},
    { -0.17f,  -0.12f},
    {-0.13f,  0.1f},
    {0.1f, 0.25f},
    {0.17f, 0.3f},
};

// 5-edge polygon you can tweak (x,y,z per vertex, CCW order)
static float PENTA5[5][3] = {
    {-0.08f,  -0.14f, 0.0f},   // mid-left
    {-0.10f, -0.08f, 0.0f},  // bottom-left
    { 0.10f, -0.06f, -0.25f},  // bottom-right
    { 0.14f,  0.06f, -0.30f},  // mid-right
    { 0.29f,  -0.02f, -0.30f}  // top
};

static void drawPenta(const float (*v)[3], float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 5; ++i) glVertex3f(v[i][0], v[i][1], v[i][2]);
    glEnd();

    glColor3f(0, 0, 0);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 5; ++i) glVertex3f(v[i][0], v[i][1], v[i][2] + 0.0001f);
    glEnd();
    glLineWidth(1);
}

// ------------- RIBBON HELPER ------------
/*** Small internal clock (advances when wind is on) ***/
static void Ribbon_UpdateClock()
{
    // Win32 timeGetTime() or GetTickCount() both fine; we only need delta
    static DWORD prev = timeGetTime();
    DWORD now = timeGetTime();
    float dt = (now - prev) * (1.0f / 1000.0f);
    prev = now;

    if (gRibbonWind)
        gRibbonTime += dt;
}

/*** Minimal unit box (centered at origin) for segments ***/
static void unitBox()
{
    glBegin(GL_QUADS);
    // +Y
    glNormal3f(0, 1, 0);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    // -Y
    glNormal3f(0, -1, 0);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    // +X
    glNormal3f(1, 0, 0);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    // -X
    glNormal3f(-1, 0, 0);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    // +Z
    glNormal3f(0, 0, 1);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    // -Z
    glNormal3f(0, 0, -1);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glEnd();
}

/*** One ribbon chain (4 segments) ***/
static void drawRibbonChain(float basePhase, float sideSign)
{
    const float sideYawDeg = 6.0f * sideSign;  // flare outward
    glPushMatrix();
    glRotatef(sideYawDeg, 0, 1, 0);

    // wind oscillation base
    float phase0 = 2.0f * 3.1415926f * 0.9f * gRibbonTime + basePhase;

    for (int i = 0; i < kSegs; ++i) {
        // --- Motion per segment ---
        float sag = kBaseSagDeg * (1.0f - 0.18f * i);          // gravity bend
        float amp = kAmpDeg * powf(kDampPerSeg, (float)i);     // amplitude damping
        float delay = i * 0.5f;                                  // phase lag down the chain
        float swing = (gRibbonWind ? amp : 0.0f) *
            sinf(phase0 - delay);                      // sway with lag

        // Apply rotations
        glRotatef(sag, 1, 0, 0);             // sag forward
        glRotatef(swing, 0, 0, 1);           // wind swing sideways

        // --- Geometry of this segment ---
        glPushMatrix();
        glTranslatef(0.0f, -0.5f * kRibbonLen, 0.0f);
        glScalef(kRibbonThk, kRibbonLen, kRibbonThk);
        float baseRed = 0.55f;      // dark red start
        float shade = baseRed + 0.10f * i;
        glColor3f(shade, 0.0f, 0.0f);
        unitBox();
        glPopMatrix();

        // move pivot down for next segment
        glTranslatef(0.0f, -kRibbonLen, 0.0f);
    }
    glPopMatrix();
}


/*** Spawns all 4 ribbons (2 per side) at head space anchors ***/
static void drawHelmetRibbons()
{
    // Two on the LEFT
    for (int i = 0; i < 2; ++i) {
        glPushMatrix();
        float spread = (i ? kSideSpread : -kSideSpread);
        glTranslatef(kAnchorX_L - spread, kAnchorY, kAnchorZ);
        drawRibbonChain(/*basePhase=*/0.35f * i, /*sideSign=*/-1.0f);
        glPopMatrix();
    }
    // Two on the RIGHT
    for (int i = 0; i < 2; ++i) {
        glPushMatrix();
        float spread = (i ? kSideSpread : -kSideSpread);
        glTranslatef(kAnchorX_R + spread, kAnchorY, kAnchorZ);
        drawRibbonChain(/*basePhase=*/0.35f * (i + 1), /*sideSign=*/+1.0f);
        glPopMatrix();
    }
}

// -------------- METAL RINGS (hang points) ----------------
static void _DrawTorus(float R, float r, int rings, int sides)
{
    for (int i = 0; i < rings; ++i) {
        float th0 = (float)i * (2.0f * 3.1415926f / rings);
        float th1 = (float)(i + 1) * (2.0f * 3.1415926f / rings);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= sides; ++j) {
            float ph = (float)j * (2.0f * 3.1415926f / sides);

            float c0 = cosf(th0), s0 = sinf(th0);
            float c1 = cosf(th1), s1 = sinf(th1);
            float cp = cosf(ph), sp = sinf(ph);

            // slice th1
            glNormal3f(cp * c1, cp * s1, sp);
            glVertex3f((R + r * cp) * c1, (R + r * cp) * s1, r * sp);
            // slice th0
            glNormal3f(cp * c0, cp * s0, sp);
            glVertex3f((R + r * cp) * c0, (R + r * cp) * s0, r * sp);
        }
        glEnd();
    }
}

static void DrawRibbonRings()
{
    glMatrixMode(GL_MODELVIEW);

    // ---- ring sizing (tweak to fit your small-scale anchors) ----
    const float ringR = 0.05f; // ring radius (center → middle of tube)
    const float tubeR = 0.007f; // tube thickness
    const int   tessRings = 24;     // around the ring
    const int   tessSides = 16;     // around the tube
    const float outwardYaw = 8.0f;   // slight outward tilt per side (deg)

    // Center Y so *bottom* of ring touches the ribbon anchor at kAnchorY
    const float ringCenterY = kAnchorY + ringR;

    // Common metal color (enable color material in init if your lighting dulls it)
    glColor3f(0.88f, 0.88f, 0.90f);

    // ---- LEFT ring ----
    glPushMatrix();
    glTranslatef(kAnchorX_L, ringCenterY, kAnchorZ);
    glRotatef(-outwardYaw, 0, 1, 0);  // lean a bit outward
    glRotatef(90.0f, 0, 1, 0);        // make torus axis = +X → vertical ring in YZ
    _DrawTorus(ringR, tubeR, tessRings, tessSides);
    glPopMatrix();

    // ---- RIGHT ring ----
    glPushMatrix();
    glTranslatef(kAnchorX_R, ringCenterY, kAnchorZ);
    glRotatef(+outwardYaw, 0, 1, 0);
    glRotatef(90.0f, 0, 1, 0);
    _DrawTorus(ringR, tubeR, tessRings, tessSides);
    glPopMatrix();
}

// ============ Body Parts ============
// --------------------
// === Torso & Body ===
// --------------------
void drawBody() {
    glColor3fv(BODY_COLORS[gBodyColorTheme]);

    GLUquadric* torso = gluNewQuadric();
    gluCylinder(torso, 0.5f, 0.7f, 0.8f, 20, 20);
    gluDeleteQuadric(torso);
}

// --------------------
// === Head & Neck ===
// --------------------
void drawNeck()
{
    GLUquadric* quad = gluNewQuadric();
    glColor3f(1.0f, 0.8f, 0.6f); // Same skin tone
    gluCylinder(quad, 0.15f, 0.25f, 0.55f, 20, 20); // Thin vertical cylinder
    gluDeleteQuadric(quad);
}

// ====================================
// =========== Head and Face ==========
// ====================================
void drawHead()
{
    glPushMatrix();

    // global scaling
    glScalef(1.35f, 1.20f, 1.35f);

    // color palette for 5 sections
    const GLfloat C_forehead[3] = { 0.0f,0.0f,0.0f };
    const GLfloat C_eyes[3] = { 1.0f, 0.8f, 0.6f };
    const GLfloat C_nose[3] = { 1.0f, 0.8f, 0.6f };
    const GLfloat C_chin[3] = { 0.90f,0.72f,0.62f };
    const GLfloat C_under[3] = { 0.90f,0.72f,0.62f };

    const int N = 6;                 // hexagon profile (front view)
    const float PHASE = 3.14159f / 2;  // face points to +Z

    // y positions for each “ring”
    float y[6] = { 0.44f, 0.36f, 0.16f, 0.07f, -0.11f, -0.18f };

    // half-widths (X) at each y (front view) → controls trapezoids
    float rx[6] = { 0.24f, 0.28f, 0.26f, 0.24f, 0.14f, 0.05f };
    // depth (Z) thickness for each ring (side profile)
    float rz[6] = { 0.18f, 0.20f, 0.20f, 0.25f, 0.18f, 0.10f };

    struct V { float x, y, z; };
    V ring[6][N];

    // build 6 rings
    for (int k = 0; k < 6; k++) {
        for (int i = 0; i < N; i++) {
            float a = (2 * 3.14159f * i) / N + PHASE;
            float ca = cosf(a), sa = sinf(a);
            float x = rx[k] * ca;
            float z = rz[k] * sa;
            ring[k][i] = { x, y[k], z };
        }
    }

    auto drawBand = [&](int k, const GLfloat* col) {
        glColor3fv(col);
        glBegin(GL_QUADS);
        for (int i = 0; i < N; i++) {
            int j = (i + 1) % N;

            // compute outward normal in XZ plane
            auto setNormal = [&](const V& v) {
                float len = sqrtf(v.x * v.x + v.z * v.z);
                if (len > 0.0f)
                    glNormal3f(v.x / len, 0.0f, v.z / len);
                else
                    glNormal3f(0.0f, 1.0f, 0.0f);
                };

            setNormal(ring[k][i]);   glVertex3f(ring[k][i].x, ring[k][i].y, ring[k][i].z);
            setNormal(ring[k][j]);   glVertex3f(ring[k][j].x, ring[k][j].y, ring[k][j].z);
            setNormal(ring[k + 1][j]); glVertex3f(ring[k + 1][j].x, ring[k + 1][j].y, ring[k + 1][j].z);
            setNormal(ring[k + 1][i]); glVertex3f(ring[k + 1][i].x, ring[k + 1][i].y, ring[k + 1][i].z);
        }
        glEnd();
    };

    // draw 5 sections
    drawBand(0, C_forehead); // forehead (0→1)
    drawBand(1, C_eyes);     // eyes section (1→2)
    // =====  Place eyes (tweak these numbers) =====
    {
        GLboolean wasLight = glIsEnabled(GL_LIGHTING);
        glDisable(GL_LIGHTING);

        // Position on your head (edit these)
        float zFace = 0.185f;          // push into/away from face. start ~ front surface
        float yEyes = 0.23f;          // vertical position (between your y[1]=0.36 and y[2]=0.16)
        float xGap = 0.09f;          // left/right from center
        float sx = 0.42f, sy = 0.42f; // size of eye
        float rot = 0.0f;           // rotate eye in its plane (deg). +CCW

        // LEFT
        glPushMatrix();
        glTranslatef(-xGap, yEyes, zFace);
        glRotatef(rot, 0, 0, 1);
        glScalef(sx, sy, 1);
        drawEyePoly3D(EYE_L, 6);
        glPopMatrix();

        // RIGHT
        glPushMatrix();
        glTranslatef(+xGap, yEyes, zFace);
        glRotatef(-rot, 0, 0, 1);
        glScalef(sx, sy, 1);
        drawEyePoly3D(EYE_R3D, 6);
        glPopMatrix();

        // -----------------------Eyebrows------------------------------
        glPushMatrix();
        glTranslatef(-xGap, yEyes, zFace);
        glScalef(sx, sy, 1);
        drawEyebrow(BROW_L);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(+xGap, yEyes, zFace);
        glScalef(sx, sy, 1);
        drawEyebrow(BROW_R);
        glPopMatrix();

        // LEFT iris
        glPushMatrix();
        glTranslatef(-xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(-0.065f, 0.054f, 0.0f);  // your local nudge
        glScalef(0.60f, 1.0f, 1.0f);           // narrower
        drawSemiDiscLR(0.16f, 0.13f, 0.00f, 0.002f, 28, irisR, irisG, irisB, 0.062f, /*backAmt*/0.042f, /*raiseSide*/-1);
        glPopMatrix();
        glPopMatrix();

        // RIGHT iris (minor)
        glPushMatrix();
        glTranslatef(+xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(+0.065f, 0.054f, 0.0f);
        glScalef(0.60f, 1.0f, 1.0f);
        drawSemiDiscLR(0.16f, 0.13f, 0.00f, 0.002f, 28,
            irisR, irisG, irisB,
            /*tiltAmt*/0.062f, /*backAmt*/0.042f,
            /*raiseSide*/+1);
        glPopMatrix();
        glPopMatrix();

        //--------------------- black pupil -----------------------
        // ---- LEFT pupil ----
        glPushMatrix();
        glTranslatef(-xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(-0.065f, 0.07f, 0.0f);  // same local offset as iris
        glScalef(0.40f, 1.0f, 1.0f);           // narrower than iris
        drawSemiDiscLR(/*rx*/0.12f, /*ry*/0.08f, /*yOfs*/0.00f, 0.0025f, 28,
            /*black*/0.0f, 0.0f, 0.0f,
            /*tiltAmt*/0.035f, /*backAmt*/0.02f, /*raiseSide*/-1);
        glPopMatrix();
        glPopMatrix();

        // ---- RIGHT pupil ----
        glPushMatrix();
        glTranslatef(+xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(+0.065f, 0.07f, 0.0f);
        glScalef(0.40f, 1.0f, 1.0f);
        drawSemiDiscLR(0.12f, 0.08f, 0.00f, 0.0025f, 28,
            0.0f, 0.0f, 0.0f,
            /*tiltAmt*/0.035f, /*backAmt*/0.02f, /*raiseSide*/+1);
        glPopMatrix();
        glPopMatrix();

        //-------------------------white shine highlight---------------------
        // ---- LEFT highlight (rectangle) ----
        glPushMatrix();
        glTranslatef(-xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(-0.005f, 0.025f, 0.0f); // align with iris/pupil
        drawHighlightRect(/*w*/0.020f, /*h*/0.015f,
            /*xOfs*/-0.02f, /*yOfs*/0.02f,
            /*zLift*/0.0032f);
        glPopMatrix();
        glPopMatrix();

        // ---- RIGHT highlight (rectangle) ----
        glPushMatrix();
        glTranslatef(+xGap, yEyes, zFace);
        glScalef(sx, sy, 1.0f);
        glPushMatrix();
        glTranslatef(+0.005f, 0.020f, 0.0f);
        drawHighlightRect(0.025f, 0.015f,
            /*xOfs*/+0.02f, /*yOfs*/0.02f,
            0.0032f);
        glPopMatrix();
        glPopMatrix();

        DrawThirdEye(yEyes, zFace);

        // =====================
        // === Mouth Section ===
        // =====================
        {
            float yMouth = yEyes - 0.26f;  // move mouth below eyes
            glPushMatrix();
            glTranslatef(0.0f, yMouth, zFace - 0.01f);
            glScalef(sx, sy, 1.0f);

            drawMouthLine(
                /*halfW*/ 0.08f,    // width
                /*curv*/  gMouthCurv,   // + smile, - frown
                /*zLift*/ 0.05f,
                /*segs*/  40,
                /*color*/ 0.0f, 0.0f, 0.0f   // black mouth line
            );

            glPopMatrix();

            if (wasLight) glEnable(GL_LIGHTING);
        }

        // =====================
        // === HELMET Section ===
        // =====================

        glPushMatrix();
        glTranslatef(0.0f, 0.40f, 0.02f);   // move above forehead
        glScalef(1.0f, 0.8f, 1.0f);         // stretch to fit head
        drawHelmetHexTaper(/*bottomR*/0.28f, /*topR*/0.15f, /*height*/0.12f);
        glPopMatrix();

        // ---------- Helmet panels (parent transform on helmet) ----------
        glPushMatrix();
        glTranslatef(0.0f, 0.40f, 0.15f);   // helmet anchor on your head (move this)
        glScalef(1.0f, 1.0f, 1.0f);         // global scale for all three

        // how far each half sits from the center (tweak this)
        float xHalf = 0.0f;

        /************ LEFT HALF (your current panels) ************/
        glPushMatrix();
        glTranslatef(-xHalf, 0.0f, 0.0f);   // push to the left side

        // TOP panel (you tweak offset/scale/rot)
        glPushMatrix();
        glTranslatef(0.00f, 0.28f, +0.06f);   // move in front/height on helmet
        glRotatef(0.0f, 0, 1, 0);               // yaw if needed
        glScalef(0.9f, 0.9f, 1.0f);             // size
        drawPoly3D(HELM_TOP5, 5, 0.75f, 0.75f, 0.78f);
        glPopMatrix();

        // MIDDLE panel
        glPushMatrix();
        glTranslatef(0.00f, 0.26f, +0.07f);
        glRotatef(-10.0f, 0, 1, 0);
        glScalef(1.0f, 1.0f, 1.0f);
        drawPoly3D(HELM_MID4, 4, 0.72f, 0.72f, 0.76f);
        glPopMatrix();

        // BOTTOM panel
        glPushMatrix();
        glTranslatef(0.00f, 0.32f, +0.08f);
        glRotatef(-20.0f, 0, 1, 0);
        glScalef(1.1f, 1.1f, 1.0f);
        drawPoly3D(HELM_BOT5, 5, 0.70f, 0.70f, 0.74f);
        glPopMatrix();

        // BACK panel
        glPushMatrix();
        glTranslatef(0.00f, 0.26f, -0.33f);
        glRotatef(0, 0, 1, 0);
        glScalef(1.0f, 1.0f, 1.0f);
        drawPoly3D(HELM_BACK, 4, 0.72f, 0.72f, 0.76f);
        glPopMatrix();

        // === overlay diamond on it ===
        glPushMatrix();
        glTranslatef(0.0f, 0.1f, 0.085f);        // lift a bit to avoid z-fighting
        glScalef(0.5f, 0.5f, 1.0f);             // diamond size relative to panel
        drawDiamond(0.03f, 0.07f, 0.9f, 0.1f, 0.1f); // red diamond
        glPopMatrix();

        glPopMatrix();

        /************ RIGHT HALF  ************/
        glPushMatrix();
        // If you have culling on, disable it for mirrored draw:
        // glDisable(GL_CULL_FACE);

        glScalef(-1.0f, 1.0f, 1.0f);        // mirror across YZ plane
        glTranslatef(-xHalf, 0.0f, 0.0f);   // same offset as left (post-mirror = right)

        // TOP panel (same transforms as left)
        glPushMatrix();
        glTranslatef(0.00f, 0.28f, +0.06f);
        glRotatef(0.0f, 0, 1, 0);
        glScalef(0.9f, 0.9f, 1.0f);
        drawPoly3D(HELM_TOP5, 5, 0.75f, 0.75f, 0.78f);

        //----------------- crystal crown---------------
        float zLift = 0.0f;                  // avoid z-fighting
        float yBase = 0.0f;                   // common baseline so all bottoms align
        float cR = 0.82f, cG = 0.82f, cB = 0.88f;

        // center (straight)
        glPushMatrix();
        glTranslatef(0.00f, yBase + 0.01f, zLift - 0.02f);
        drawCrystalShardEx(0.055f, 0.19f, 0.06f, 0.85f, 0.00f, zLift, cR, cG, cB);
        glPopMatrix();

        // inner-left (tilt toward center)
        glPushMatrix();
        glTranslatef(-0.025f, yBase + 0.02f, zLift - 0.03f);
        glRotatef(14.0f, 0, 0, 1);
        drawCrystalShardEx(0.05f, 0.14f, 0.05f, 0.85f, -0.010f, zLift, cR, cG, cB);
        glPopMatrix();

        // inner-right
        glPushMatrix();
        glTranslatef(+0.025f, yBase + 0.02f, zLift - 0.03f);
        glRotatef(-14.0f, 0, 0, 1);
        drawCrystalShardEx(0.05f, 0.14f, 0.05f, 0.85f, +0.010f, zLift, cR, cG, cB);
        glPopMatrix();

        // outer-left (more tilt, same baseline feel)
        glPushMatrix();
        glTranslatef(-0.05f, yBase + 0.015f, zLift - 0.04f);
        glRotatef(24.0f, 0, 0, 1);
        drawCrystalShardEx(0.04f, 0.12f, 0.04f, 0.85f, -0.015f, zLift, cR, cG, cB);
        glPopMatrix();

        // outer-right
        glPushMatrix();
        glTranslatef(+0.05f, yBase + 0.015f, zLift - 0.04f);
        glRotatef(-24.0f, 0, 0, 1);
        drawCrystalShardEx(0.04f, 0.12f, 0.04f, 0.85f, +0.015f, zLift, cR, cG, cB);
        glPopMatrix();

        glPopMatrix();

        // MIDDLE panel
        glPushMatrix();
        glTranslatef(0.00f, 0.26f, +0.07f);
        glRotatef(-10.0f, 0, 1, 0);
        glScalef(1.0f, 1.0f, 1.0f);
        drawPoly3D(HELM_MID4, 4, 0.72f, 0.72f, 0.76f);
        glPopMatrix();

        // BOTTOM panel
        glPushMatrix();
        glTranslatef(0.00f, 0.32f, +0.08f);
        glRotatef(-20.0f, 0, 1, 0);
        glScalef(1.1f, 1.1f, 1.0f);
        drawPoly3D(HELM_BOT5, 5, 0.70f, 0.70f, 0.74f);
        glPopMatrix();

        glPopMatrix(); // end RIGHT half

        glPopMatrix(); // end helmet anchor

        // RIGHT cheek (visible in right side view)
        glPushMatrix();
        glTranslatef(+0.25f, 0.22f, -0.15f);  // X pushes it out from the head; tweak Y/Z as needed
        drawSidePlateYZ(SIDE7_YZ, 7, /*xPos=*/0.0f, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        // LEFT cheek (mirror) — just use negative X
        glPushMatrix();
        glTranslatef(-0.25f, 0.22f, -0.15f);
        drawSidePlateYZ(SIDE7_YZ, 7, 0.0f, 0.85f, 0.85f, 0.90f);
        glPopMatrix();
        ;

        //--------------------YUMAO-------------------
        //----- left ------
        glPushMatrix();
        glTranslatef(0.32f, 0.43f, -0.06f);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.30f, 0.35f, -0.01f);
        glRotatef(8, 0, 0, 1);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(0.29f, 0.28f, -0.09f);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        // ---- right -----
        glPushMatrix();
        glScalef(-1.0f, 1.0f, 1.0f);
        glTranslatef(0.32f, 0.43f, -0.06f);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        glPushMatrix();
        glScalef(-1.0f, 1.0f, 1.0f);
        glTranslatef(0.30f, 0.35f, -0.01f);
        glRotatef(8, 0, 0, 1);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();

        glPushMatrix();
        glScalef(-1.0f, 1.0f, 1.0f);
        glTranslatef(0.29f, 0.28f, -0.09f);
        drawPenta(PENTA5, 0.85f, 0.85f, 0.90f);
        glPopMatrix();
    }
    drawBand(2, C_nose);     // nose section (2→3)
    drawBand(3, C_chin);     // chin wedge (3→4)
    drawBand(4, C_under);    // under jaw close (4→5)

    // --- Helmet ribbons (live in the head's local space) ---
    {
        glPushMatrix();
        DrawRibbonRings();   // draw the two metal rings
        glPopMatrix();
    }
    {
        glPushMatrix();
        drawHelmetRibbons();
        glPopMatrix();
    }

    glPopMatrix();
}

// --------------------
// === Arms ===
// --------------------
void drawLowerArmArmor()
{
    GLUquadric* quad = gluNewQuadric();

    float armorLength = 2.0f; // match lower arm length

    // --- Main metallic bracer ---
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.5f);
    glColor3f(0.7f, 0.7f, 0.75f); // Metallic silver
    gluCylinder(quad, 0.40f, 0.20f, armorLength, 20, 20);
    glPopMatrix();

    // --- Rim at the wrist ---
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, armorLength + 0.5f); // move to wrist
    glColor3f(0.9f, 0.9f, 1.0f);           // Lighter silver/white rim
    drawRing(0.20f, 0.32f, 0.06f);         // Slightly thicker rim
    glPopMatrix();

    // --- Extended decorative rim at the elbow ---
    glPushMatrix();
    // Move slightly backward along -Z to extend upward past elbow
    glTranslatef(0.0f, 0.0f, -0.1f);
    glColor3f(0.85f, 0.65f, 0.2f);         // Gold/brass tone for visibility
    drawRing(0.24f, 0.30f, 0.7f);         // Thicker & taller ring
    glPopMatrix();

    gluDeleteQuadric(quad);
}

void drawUpperArm()
{
    GLUquadric* quad = gluNewQuadric();
    glColor3f(1.0f, 0.8f, 0.6f); // Skin color

    // gluCylinder(quad, radius, radius, length, 20, 20);
    gluCylinder(quad, 0.3f, 0.15f, 2.7f, 20, 20);

    gluDeleteQuadric(quad);
}

void drawLowerArm()
{
    GLUquadric* quad = gluNewQuadric();

    float lowerArmLength = 2.8f;

    // --- Arm (skin) ---
    glColor3f(1.0f, 0.7f, 0.6f);
    gluCylinder(quad, 0.25f, 0.18f, lowerArmLength, 20, 20);

    // --- Armor overlay ---
    glPushMatrix();
    drawLowerArmArmor();
    glPopMatrix();

    gluDeleteQuadric(quad);
}

void drawFinger(float baseLen = 0.3f, float tipLen = 0.2f, float radius = 0.05f, float bendAngle = -15.0f)
{
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH); // Enable normals for lighting

    glColor3f(1.0f, 0.82f, 0.65f); // Slightly lighter for finger

    glPushMatrix();
    // --- Base segment ---
    gluCylinder(quad, radius, radius * 0.9f, baseLen, 16, 16);

    // --- Joint (sphere at end of base) ---
    glTranslatef(0.0f, 0.0f, baseLen);
    gluSphere(quad, radius * 1.05f, 16, 16);

    // --- Tip segment (bend slightly upward) ---
    glRotatef(bendAngle, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, radius * 0.9f, radius * 0.7f, tipLen, 16, 16);

    glPopMatrix();
    gluDeleteQuadric(quad);
}

void drawHand()
{
    glPushMatrix();

    // === Fist blend during 'B' animation ===
    float uGrip = 0.0f; // 0=open, 1=fist
    if (weaponState == WPN_RAISE_SPIN) {
        uGrip = SmoothStep01(weaponTimer / kRaiseDur);
    }
    else if (weaponState == WPN_MOVE_TO_BACK || weaponState == WPN_HOLD_BACK || attackState != ATK_IDLE) {
        uGrip = 1.0f;
    }

    // Pose state
    const bool rightHand = (gWhichHand == +1);
    const HandPose pose = rightHand ? gRightHandPose : gLeftHandPose;
    const float    pMix = rightHand ? gRightHandPoseBlend : gLeftHandPoseBlend; // 0..1

    // --- Palm ---
    glColor3f(0.95f, 0.75f, 0.55f);
    const float palmWidth = 0.45f, palmHeight = 0.25f, palmDepth = 0.7f;
    glPushMatrix(); glScalef(palmWidth, palmHeight, palmDepth); drawCube(1.0f); glPopMatrix();

    // === Finger params (index..pinky) ===
    const float fingerBaseLen[4] = { 0.32f, 0.35f, 0.33f, 0.28f };
    const float fingerTipLen[4] = { 0.22f, 0.25f, 0.23f, 0.18f };

    const float baseOpen[4] = { 0.0f,  0.0f,  0.0f,  0.0f };
    const float baseFist[4] = { -55.f, -60.f, -65.f, -70.f };

    const float tipOpen[4] = { -20.f, -15.f, -25.f, -30.f };
    const float tipFist[4] = { -70.f, -75.f, -80.f, -85.f };

    const float xOpen[4] = { -0.18f, -0.05f,  0.08f,  0.21f };
    const float xGrip[4] = { -0.15f, -0.03f,  0.06f,  0.18f };

    // Pose targets
    float basePose[4] = { 0,0,0,0 }, tipPose[4] = { 0,0,0,0 };
    if (pose == HANDPOSE_PEACE) {
        float b[4] = { 0.f, 0.f, -65.f, -70.f };
        float t[4] = { 0.f, 0.f, -82.f, -88.f };
        memcpy(basePose, b, sizeof(b)); memcpy(tipPose, t, sizeof(t));
    }
    else if (pose == HANDPOSE_THUMBSUP) {
        float b[4] = { -60.f, -65.f, -70.f, -75.f };
        float t[4] = { -85.f, -90.f, -95.f, -100.f };
        memcpy(basePose, b, sizeof(b)); memcpy(tipPose, t, sizeof(t));
    }

    // === Fingers (index..pinky) ===
    for (int i = 0; i < 4; ++i)
    {
        float baseDef = Lerp(baseOpen[i], baseFist[i], uGrip);
        float tipDef = Lerp(tipOpen[i], tipFist[i], uGrip);
        float xDef = Lerp(xOpen[i], xGrip[i], uGrip);

        // Blend toward pose targets (not the thumb!)
        if (pMix > 0.001f && pose != HANDPOSE_NONE) {
            baseDef = Lerp(baseDef, basePose[i], pMix);
            tipDef = Lerp(tipDef, tipPose[i], pMix);
            // widen V for ✌️
            if (pose == HANDPOSE_PEACE && (i == 0 || i == 1))
                xDef += ((i == 0 ? -0.015f : +0.015f) * pMix);
        }

        glPushMatrix();
        const float zEmbed = Lerp(0.28f, 0.22f, uGrip);
        glTranslatef(xDef, 0.03f, zEmbed);

        const float slant = Lerp(-45.f, -55.f, uGrip);
        glRotatef(baseDef, 1, 0, 0);
        glRotatef(slant, 1, 0, 0);

        drawFinger(fingerBaseLen[i], fingerTipLen[i], 0.05f, tipDef);
        glPopMatrix();
    }

    // === Thumb ===
    glPushMatrix();
    float tx = -0.25f + 0.03f * uGrip;
    float ty = -0.02f;
    float tz = 0.05f - 0.03f * uGrip;

    // Nudge the *attach point* slightly outward for THUMBSUP (helps silhouette)
    if (pMix > 0.001f && pose == HANDPOSE_THUMBSUP) {
        tx += rightHand ? -0.015f : +0.015f;   // move base a bit to thumb side
    }
    glTranslatef(tx, ty, tz);

    // Extra lift & outward offset (position) for THUMBSUP
    if (pMix > 0.001f && pose == HANDPOSE_THUMBSUP) {
        float xOut = (rightHand ? -0.050f : +0.050f) * pMix;  // was 0.02 → 0.05
        float yUp = 0.080f * pMix;                            // was 0.04 → 0.08
        glTranslatef(xOut, yUp, 0.0f);
    }

    // Base thumb pose (open→fist)
    float thumbOut = Lerp(25.f, 5.f, uGrip);   // Z-roll/splay
    float thumbFwd = Lerp(-65.f, -82.f, uGrip);   // X-pitch (flex)
    float thumbTip = Lerp(8.f, -30.f, uGrip);   // distal
    float thumbYaw = 0.0f;                        // Y-yaw

    // Pose targets
    if (pMix > 0.001f && pose != HANDPOSE_NONE) {
        float outTgt, fwdTgt, tipTgt, yawTgt = 0.0f;
        if (pose == HANDPOSE_PEACE) {
            outTgt = 10.f;  fwdTgt = -75.f; tipTgt = -20.f;
        }
        else if (pose == HANDPOSE_THUMBSUP) {
            outTgt = 330.f;                      
            fwdTgt = +22.f;                      
            tipTgt = +12.f;                      
            yawTgt = rightHand ? -30.f : -80.f;  
        }
        else {
            outTgt = 25.f;  fwdTgt = -82.f; tipTgt = -30.f;
        }
        thumbOut = Lerp(thumbOut, outTgt, pMix);
        thumbFwd = Lerp(thumbFwd, fwdTgt, pMix);
        thumbTip = Lerp(thumbTip, tipTgt, pMix);
        thumbYaw = Lerp(thumbYaw, yawTgt, pMix);
    }

    // Apply rotations
    // (Order chosen to read well: yaw → roll → pitch)
    glRotatef(thumbYaw, 0, 1, 0);
    glRotatef(thumbOut, 0, 0, 1);
    glRotatef(thumbFwd, 1, 0, 0);

    drawFinger(0.20f, 0.18f, 0.06f, thumbTip);
    glPopMatrix();

    glPopMatrix();
}


void drawLeftArm()
{
    glPushMatrix();
    glTranslatef(-1.0f, 3.0f, 2.0f);
    glScalef(0.6f, 0.6f, 0.6f);
    drawJoint(0.3f);

    // ===== Idle/Walk base =====
    const float ampMul = running ? kRunAmpMul : 1.0f;
    const float kArmSwingAmp = 22.0f * ampMul;
    const float kElbowBase = -20.0f;
    const float kElbowAmp = -14.0f * ampMul;
    const float kWristAmp = -0.5f;

    const float phase = walkPhase + 3.1415926f; // opposite the left leg
    const float shoulderFX = kArmSwingAmp * sinf(phase);
    const float elbowFlexW = kElbowBase + kElbowAmp * fmaxf(0.0f, -sinf(phase));
    const float wristSwingW = kWristAmp * shoulderFX;

    float shoulderPitch_idle = 75.0f + shoulderFX;
    float shoulderYaw_idle = -6.0f;
    float shoulderRoll_idle = 0.0f;
    float elbowFlex_idle = elbowFlexW;
    float wristRoll_idle = -80.0f;
    float wristPitch_idle = wristSwingW;
    float handTx_idle = -0.10f;
    float handSx_idle = 1.40f;

    // ===== Combat targets (your guard pose) =====
    float shoulderPitch_combat = -120.0f;
    float shoulderYaw_combat = -25.0f;
    float shoulderRoll_combat = 45.0f;
    float elbowFlex_combat = 65.0f;
    float wristRoll_combat = -90.0f;
    float wristPitch_combat = 0.0f;
    float handTx_combat = -0.10f;
    float handSx_combat = 1.40f;

    // ===== Blend =====
    float t = combatBlend;
    float sPitch = Lerp(shoulderPitch_idle, shoulderPitch_combat, t);
    float sYaw = Lerp(shoulderYaw_idle, shoulderYaw_combat, t);
    float sRoll = Lerp(shoulderRoll_idle, shoulderRoll_combat, t);
    float eFlex = Lerp(elbowFlex_idle, elbowFlex_combat, t);
    float wRoll = Lerp(wristRoll_idle, wristRoll_combat, t);
    float wPitch = Lerp(wristPitch_idle, wristPitch_combat, t);
    float handTx = Lerp(handTx_idle, handTx_combat, t);
    float handSx = Lerp(handSx_idle, handSx_combat, t);

    // ===== Override for Attack 'N' (takes precedence over B) =====
    if (attackState != ATK_IDLE) {
        // bring the hand forward (combat-ish guard → striking)
        float uW =
            (attackState == ATK_WINDUP) ? SmoothStep01(attackTimer / kAtkWindup) :
            (attackState == ATK_SWEEP) ? 1.0f :
            /* ATK_RECOVER */               1.0f - SmoothStep01(attackTimer / kAtkRecover);

        // after computing uW, add this:
        float sweepA = (attackState == ATK_SWEEP) ? SmoothStep01(attackTimer / kAtkSweep)
            : (attackState == ATK_RECOVER) ? 1.0f - SmoothStep01(attackTimer / kAtkRecover)
            : 0.0f;
        sPitch += 18.0f * sweepA;  // try 12–25 for extra upward arc

        // Start from your raise/hold guard numbers and blend in
        const float sPitch_hold = 10.0f, 
                    sYaw_hold = 30.0f, 
                    sRoll_hold = 28.0f;
        const float eFlex_hold = 32.0f, 
                    wRoll_hold = -200.0f, 
                    wPitch_hold = 0.0f;

        sPitch = Lerp(sPitch, sPitch_hold, uW);
        sYaw = Lerp(sYaw, sYaw_hold, uW);
        sRoll = Lerp(sRoll, sRoll_hold, uW);
        eFlex = Lerp(eFlex, eFlex_hold, uW);
        wRoll = Lerp(wRoll, wRoll_hold, uW);
        wPitch = Lerp(wPitch, wPitch_hold, uW);
        handTx = -0.10f;
        handSx = 1.40f;
    }

    // ===== Override for Weapon-B sequence =====
    if (weaponState != WPN_IDLE && attackState == ATK_IDLE) {
        // progress (eased)
        float uRaise = 0.f, uMove = 0.f, uBack = 0.f;
        if (weaponState == WPN_RAISE_SPIN) {
            uRaise = SmoothStep01(weaponTimer / kRaiseDur);
        }
        else if (weaponState == WPN_MOVE_TO_BACK) {
            uRaise = 1.0f; // already raised
            uMove = SmoothStep01(weaponTimer / kMoveDur);
        }
        else { // WPN_HOLD_BACK
            uRaise = 1.0f;
            uBack = 1.0f;
        }

        // A) raised & spinning in front
        const float sPitch_hold = 10.0f;
        const float sYaw_hold = -18.0f;
        const float sRoll_hold = 28.0f;
        const float eFlex_hold = 22.0f;
        const float wRoll_hold = -200.0f;
        const float wPitch_hold = 0.0f;

        // B) hand to back while holding
        const float sPitch_back = -62.0f;
        const float sYaw_back = -172.0f;
        const float sRoll_back = 18.0f;
        const float eFlex_back = 34.0f;
        const float wRoll_back = -220.0f;
        const float wPitch_back = 0.0f;

        // current → raised
        sPitch = Lerp(sPitch, sPitch_hold, uRaise);
        sYaw = Lerp(sYaw, sYaw_hold, uRaise);
        sRoll = Lerp(sRoll, sRoll_hold, uRaise);
        eFlex = Lerp(eFlex, eFlex_hold, uRaise);
        wRoll = Lerp(wRoll, wRoll_hold, uRaise);
        wPitch = Lerp(wPitch, wPitch_hold, uRaise);

        // raised → back (during move and then hold)
        float uToBack = fmaxf(uMove, uBack);
        sPitch = Lerp(sPitch, sPitch_back, uToBack);
        sYaw = Lerp(sYaw, sYaw_back, uToBack);
        sRoll = Lerp(sRoll, sRoll_back, uToBack);
        eFlex = Lerp(eFlex, eFlex_back, uToBack);
        wRoll = Lerp(wRoll, wRoll_back, uToBack);
        wPitch = Lerp(wPitch, wPitch_back, uToBack);

        handTx = -0.10f;
        handSx = 1.40f;
    }

    // ===== Extra life (no new globals) =====
    if (t > 0.1f && attackState == ATK_IDLE) {
        float ph = combatBobPh;
        float ph2 = 1.9f * combatBobPh;
        eFlex += (5.0f * sinf(ph - 0.3f)) * t; // tiny elbow “breathing”
        sYaw += (5.0f * sinf(ph - 0.3f)) * t;
        wPitch += (5.0f * sinf(0.95f * ph)) * t;
        wRoll += (4.0f * sinf(ph2 + 0.6f)) * t;
    }

    // === Manual lift offsets (final layer, always on top) ===
    if (gUpperLiftLevelL > 0) sPitch -= kUpperLiftDeg[gUpperLiftLevelL];
    if (gLowerLiftLevelL > 0) eFlex -= kLowerLiftDeg[gLowerLiftLevelL];

    // Shoulder
    glRotatef(sPitch, 1, 0, 0);
    glRotatef(sYaw, 0, 1, 0);
    glRotatef(sRoll, 0, 0, 1);
    drawUpperArm();

    // Elbow / forearm
    if (attackState != ATK_IDLE) {
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 2.7f);
        drawJoint();
        glScalef(0.6f, 0.6f, 0.6f);
        glRotatef(eFlex - 40, 1, 0, 0);
        glRotatef(40.0f, 0, 1, 0);
        drawLowerArm();
    }
    else {
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 2.7f);
        drawJoint();
        glScalef(0.6f, 0.6f, 0.6f);
        glRotatef(eFlex, 1, 0, 0);
        drawLowerArm();
    }
    
    // Wrist / hand (+ a little reach variation)
    glPushMatrix();
    float reach = (t > 0.1f) ? (0.11f * sinf(2.2f * combatBobPh + 0.7f)) * t : 0.0f;
    glTranslatef(0.0f, 0.0f, 2.8f + reach);
    drawJoint();
    glTranslatef(handTx, 0.0f, 0.02f);
    glScalef(handSx, 1.4f, 1.4f);
    glRotatef(wRoll, 0, 0, 1);
    glRotatef(wPitch, 1, 0, 0);
    if (gLeftHandPose != HANDPOSE_NONE) {
        float b = gLeftHandPoseBlend;
        if (gLeftHandPose == HANDPOSE_PEACE) {
            wRoll = Lerp(wRoll, -90.0f, b);
            wPitch = Lerp(wPitch, 10.0f, b);
        }
        else if (gLeftHandPose == HANDPOSE_THUMBSUP) {
            wRoll = Lerp(wRoll, -90.0f, b);  // palm sideways
            wPitch = Lerp(wPitch, 0.0f, b);  // neutral pitch
        }
    }

    gWhichHand = -1;     // LEFT
    drawHand();
    gWhichHand = 0;

    // ------------------------------------------------------------
    // ==== WEAPON ATTACH (B - spin/move/back, N - sweep like B) ====
    if (weaponState != WPN_IDLE || attackState != ATK_IDLE) {
        glPushMatrix();

        // --- Hand-frame placement (same as your B numbers) ---
        const float WPN_HAND_OFFS_X = 0.00f;
        const float WPN_HAND_OFFS_Y = -0.06f;
        const float WPN_HAND_OFFS_Z = 0.10f;

        const float WPN_HAND_EULER_X = 26.5f;   // pitch
        const float WPN_HAND_EULER_Y = -53.0f;  // yaw
        const float WPN_HAND_EULER_Z = -85.0f;  // roll

        const float WPN_ORIGIN_TO_GRIP_Y = 9.0f; // your model’s grip distance

        // Hand → weapon alignment
        glTranslatef(WPN_HAND_OFFS_X, WPN_HAND_OFFS_Y, WPN_HAND_OFFS_Z);
        glRotatef(WPN_HAND_EULER_X, 1, 0, 0);
        glRotatef(WPN_HAND_EULER_Y, 0, 1, 0);
        glRotatef(WPN_HAND_EULER_Z, 0, 0, 1);

        // Your extra tilts (to match B’s front orientation)
        glRotatef(60.0f, 1, 0, 0); 
        glRotatef(20.0f, 0, 0, 1); 
        glRotatef(-60.0f, 0, 1, 0); 

        // === Animate exactly like B’s front path when N is active ===
        if (attackState != ATK_IDLE) { 
            glTranslatef(0.0f, -WPN_ORIGIN_TO_GRIP_Y, 0.0f);
        }
        else if (attackState == ATK_WINDUP) {
            glRotatef(90.0f, 1, 0, 0);
        }
        else if (weaponState == WPN_RAISE_SPIN) {
            glRotatef(weaponSpin, 0, 0, 1);
            glTranslatef(0.0f, -WPN_ORIGIN_TO_GRIP_Y, 0.0f);
        }
        else if (weaponState == WPN_MOVE_TO_BACK) {
            glTranslatef(0.0f, -WPN_ORIGIN_TO_GRIP_Y, 0.0f);
        }
        else { // WPN_HOLD_BACK
            glTranslatef(0.0f, -WPN_ORIGIN_TO_GRIP_Y, 0.0f);
        }
        
        // Draw once
        glPushMatrix();
        glScalef(1.8f, 1.8f, 1.8f);
        drawWeapon();
        glPopMatrix();

        // === Thunder FX while spinning in front (B) or during N attack ===
        if (gFXEnabled && (weaponState == WPN_RAISE_SPIN || weaponState == WPN_MOVE_TO_BACK 
            || weaponState == WPN_HOLD_BACK || attackState != ATK_IDLE)) {
            // Stronger during sweep; good pop while spinning
            float fxIntensity = 1.1f;
            if (attackState == ATK_WINDUP)  fxIntensity = 1.25f;
            if (attackState == ATK_SWEEP)   fxIntensity = 1.55f;
            if (attackState == ATK_RECOVER) fxIntensity = 1.2f;

            // Center at blade head relative to the hand grip
            FX_DrawWeaponThunderAt(WPN_BLADE_HEAD_FROM_GRIP_Y, fxIntensity);
        }

        glPopMatrix();
    }
    // ------------------------------------------------------------

    glPopMatrix();    // wrist

    glPopMatrix();    // lower arm
    glPopMatrix();    // shoulder
}

void drawRightArm()
{
    glPushMatrix();
    glTranslatef(1.0f, 3.0f, 2.0f);
    glScalef(0.6f, 0.6f, 0.6f);
    drawJoint(0.3f);

    // ===== Idle/Wave/Walk base (preserve your behavior) =====
    float shoulderPitch_idle, shoulderYaw_idle, shoulderRoll_idle;
    float elbowFlex_idle, elbowYaw_idle, wristRoll_idle, wristPitch_idle;
    float handTx_idle, handScaleX_idle;

    if (waving) {
        float forearmSwing = 22.0f * sinf(wavePhase);
        shoulderPitch_idle = 45.0f;
        shoulderYaw_idle = 25.0f;
        shoulderRoll_idle = 0.0f;
        elbowFlex_idle = -95.0f;
        elbowYaw_idle = forearmSwing;                   // side swing
        wristRoll_idle = -140.0f - 0.6f * forearmSwing;
        wristPitch_idle = 8.0f;
        handTx_idle = 0.08f;
        handScaleX_idle = -1.35f;
    }
    else {
        const float ampMul = running ? kRunAmpMul : 1.0f;
        const float kArmSwingAmp = 22.0f * ampMul;
        const float kElbowBase = -20.0f;
        const float kElbowAmp = -14.0f * ampMul;
        const float kWristAmp = -0.5f;

        float phase = walkPhase;
        float shoulderFX = kArmSwingAmp * sinf(phase);
        float elbowFlexW = kElbowBase + kElbowAmp * fmaxf(0.0f, -sinf(phase));
        float wristSwingW = kWristAmp * shoulderFX;

        shoulderPitch_idle = 75.0f + shoulderFX;
        shoulderYaw_idle = 6.0f;
        shoulderRoll_idle = 0.0f;
        elbowFlex_idle = elbowFlexW;
        elbowYaw_idle = 0.0f;
        wristRoll_idle = -80.0f;
        wristPitch_idle = wristSwingW;
        handTx_idle = 0.10f;
        handScaleX_idle = -1.40f;
    }

    // ===== Combat targets =====
    float shoulderPitch_combat = -15.0f;
    float shoulderYaw_combat = 10.0f;
    float shoulderRoll_combat = 15.0f;
    float elbowFlex_combat = -35.0f;
    float elbowYaw_combat = 0.0f;
    float wristRoll_combat = -90.0f;
    float wristPitch_combat = 5.0f;
    float handTx_combat = 0.08f;
    float handScaleX_combat = -1.35f;

    // ===== Blend =====
    float t = combatBlend;
    float sPitch = Lerp(shoulderPitch_idle, shoulderPitch_combat, t);
    float sYaw = Lerp(shoulderYaw_idle, shoulderYaw_combat, t);
    float sRoll = Lerp(shoulderRoll_idle, shoulderRoll_combat, t);
    float eFlex = Lerp(elbowFlex_idle, elbowFlex_combat, t);
    float eYaw = Lerp(elbowYaw_idle, elbowYaw_combat, t);
    float wRoll = Lerp(wristRoll_idle, wristRoll_combat, t);
    float wPitch = Lerp(wristPitch_idle, wristPitch_combat, t);
    float handTx = Lerp(handTx_idle, handTx_combat, t);
    float handSx = Lerp(handScaleX_idle, handScaleX_combat, t);

    // ===== Extra life (no new globals): tiny sway & reach that fade in with t =====
    if (t > 0.1f) {
        // use existing combatBobPh/kCombatBobAmp as the driver
        const float PI = 3.1415926f;
        float ph = combatBobPh + PI;
        float ph2 = 1.7f * combatBobPh + PI;        // slightly different rate

        // elbow “lag” feel (procedural, no stored state)
        eFlex += (6.0f * sinf(ph - 0.5f)) * t;

        sYaw += (5.0f * sinf(ph - 0.3f)) * t;

        // wrist expressive motion
        wPitch += (6.0f * sinf(ph)) * t;     // pitch sway
        wRoll += (5.0f * sinf(ph2)) * t;     // roll sway
        eYaw += (4.0f * sinf(0.8f * ph2)) * t; // tiny forearm yaw

        // forward/back reach along the chain’s +Z
        float reach = (0.18f * sinf(1.9f * ph)) * t;
    }

    // Subtle thrust in forearm base (kept from your version)
    float thrust = (combatBlend > 0.01f) ? (kCombatBobAmp * 0.6f * sinf(combatBobPh)) : 0.0f;

    // === Manual lift offsets (final layer, always on top) ===
    if (gUpperLiftLevelR > 0) sPitch -= kUpperLiftDeg[gUpperLiftLevelR] + 20;
    if (gLowerLiftLevelR > 0) eFlex -= kLowerLiftDeg[gLowerLiftLevelR];

    // Shoulder
    glRotatef(sPitch, 1, 0, 0);
    glRotatef(sYaw, 0, 1, 0);
    glRotatef(sRoll, 0, 0, 1);
    drawUpperArm();

    // Elbow / forearm
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.7f + thrust);
    drawJoint();
    glScalef(0.6f, 0.6f, 0.6f);
    glRotatef(eFlex, 1, 0, 0);
    if (gLowerLiftLevelR > 1) {
        glRotatef(eFlex * 0.6, 0, 1, 0);
    }
    glRotatef(eYaw, 0, 1, 0);
    drawLowerArm();

    // Wrist / hand
    glPushMatrix();
    float ph = combatBobPh; // local re-use only
    float reach = (t > 0.1f) ? (0.18f * sinf(1.9f * ph)) * t : 0.0f;

    glTranslatef(0.0f, 0.0f, 2.8f + reach);
    drawJoint();
    glTranslatef(handTx, 0.0f, 0.02f);
    glScalef(handSx, 1.35f, 1.35f);
    glRotatef(wRoll, 0, 0, 1);
    glRotatef(wPitch, 1, 0, 0);

    if (gLowerLiftLevelR > 1) {
        glRotatef(eFlex * -0.6, 1, 0, 0);
    }

    // Pose-friendly wrist orientation for photos
    if (gRightHandPose != HANDPOSE_NONE) {
        float b = gRightHandPoseBlend;
        if (gRightHandPose == HANDPOSE_PEACE) {
            wRoll = Lerp(wRoll, -90.0f, b);   // palm out
            wPitch = Lerp(wPitch, 10.0f, b);
        }
        else if (gRightHandPose == HANDPOSE_THUMBSUP) {
            wRoll = Lerp(wRoll, -90.0f, b);
            wPitch = Lerp(wPitch, 0.0f, b);
        }
    }

    gWhichHand = +1;     // RIGHT
    drawHand();
    gWhichHand = 0;
    glPopMatrix();   // wrist

    glPopMatrix();   // lower arm
    glPopMatrix();   // shoulder
}

// --------------------------------
// === Shoulder and Chest Armor ===
// --------------------------------
void drawShouderBlock(float depth = 0.65f) {
    // Geometry (unchanged)
    GLfloat pentagon[4][2] = {
        {-0.2f,  0.5f},
        {-0.5f,  0.0f},
        { 0.7f, -0.35f},
        { 0.9f, -0.05f}
    };

    // Original fallback colors
    float frontColor[3] = { 0.4f, 0.4f, 0.8f };
    float backColor[3] = { 0.2f, 0.2f, 0.5f };
    float sideColor[3] = { 0.3f, 0.3f, 0.6f };

    // ---- Texture setup (same pattern as your other parts) ----
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    HBITMAP h = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), CurrentTex(0),
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    BITMAP bmp = {};
    GLuint tex = 0;
    bool haveTex = false;

    if (h && GetObject(h, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits &&
        (bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32))
    {
        GLenum srcFmt = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
        GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Ensure no tinting from previous colors
        glColor3f(1.0f, 1.0f, 1.0f);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glTexImage2D(GL_TEXTURE_2D, 0, internal,
            bmp.bmWidth, bmp.bmHeight, 0,
            srcFmt, GL_UNSIGNED_BYTE, bmp.bmBits);

        haveTex = true;
    }

    if (haveTex) {
        // --- Compute planar UV bounds for front/back ---
        float minX = pentagon[0][0], maxX = pentagon[0][0];
        float minY = pentagon[0][1], maxY = pentagon[0][1];
        for (int i = 1; i < 4; ++i) {
            if (pentagon[i][0] < minX) minX = pentagon[i][0];
            if (pentagon[i][0] > maxX) maxX = pentagon[i][0];
            if (pentagon[i][1] < minY) minY = pentagon[i][1];
            if (pentagon[i][1] > maxY) maxY = pentagon[i][1];
        }
        float invW = (maxX > minX) ? 1.0f / (maxX - minX) : 1.0f;
        float invH = (maxY > minY) ? 1.0f / (maxY - minY) : 1.0f;

        // --- Front face (z = 0) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 4; ++i) {
            float s = (pentagon[i][0] - minX) * invW;
            float t = (pentagon[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(pentagon[i][0], pentagon[i][1], 0.0f);
        }
        glEnd();

        // --- Back face (z = -depth) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 4; ++i) {
            float s = (pentagon[i][0] - minX) * invW;
            float t = (pentagon[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(pentagon[i][0], pentagon[i][1], -depth);
        }
        glEnd();

        // --- Side walls (wrap U along perimeter, V across depth) ---
        auto edgeLen = [&](int a, int b) {
            float dx = pentagon[b][0] - pentagon[a][0];
            float dy = pentagon[b][1] - pentagon[a][1];
            return sqrtf(dx * dx + dy * dy);
            };
        float perim = 0.f;
        for (int i = 0; i < 4; ++i) perim += edgeLen(i, (i + 1) % 4);
        float invP = (perim > 0.f) ? 1.f / perim : 1.f;

        float uAcc = 0.f;
        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) % 4;
            float seg = edgeLen(i, j);
            float u0 = uAcc * invP, u1 = (uAcc + seg) * invP;
            uAcc += seg;

            glBegin(GL_QUADS);
            glTexCoord2f(u0, 0.0f); glVertex3f(pentagon[i][0], pentagon[i][1], 0.0f);
            glTexCoord2f(u1, 0.0f); glVertex3f(pentagon[j][0], pentagon[j][1], 0.0f);
            glTexCoord2f(u1, 1.0f); glVertex3f(pentagon[j][0], pentagon[j][1], -depth);
            glTexCoord2f(u0, 1.0f); glVertex3f(pentagon[i][0], pentagon[i][1], -depth);
            glEnd();
        }

        // Restore defaults / clean up texture state
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        // Fallback: original solid colors
        drawPolygonPlate(pentagon, 4, depth, frontColor, backColor, sideColor);
    }

    // Release GDI/OpenGL objects
    if (tex) glDeleteTextures(1, &tex);
    if (h)   DeleteObject(h);
}

void drawShoulderArmor(float depth = 0.2f)
{
    // Your original pentagon shape
    GLfloat pentagon[5][2] = {
        {-0.15f, 0.45f},
        {-0.4f,  0.05f},
        {-0.1f, -0.6f},
        { 0.3f, -0.3f},
        { 0.45f, 0.15f}
    };

    // === OUTLINE PASS (unchanged) ===
    glPushMatrix();
    glScalef(1.05f, 1.05f, 1.05f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    float borderColor[3] = { 0.0f, 0.0f, 0.0f };
    drawPolygonPlate(pentagon, 5, depth, borderColor, borderColor, borderColor);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // === FILL PASS (texture goldwhite.bmp) ===
    // Texture setup (same flow as your boot/chest)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    HBITMAP h = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), CurrentTex(2),
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    BITMAP bmp = {};
    GLuint tex = 0;
    bool haveTex = false;

    if (h && GetObject(h, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits &&
        (bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32))
    {
        GLenum srcFmt = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
        GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Avoid darkening by vertex colors
        glColor3f(1, 1, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glTexImage2D(GL_TEXTURE_2D, 0, internal,
            bmp.bmWidth, bmp.bmHeight, 0, srcFmt, GL_UNSIGNED_BYTE, bmp.bmBits);

        haveTex = true;
    }

    if (haveTex) {
        // --- compute bounds for planar UVs on front/back ---
        float minX = pentagon[0][0], maxX = pentagon[0][0];
        float minY = pentagon[0][1], maxY = pentagon[0][1];
        for (int i = 1; i < 5; ++i) {
            if (pentagon[i][0] < minX) minX = pentagon[i][0];
            if (pentagon[i][0] > maxX) maxX = pentagon[i][0];
            if (pentagon[i][1] < minY) minY = pentagon[i][1];
            if (pentagon[i][1] > maxY) maxY = pentagon[i][1];
        }
        float invW = (maxX > minX) ? 1.0f / (maxX - minX) : 1.0f;
        float invH = (maxY > minY) ? 1.0f / (maxY - minY) : 1.0f;

        // --- Front face (z = 0) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 5; ++i) {
            float s = (pentagon[i][0] - minX) * invW;
            float t = (pentagon[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(pentagon[i][0], pentagon[i][1], 0.0f);
        }
        glEnd();

        // --- Back face (z = -depth) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 5; ++i) {
            float s = (pentagon[i][0] - minX) * invW;
            float t = (pentagon[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(pentagon[i][0], pentagon[i][1], -depth);
        }
        glEnd();

        // --- Side walls (wrap U along the edge length) ---
        auto edgeLen = [&](int a, int b) {
            float dx = pentagon[b][0] - pentagon[a][0];
            float dy = pentagon[b][1] - pentagon[a][1];
            return sqrtf(dx * dx + dy * dy);
            };
        float perim = 0.f;
        for (int i = 0; i < 5; ++i) perim += edgeLen(i, (i + 1) % 5);
        float invP = (perim > 0.f) ? 1.f / perim : 1.f;

        float uAcc = 0.f;
        for (int i = 0; i < 5; ++i) {
            int j = (i + 1) % 5;
            float seg = edgeLen(i, j);
            float u0 = uAcc * invP, u1 = (uAcc + seg) * invP;
            uAcc += seg;

            glBegin(GL_QUADS);
            glTexCoord2f(u0, 0.0f); glVertex3f(pentagon[i][0], pentagon[i][1], 0.0f);
            glTexCoord2f(u1, 0.0f); glVertex3f(pentagon[j][0], pentagon[j][1], 0.0f);
            glTexCoord2f(u1, 1.0f); glVertex3f(pentagon[j][0], pentagon[j][1], -depth);
            glTexCoord2f(u0, 1.0f); glVertex3f(pentagon[i][0], pentagon[i][1], -depth);
            glEnd();
        }

        // restore defaults and disable texturing for whatever comes next
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        // Fallback: original solid colors
        float frontColor[3] = { 0.6f, 0.6f, 1.0f };
        float backColor[3] = { 0.4f, 0.4f, 0.8f };
        float sideColor[3] = { 0.3f, 0.3f, 0.6f };
        drawPolygonPlate(pentagon, 5, depth, frontColor, backColor, sideColor);
    }

    // Cleanup bitmap/texture objects
    if (tex) glDeleteTextures(1, &tex);
    if (h)   DeleteObject(h);
}

void drawChestPlate(float depth = 0.1f)
{
    // ===== 1) OUTLINE PASS (unchanged, untextured) =====
    float black[3] = { 0.0f, 0.0f, 0.0f };

    glPushMatrix();
    glScalef(1.05f, 1.05f, 1.05f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);

    GLfloat basePlate[6][2] = {
        {0.1f, 0.5f}, {-0.05f, 0.45f}, {-0.3f, -0.3f},
        {-0.2f, -0.5f}, {0.15f, -0.7f}, {0.4f, -0.2f}
    };
    GLfloat lowerPlate[6][2] = {
        {0.1f, 0.5f}, {-0.05f, 0.45f}, {-0.35f, -0.35f},
        {-0.25f, -0.6f}, {0.2f, -0.85f}, {0.45f, -0.25f}
    };

    drawPolygonPlate(basePlate, 6, depth, black, black, black);
    glPushMatrix(); glTranslatef(0.0f, 0.0f, -0.1f);
    drawPolygonPlate(lowerPlate, 6, depth, black, black, black);
    glPopMatrix();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // ===== 2) FILL PASS (textured with goldshining.bmp) =====

    // --- Texture setup ---
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    HBITMAP h = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), CurrentTex(1),
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    BITMAP bmp = {};
    GLuint tex = 0;
    bool haveTexture = false;

    if (h && GetObject(h, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits &&
        (bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32))
    {
        GLenum srcFormat = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
        GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Make texture unaffected by vertex color
        glColor3f(1, 1, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glTexImage2D(GL_TEXTURE_2D, 0, internal,
            bmp.bmWidth, bmp.bmHeight, 0,
            srcFormat, GL_UNSIGNED_BYTE, bmp.bmBits);

        haveTexture = true;
    }

    auto drawTexturedPlate = [&](GLfloat verts[][2], int n, float zOffset)
        {
            // compute bounds for planar UVs
            float minX = verts[0][0], maxX = verts[0][0];
            float minY = verts[0][1], maxY = verts[0][1];
            for (int i = 1; i < n; ++i) {
                if (verts[i][0] < minX) minX = verts[i][0];
                if (verts[i][0] > maxX) maxX = verts[i][0];
                if (verts[i][1] < minY) minY = verts[i][1];
                if (verts[i][1] > maxY) maxY = verts[i][1];
            }
            float invW = (maxX > minX) ? 1.0f / (maxX - minX) : 1.0f;
            float invH = (maxY > minY) ? 1.0f / (maxY - minY) : 1.0f;

            // Front face (z = zOffset)
            glBegin(GL_POLYGON);
            for (int i = 0; i < n; ++i) {
                float s = (verts[i][0] - minX) * invW;
                float t = (verts[i][1] - minY) * invH;
                glTexCoord2f(s, t);
                glVertex3f(verts[i][0], verts[i][1], zOffset);
            }
            glEnd();

            // Back face (z = zOffset - depth)
            glBegin(GL_POLYGON);
            for (int i = 0; i < n; ++i) {
                float s = (verts[i][0] - minX) * invW;
                float t = (verts[i][1] - minY) * invH;
                glTexCoord2f(s, t);
                glVertex3f(verts[i][0], verts[i][1], zOffset - depth);
            }
            glEnd();

            // Side walls with edge-length wrapped U
            auto edgeLen = [&](int a, int b) {
                float dx = verts[b][0] - verts[a][0];
                float dy = verts[b][1] - verts[a][1];
                return sqrtf(dx * dx + dy * dy);
                };
            float perim = 0.f;
            for (int i = 0; i < n; ++i) perim += edgeLen(i, (i + 1) % n);
            float invP = (perim > 0.f) ? 1.f / perim : 1.f;

            float uAccum = 0.f;
            for (int i = 0; i < n; ++i) {
                int j = (i + 1) % n;
                float seg = edgeLen(i, j);
                float u0 = uAccum * invP, u1 = (uAccum + seg) * invP;
                uAccum += seg;

                glBegin(GL_QUADS);
                glTexCoord2f(u0, 0.0f); glVertex3f(verts[i][0], verts[i][1], zOffset);
                glTexCoord2f(u1, 0.0f); glVertex3f(verts[j][0], verts[j][1], zOffset);
                glTexCoord2f(u1, 1.0f); glVertex3f(verts[j][0], verts[j][1], zOffset - depth);
                glTexCoord2f(u0, 1.0f); glVertex3f(verts[i][0], verts[i][1], zOffset - depth);
                glEnd();
            }
        };

    if (haveTexture) {
        // Base plate at its current Z
        drawTexturedPlate(basePlate, 6, /*zOffset*/ 0.0f);

        // Lower plate 0.1 behind
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -0.1f);
        drawTexturedPlate(lowerPlate, 6, /*zOffset*/ 0.0f);
        glPopMatrix();

        // Restore default env + disable texturing for anything drawn after
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        // Fallback to your original color fill (unchanged)
        float baseFront[3] = { 0.6f, 0.6f, 1.0f };
        float baseBack[3] = { 0.4f, 0.4f, 0.8f };
        float baseSide[3] = { 0.3f, 0.3f, 0.6f };
        float lowerFront[3] = { 0.2f, 0.2f, 1.0f };
        float lowerBack[3] = { 0.1f, 0.1f, 1.0f };
        float lowerSide[3] = { 0.3f, 0.3f, 0.6f };

        drawPolygonPlate(basePlate, 6, depth, baseFront, baseBack, baseSide);
        glPushMatrix(); glTranslatef(0.0f, 0.0f, -0.1f);
        drawPolygonPlate(lowerPlate, 6, depth, lowerFront, lowerBack, lowerSide);
        glPopMatrix();
    }

    // Cleanup texture resources
    if (tex) glDeleteTextures(1, &tex);
    if (h)   DeleteObject(h);
}


void drawCenterPlate(float depth = 0.1f) {
    // ======== Main Chest Plate ========
    GLfloat chestPlate[5][2] = {
        {-0.5f, 0.3f},
        {-0.4f, -0.2f},
        { 0.0f, -0.4f},
        { 0.4f, -0.2f},
        { 0.5f, 0.3f}
    };

    // ======== Collar ========
    GLfloat collarOutline[8][2] = {
        {0.15f, 0.25f}, {-0.2f, 0.45f}, {-0.3f, 0.38f}, {-0.15f, 0.25f},
        {0.15f, 0.25f}, {0.30f, 0.38f}, {0.2f, 0.45f}, {0.0f, 0.32f}
    };
    GLfloat collar[7][2] = {
        {-0.2f, 0.45f}, {-0.3f, 0.38f}, {-0.15f, 0.25f},
        { 0.15f, 0.25f}, { 0.30f, 0.38f}, { 0.2f, 0.45f}, {0.0f, 0.42f}
    };

    // ===== Colors =====
    float frontColor[3] = { 0.6f, 0.6f, 1.0f };
    float backColor[3] = { 0.4f, 0.4f, 0.8f };
    float sideColor[3] = { 0.3f, 0.3f, 0.6f };
    float collarFrontColor[3] = { 0.969f, 0.973f, 0.988f };
    float collarBackColor[3] = { 0.969f, 0.973f, 0.988f };
    float collarSideColor[3] = { 0.969f, 0.973f, 0.988f };
    float outlineColor[3] = { 0.031f, 0.082f, 0.49f };

    // ==== Texture (same flow as boot) ====
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    HBITMAP h = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), CurrentTex(0),
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    BITMAP bmp = {};
    GLuint tex = 0;
    bool haveTexture = false;

    if (h && GetObject(h, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits &&
        (bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32))
    {
        GLenum srcFormat = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
        GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glColor3f(1.0f, 1.0f, 1.0f); // no tint

        glTexImage2D(GL_TEXTURE_2D, 0, internal,
            bmp.bmWidth, bmp.bmHeight, 0,
            srcFormat, GL_UNSIGNED_BYTE, bmp.bmBits);

        haveTexture = true;
    }

    // ===== Outline (keep untextured) =====
    glPushMatrix();
    glScalef(1.05f, 1.05f, 1.05f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    drawPolygonPlate(chestPlate, 5, depth, outlineColor, outlineColor, outlineColor);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // ===== Fill =====
    if (haveTexture) {
        glColor3f(0.0f, 0.039f, 0.941f);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        // Compute UV bounds for front/back mapping
        float minX = chestPlate[0][0], maxX = chestPlate[0][0];
        float minY = chestPlate[0][1], maxY = chestPlate[0][1];
        for (int i = 1; i < 5; ++i) {
            if (chestPlate[i][0] < minX) minX = chestPlate[i][0];
            if (chestPlate[i][0] > maxX) maxX = chestPlate[i][0];
            if (chestPlate[i][1] < minY) minY = chestPlate[i][1];
            if (chestPlate[i][1] > maxY) maxY = chestPlate[i][1];
        }
        float invW = (maxX > minX) ? 1.0f / (maxX - minX) : 1.0f;
        float invH = (maxY > minY) ? 1.0f / (maxY - minY) : 1.0f;

        // --- Front face (z = 0) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 5; ++i) {
            float s = (chestPlate[i][0] - minX) * invW;
            float t = (chestPlate[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(chestPlate[i][0], chestPlate[i][1], 0.0f);
        }
        glEnd();

        // --- Back face (z = -depth) ---
        glBegin(GL_POLYGON);
        for (int i = 0; i < 5; ++i) {
            float s = (chestPlate[i][0] - minX) * invW;
            float t = (chestPlate[i][1] - minY) * invH;
            glTexCoord2f(s, t);
            glVertex3f(chestPlate[i][0], chestPlate[i][1], -depth);
        }
        glEnd();

        // --- Side faces (simple edge-wrap UVs) ---
        auto edgeLen = [&](int i0, int i1) {
            float dx = chestPlate[i1][0] - chestPlate[i0][0];
            float dy = chestPlate[i1][1] - chestPlate[i0][1];
            return sqrtf(dx * dx + dy * dy);
            };
        float perimeter = 0.0f;
        for (int i = 0; i < 5; ++i) perimeter += edgeLen(i, (i + 1) % 5);
        float invPerim = (perimeter > 0.0f) ? 1.0f / perimeter : 1.0f;

        float uAccum = 0.0f;
        for (int i = 0; i < 5; ++i) {
            int j = (i + 1) % 5;
            float seg = edgeLen(i, j);
            float u0 = uAccum * invPerim;
            float u1 = (uAccum + seg) * invPerim;
            uAccum += seg;

            glBegin(GL_QUADS);
            glTexCoord2f(u0, 0.0f); glVertex3f(chestPlate[i][0], chestPlate[i][1], 0.0f);
            glTexCoord2f(u1, 0.0f); glVertex3f(chestPlate[j][0], chestPlate[j][1], 0.0f);
            glTexCoord2f(u1, 1.0f); glVertex3f(chestPlate[j][0], chestPlate[j][1], -depth);
            glTexCoord2f(u0, 1.0f); glVertex3f(chestPlate[i][0], chestPlate[i][1], -depth);
            glEnd();
        }

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        // Turn off texturing for the collar
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        // Fallback to your existing solid-color plate
        drawPolygonPlate(chestPlate, 5, depth, frontColor, backColor, sideColor);
    }

    // ===== Collar (flat colors) =====
    glPushMatrix();
    glTranslatef(0.0f, -0.05f, 0.05f);
    glPushMatrix();
    glScalef(1.05f, 1.05f, 1.05f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    drawPolygonPlate(collarOutline, 8, 0.10f, outlineColor, outlineColor, outlineColor);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    drawPolygonPlate(collar, 7, 0.10f, collarFrontColor, collarBackColor, collarSideColor);
    glPopMatrix();

    // Cleanup (same pattern you used on the boot)
    if (tex) glDeleteTextures(1, &tex);
    if (h)   DeleteObject(h);
}

// ------------
// === Belt ===
// ------------
void drawBelt(float innerRadius = 0.45f, float outerRadius = 0.55f,
    float zHalf = 0.2f, int segments = 64)
{
    float beltFront[3] = { 0.3f, 0.3f, 0.4f };
    float beltBack[3] = { 0.25f, 0.25f, 0.35f };
    float beltSide[3] = { 0.2f, 0.2f, 0.3f };

    float angleStep = 2.0f * 3.1415926f / segments;

    // === Side walls (tube) ===
    for (int i = 0; i < segments; i++)
    {
        float theta = i * angleStep;
        float nextTheta = (i + 1) * angleStep;

        float cosT = cos(theta), sinT = sin(theta);
        float cosNext = cos(nextTheta), sinNext = sin(nextTheta);

        glBegin(GL_QUADS);
        glColor3fv(beltSide);

        // Outer wall
        glVertex3f(outerRadius * cosT, outerRadius * sinT, -zHalf);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, -zHalf);
        glVertex3f(outerRadius * cosNext, outerRadius * sinNext, zHalf);
        glVertex3f(outerRadius * cosT, outerRadius * sinT, zHalf);

        // Inner wall (flip for correct facing)
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, -zHalf);
        glVertex3f(innerRadius * cosT, innerRadius * sinT, -zHalf);
        glVertex3f(innerRadius * cosT, innerRadius * sinT, zHalf);
        glVertex3f(innerRadius * cosNext, innerRadius * sinNext, zHalf);
        glEnd();
    }

    // === Front face (z = +zHalf) ===
    glBegin(GL_TRIANGLE_STRIP);
    for (int j = 0; j <= segments; j++)
    {
        float theta = j * angleStep;
        glColor3fv(beltFront);
        glVertex3f(innerRadius * cos(theta), innerRadius * sin(theta), zHalf);
        glVertex3f(outerRadius * cos(theta), outerRadius * sin(theta), zHalf);
    }
    glEnd();

    // === Back face (z = -zHalf) ===
    glBegin(GL_TRIANGLE_STRIP);
    for (int j = 0; j <= segments; j++)
    {
        float theta = j * angleStep;
        glColor3fv(beltBack);
        glVertex3f(innerRadius * cos(theta), innerRadius * sin(theta), -zHalf);
        glVertex3f(outerRadius * cos(theta), outerRadius * sin(theta), -zHalf);
    }
    glEnd();

    // === Decorative Holes (front face) ===
    glColor3f(1.0f, 1.0f, 1.0f); // White
    float holeRadius = 0.05f;
    float holeRingRadius = outerRadius + 0.02f; // mid of belt
    float holeVerticalOffset = 0.0f;

    for (int i = 0; i < segments; i += 6)  // spacing
    {
        float theta = i * angleStep;
        float holeX = holeRingRadius * cos(theta);
        float holeY = holeRingRadius * sin(theta);

        // Hole circle drawn in XZ plane so it's vertical
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(holeX, holeY, 0.051f + holeVerticalOffset); // center of hole
        for (int j = 0; j <= 20; j++)
        {
            float ang = j * 2.0f * 3.1415926f / 20;
            glVertex3f(
                holeX + holeRadius * cos(ang), // X direction (around belt)
                holeY,                         // Y is fixed, circle is vertical
                0.051f + holeRadius * sin(ang) + holeVerticalOffset
            );
        }
        glEnd();
    }
}

// -----------------------
// === Pelvis and Hip ===
// -----------------------
void drawPelvis()
{
    GLUquadric* quad = gluNewQuadric();

    glPushMatrix();
    glColor3f(0.8f, 0.6f, 0.5f); // Slightly darker tone
    glScalef(1.2f, 0.2f, 0.6f);  // Wider than tall
    gluCylinder(quad, 0.4f, 0.2f, 1.9f, 20, 20);
    gluDeleteQuadric(quad);
    glPopMatrix();
}

void drawHipPlate()
{
    float hipFrontColor[3] = { 0.82f, 0.82f, 0.831f }; // Light steel
    float hipBackColor[3] = { 0.6f, 0.6f, 0.7f };     // Slightly darker
    float hipSideColor[3] = { 0.5f, 0.5f, 0.6f };     // Even darker

    GLfloat hipPlate[4][2] = {
        {-0.4f,  0.1f},
        {-0.3f, -0.6f},
        { 0.3f, -0.6f},
        { 0.4f,  0.1f}
    };

    // ====== Draw main hip plate ======
    drawPolygonPlate(hipPlate, 4, 0.2f, hipFrontColor, hipBackColor, hipSideColor);

    // ====== Diamond vertex (rotated square shape) ======
    GLfloat diamond[4][2] = {
        {  0.0f,  0.2f},   // Top
        { -0.15f,  0.0f},  // Left
        {  0.0f, -0.2f},   // Bottom
        {  0.15f,  0.0f}   // Right
    };

    // ====== Draw black outline (slightly larger) ======
    GLfloat diamondOutline[4][2] = {
        {  0.0f,  0.26f },
        { -0.21f, 0.0f },
        {  0.0f, -0.26f },
        {  0.21f, 0.0f }
    };

    float outlineFront[3] = { 1.0f, 0.361f, 0.0f };     // Dark orange
    float outlineBack[3] = { 1.0f, 0.361f, 0.0f };
    float outlineSide[3] = { 1.0f, 0.361f, 0.0f };

    glPushMatrix();
    glTranslatef(0.0f, -0.3f, 0.105f); // Slightly behind the gold diamond
    drawPolygonPlate(diamondOutline, 4, 0.01f, outlineFront, outlineBack, outlineSide);
    glPopMatrix();

    // ====== Draw golden diamond ======
    float goldFront[3] = { 1.0f, 0.851f, 0.278f };
    float goldBack[3] = { 0.8f, 0.7f, 0.1f };
    float goldSide[3] = { 0.6f, 0.5f, 0.1f };

    glPushMatrix();
    glTranslatef(0.0f, -0.3f, 0.11f); // On top of the outline
    drawPolygonPlate(diamond, 4, 0.02f, goldFront, goldBack, goldSide);
    glPopMatrix();

    // ====== Small inverted pentagon plate ======
    GLfloat smallPentagon[5][2] = {
        {  0.0f, -0.70f }, // Bottom tip
        { -0.35f, -0.60f },
        { -0.30f, -0.45f },
        {  0.30f, -0.45f },
        {  0.35f, -0.60f }
    };

    float pentFront[3] = { 0.82f, 0.82f, 0.831f };
    float pentBack[3] = { 0.55f, 0.55f, 0.65f };
    float pentSide[3] = { 0.45f, 0.45f, 0.55f };

    glPushMatrix();
    glTranslatef(0.0f, -0.2f, 0.0f); // Slightly below the current plate
    drawPolygonPlate(smallPentagon, 5, 0.15f, pentFront, pentBack, pentSide);
    glPopMatrix();

    // ====== Back Hip Plate ======
    GLfloat backPlate[8][2] = {
        {-0.35f, -0.05f},
        {-0.55f, 0.15f},  // Top left
        {-0.45f, -0.55f}, // Bottom-left slant
        {-0.15f,  -0.75f}, // Bottom tip
        {0.15f, -0.75f},
        { 0.45f, -0.55f}, // Bottom-right slant
        { 0.55f, 0.15f},   // Top right
        { 0.35f, -0.05f}
    };

    float backFrontColor[3] = { 0.55f, 0.55f, 0.7f };
    float backSideColor[3] = { 0.45f, 0.45f, 0.6f };
    float backOutlineColor[3] = { 0.1f, 0.1f, 0.3f };

    // --- Outline ---
    glPushMatrix();
    glTranslatef(0.0f, -0.4f, -0.02f); // Behind current hip plate
    glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);
    glScalef(1.05f, 1.05f, 1.0f);     // Slightly larger outline
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    drawPolygonPlate(backPlate, 8, 0.12f, backOutlineColor, backOutlineColor, backOutlineColor);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // --- Fill ---
    glPushMatrix();
    glTranslatef(0.0f, -0.4f, -0.025f);
    drawPolygonPlate(backPlate, 8, 0.12f, backFrontColor, backFrontColor, backSideColor);
    glPopMatrix();

    // ====== Belt Between Front & Back Plates ======
    glPushMatrix();
    glTranslatef(0.0f, -0.6f, -0.5f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f); // Lay flat
    glScalef(1.8f, 1.2f, 1.0f);         // Oval (waist size)
    drawBelt(0.35f, 0.4f, 0.15f, 64);   // More segments = smoother
    glPopMatrix();
}

// -------------------------------------------------
// ================ Lower Body Part ================
// -------------------------------------------------
// ---------- Colors ----------
static float COL_METAL[3] = { 0.55f, 0.55f, 0.65f };
static float COL_METAL_D[3] = { 0.949f, 0.949f, 0.945f };
static float COL_BACK[3] = { 0.38f, 0.38f, 0.48f };
static float COL_RIM[3] = { 0.75f, 0.75f, 0.82f };
static float COL_RIVET[3] = { 1.0f, 0.84f, 0.0f };

// Tasset palette variants
static float COL_TASSET_FRONT[3] = { 0.60f, 0.60f, 0.72f };
static float COL_TASSET_FRONT_RIM[3] = { 0.941f, 0.647f, 0.098f };
static float COL_TASSET_BACK[3] = { 0.48f, 0.48f, 0.60f };
static float COL_TASSET_BACK_RIM[3] = { 0.80f,  0.55f,  0.12f };
static float COL_TASSET_BACKFACE[3] = { 0.32f, 0.32f, 0.42f };

float Z_EPS = 0.003f;   // tiny push toward camera (front is z=0)


// ---------- Primitives ----------
static void drawRectPlate(float w, float h, float t,
    float* fc = COL_METAL, float* bc = COL_BACK, float* sc = COL_RIM)
{
    GLfloat v[4][2] = {
        {-w * 0.5f, -h * 0.5f}, { w * 0.5f, -h * 0.5f},
        { w * 0.5f,  h * 0.5f}, {-w * 0.5f,  h * 0.5f}
    };
    drawPolygonPlate(v, 4, t, fc, bc, sc);
}

static void drawRhombusPlate2(float w, float h, float t,
    float* fc = COL_METAL, float* bc = COL_BACK, float* sc = COL_METAL_D)
{
    GLfloat v[4][2] = { {0,-h * 0.5f}, {w * 0.5f,0}, {0,h * 0.5f}, {-w * 0.5f,0} };
    drawPolygonPlate(v, 4, t, fc, bc, sc);
}

static void drawRivet2(float r = 0.03f, float h = 0.02f, const float* col = COL_RIVET)
{
    glColor3fv(col);
    drawRing(r * 0.55f, r * 0.95f, h * 0.30f, 16);      // washer

    glPushMatrix();                                // dome cap
    glTranslatef(0, 0, h * 0.55f);
    glScalef(r, r, r);
    GLUquadric* sphere = gluNewQuadric();
    glColor3fv(col);
    gluSphere(sphere, 1.0, 16, 16);
    gluDeleteQuadric(sphere);
    glPopMatrix();
}

static void drawSpike(float w = 0.12f, float h = 0.12f, float t = 0.06f)
{
    float x = w * 0.5f, z = t * 0.5f;
    glColor3fv(COL_METAL);
    glBegin(GL_TRIANGLES);
    glVertex3f(-x, 0, z); glVertex3f(x, 0, z); glVertex3f(0, h, z);
    glVertex3f(-x, 0, -z); glVertex3f(0, h, -z); glVertex3f(x, 0, -z);
    glVertex3f(-x, 0, -z); glVertex3f(-x, 0, z); glVertex3f(0, h, 0);
    glVertex3f(x, 0, z); glVertex3f(x, 0, -z); glVertex3f(0, h, 0);
    glEnd();
}


// ---------- Trousers ----------
void drawTrouserLeg(float upperRadius = 0.4f, float lowerRadius = 0.6f, float length = 3.0f)
{
    // Load once, reuse every call
    static GLuint gTrouserTex = 0;
    if (!gTrouserTex) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        HBITMAP hBmp = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), "trouser.bmp",
            IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

        if (hBmp) {
            BITMAP bmp = {};
            if (GetObject(hBmp, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits) {
                glGenTextures(1, &gTrouserTex);
                glBindTexture(GL_TEXTURE_2D, gTrouserTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                GLenum srcFmt = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
                GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

                glTexImage2D(GL_TEXTURE_2D, 0, internal,
                    bmp.bmWidth, bmp.bmHeight, 0,
                    srcFmt, GL_UNSIGNED_BYTE, bmp.bmBits);

                glBindTexture(GL_TEXTURE_2D, 0);
            }
            DeleteObject(hBmp);
        }
    }

    // Scale as before
    glPushMatrix();
    glScalef(1.0f, 1.3f, 1.0f);

    // Bind + draw textured cylinder
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gTrouserTex);
    glColor3f(1.0f, 1.0f, 1.0f);                           // no tint
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluCylinder(quad, upperRadius, lowerRadius, length, 20, 20);
    gluDeleteQuadric(quad);

    // Restore state
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}

struct TrouserCtrl {
    float splayZDeg;             // hip abduction around Z  (outward)
    float toeOutYDeg;            // toe-out around Y
    float swingXDeg;             // forward/back swing around X
    float lateralX;              // side offset from pelvis anchor
    float upperR = 0.35f, lowerR = 0.60f, length = 2.5f;
};

static void drawTrouserSide(bool left, const TrouserCtrl& c)
{
    glPushMatrix();
    glRotatef(left ? -c.splayZDeg : c.splayZDeg, 0, 0, 1);
    glTranslatef(left ? -c.lateralX : c.lateralX, 0, 0);
    glRotatef(left ? -c.toeOutYDeg : c.toeOutYDeg, 0, 1, 0);
    glRotatef(90.0f + c.swingXDeg, 1, 0, 0);     // point down + swing
    drawTrouserLeg(c.upperR, c.lowerR, c.length);
    glPopMatrix();
}


// ---------- Tassets ----------
static void drawTassetEx(float w, float h, float t,
    float yawOutDeg, float swingDeg,
    float* faceCol, float* backCol, float* rimCol)
{
    glPushMatrix();

    // Hinge at the TOP center so the plate swings from the belt line
    glTranslatef(0, +h * 0.5f, 0);
    glRotatef(yawOutDeg, 0, 0, 1);   // outward “splay” around Z
    glRotatef(swingDeg, 1, 0, 0);  // forward/back swing around X
    glTranslatef(0, -h * 0.5f, 0);

    // main plate
    drawRectPlate(w, h, t, faceCol, backCol, rimCol);

    // border strips (nudged forward)
    glPushMatrix(); glTranslatef(0, 0, Z_EPS);
    drawRectPlate(w * 1.02f, 0.10f, t * 1.05f, rimCol, backCol, faceCol); glPopMatrix();
    glPushMatrix(); glTranslatef(0, -h * 0.5f + 0.05f, Z_EPS);
    drawRectPlate(w * 1.02f, 0.10f, t * 1.05f, rimCol, backCol, faceCol); glPopMatrix();
    glPushMatrix(); glTranslatef(-w * 0.5f + 0.04f, 0, Z_EPS);
    drawRectPlate(0.08f, h * 1.02f, t * 1.05f, rimCol, backCol, faceCol); glPopMatrix();
    glPushMatrix(); glTranslatef(w * 0.5f - 0.04f, 0, Z_EPS);
    drawRectPlate(0.08f, h * 1.02f, t * 1.05f, rimCol, backCol, faceCol); glPopMatrix();

    // rivets
    const float zF = 0.001f, pitch = 0.22f, y0 = h * 0.5f - 0.18f;
    for (float y = y0; y > -y0; y -= pitch) {
        glPushMatrix(); glTranslatef(-w * 0.5f, y, zF); drawRivet2(0.03f, 0.02f, COL_RIVET); glPopMatrix();
        glPushMatrix(); glTranslatef(w * 0.5f, y, zF); drawRivet2(0.03f, 0.02f, COL_RIVET); glPopMatrix();
    }
    for (float x = -w * 0.45f; x <= w * 0.45f; x += pitch) {
        glPushMatrix(); glTranslatef(x, h * 0.5f, zF); drawRivet2(0.03f, 0.02f, COL_RIVET); glPopMatrix();
        glPushMatrix(); glTranslatef(x, -h * 0.5f, zF); drawRivet2(0.03f, 0.02f, COL_RIVET); glPopMatrix();
    }

    // spikes
    float gx = w * 0.28f, gy = -h * 0.5f - 0.02f;
    glPushMatrix(); glTranslatef(-gx, gy, zF); drawSpike(); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, gy, zF); drawSpike(); glPopMatrix();
    glPushMatrix(); glTranslatef(gx, gy, zF); drawSpike(); glPopMatrix();

    glPopMatrix();
}

// A layered (back+front) tasset stack positioned to one side
struct TassetStackCtrl {
    float sideX;           // lateral anchor offset of the stack (≈ 0.55)
    float backDropY;      // Y offset of back plate (lower)
    float backZ;          // Z of back plate (behind)
    float frontDropY;     // Y offset of front plate (higher)
    float frontZ;         // Z of front plate (forward)
    float twistYdeg;      // outward twist (+right / -left)
    float splayZdegBack;  // Z splay for back
    float splayZdegFront; // Z splay for front
    float yawOutDeg;      // passed to drawTassetEx (sign handled per-side)
    float swingDeg;       // passed to drawTassetEx (animate)
};

static void drawTassetStack(bool left, const TassetStackCtrl& c)
{
    const float s = left ? -1.0f : +1.0f;

    // Back (bigger)
    glPushMatrix();
    glTranslatef(s * c.sideX, c.backDropY, c.backZ);
    glRotatef(s * c.twistYdeg, 0, 1, 0);
    glRotatef(s * c.splayZdegBack, 0, 0, 1);
    drawTassetEx(0.70f, 1.60f, 0.06f,
        s * c.yawOutDeg, c.swingDeg,
        COL_TASSET_BACK, COL_TASSET_BACKFACE, COL_TASSET_BACK_RIM);
    glPopMatrix();

    // Front (smaller)
    glPushMatrix();
    glTranslatef(s * c.sideX, c.frontDropY, c.frontZ);
    glRotatef(s * c.twistYdeg, 0, 1, 0);
    glRotatef(s * c.splayZdegFront, 0, 0, 1);
    drawTassetEx(0.50f, 1.10f, 0.06f,
        s * c.yawOutDeg, c.swingDeg,
        COL_TASSET_FRONT, COL_TASSET_BACKFACE, COL_TASSET_FRONT_RIM);
    glPopMatrix();
}


// ---------- Center & Apron ----------
static void drawCenterAbPlate()
{
    // backdrop / rim / face / inlay (you can customize as desired)
    glPushMatrix(); glTranslatef(0, 0, -0.010f);
    drawRhombusPlate2(0.60f, 0.42f, 0.08f, COL_RIM, COL_BACK, COL_METAL); glPopMatrix();

    glPushMatrix(); glTranslatef(0, 0, Z_EPS * 0.5f);
    drawRhombusPlate2(0.60f, 0.42f, 0.08f, COL_RIM, COL_BACK, COL_METAL); glPopMatrix();

    drawRhombusPlate2(0.60f, 0.42f, 0.08f, COL_RIM, COL_BACK, COL_METAL);

    glPushMatrix(); glTranslatef(0, 0, Z_EPS * 0.6f);
    drawRhombusPlate2(0.60f, 0.42f, 0.08f, COL_RIM, COL_BACK, COL_METAL); glPopMatrix();

    glPushMatrix(); glTranslatef(0, 0, Z_EPS * 1.2f);
    drawRectPlate(0.36f, 0.08f, 0.05f, COL_RIM, COL_BACK, COL_METAL); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0, Z_EPS * 1.2f);
    drawRectPlate(0.06f, 0.40f, 0.05f, COL_RIM, COL_BACK, COL_METAL); glPopMatrix();

    float GEM[3] = { 1.00f, 0.86f, 0.26f };
    glPushMatrix(); glTranslatef(0, 0, Z_EPS * 2.0f);
    drawRhombusPlate2(0.26f, 0.18f, 0.06f, GEM, GEM, GEM); glPopMatrix();

    const float zF = Z_EPS * 1.6f, w2 = 0.55f * 0.5f, h2 = 0.38f * 0.5f;
    glPushMatrix(); glTranslatef(0.0f, h2, zF); drawRivet2(0.022f, 0.018f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, -h2, zF); drawRivet2(0.022f, 0.018f); glPopMatrix();
    glPushMatrix(); glTranslatef(w2, 0.0f, zF); drawRivet2(0.022f, 0.018f); glPopMatrix();
    glPushMatrix(); glTranslatef(-w2, 0.0f, zF); drawRivet2(0.022f, 0.018f); glPopMatrix();
}

static void drawLamellarApron(int n = 9, float totalW = 1.1f, float h = 0.42f, float t = 0.04f)
{
    float gap = totalW / n;
    for (int i = 0; i < n; ++i) {
        float x = -totalW * 0.5f + (i + 0.5f) * gap;
        glPushMatrix();
        glTranslatef(x, -h * 0.35f, Z_EPS);
        glRotatef(8.0f, 1, 0, 0);
        drawRectPlate(gap * 0.85f, h, t, COL_METAL, COL_BACK, COL_RIM);
        glPushMatrix(); glTranslatef(-gap * 0.25f, h * 0.5f - 0.05f, 0.001f); drawRivet2(0.025f, 0.02f); glPopMatrix();
        glPushMatrix(); glTranslatef(gap * 0.25f, h * 0.5f - 0.05f, 0.001f); drawRivet2(0.025f, 0.02f); glPopMatrix();
        glPopMatrix();
    }
}

static void drawFrontCenter()
{
    glPushMatrix(); glTranslatef(0.0f, -0.02f, 0.20f); glRotatef(7.0f, 1, 0, 0);
    drawCenterAbPlate(); glPopMatrix();

    glPushMatrix(); glTranslatef(0.0f, -0.05f, -0.04f);
    drawLamellarApron(); glPopMatrix();
}


// ---------- Side Assembly (everything that moves with one leg) ----------
struct SideRig {
    // Trouser
    TrouserCtrl trouser;
    // Tassets
    TassetStackCtrl tassets;
    // Anchors (world-ish)
    float pelvisY = 0.8f, pelvisZ = 2.0f;   // trouser anchor
    float frontCY = 0.35f, frontCZ = 2.56f;  // tasset/apron anchor
};

static void drawLowerBodySide(bool left, const SideRig& s)
{
    // --- Trousers (at pelvis anchor) ---
    glPushMatrix();
    glTranslatef(0.0f, s.pelvisY, s.pelvisZ);
    drawTrouserSide(left, s.trouser);
    glPopMatrix();

    // --- Tassets (at front-center anchor) ---
    glPushMatrix();
    glTranslatef(0.0f, s.frontCY, s.frontCZ);
    glRotatef(-10.0f, 1, 0, 0);
    drawTassetStack(left, s.tassets);
    glPopMatrix();
}


// ---------- Whole Lower Body wrapper ----------
static void drawLowerBody(const SideRig& L, const SideRig& R)
{
    // Hip + belt are drawn by your own function (not moved with legs)
    //   drawHipPlate();  <-- call outside if you want

    // Center group once
    drawFrontCenter();

    // Left & right sides (independent)
    drawLowerBodySide(true, L);
    drawLowerBodySide(false, R);
}

// ===================== Long Boots (silver) =====================
// Palette (harmonized with your tassets)
static float BOOT_METAL[3] = { 0.80f, 0.80f, 0.88f };
static float BOOT_METAL_D[3] = { 0.58f, 0.58f, 0.70f };
static float BOOT_RIM[3] = { 0.95f, 0.95f, 0.98f };
static float BOOT_BACK[3] = { 0.42f, 0.42f, 0.52f };
static float BOOT_LEATHER[3] = { 0.22f, 0.13f, 0.08f };
static float BOOT_GOLD[3] = { 1.00f, 0.84f, 0.00f };

// — small helper to drop a vertical rivet column
static void rivetColumn(float x, float yTop, float yBot, float step, float z = 0.001f) {
    for (float y = yTop; y >= yBot; y -= step) {
        glPushMatrix(); glTranslatef(x, y, z); drawRivet2(0.024f, 0.018f, BOOT_GOLD); glPopMatrix();
    }
}

// Front shin plate (tall rectangle + border + rivets + 3 spikes)
static void drawShinPlate(float w = 0.42f, float h = 1.70f, float t = 0.06f)
{
    // main face
    drawRectPlate(w, h, t, BOOT_METAL, BOOT_BACK, BOOT_RIM);

    // lifted border (thin)
    glPushMatrix(); glTranslatef(0, 0, Z_EPS);
    drawRectPlate(w * 1.02f, 0.10f, t * 1.05f, BOOT_RIM, BOOT_BACK, BOOT_METAL); glPopMatrix();
    glPushMatrix(); glTranslatef(0, -h * 0.5f + 0.05f, Z_EPS);
    drawRectPlate(w * 1.02f, 0.10f, t * 1.05f, BOOT_RIM, BOOT_BACK, BOOT_METAL); glPopMatrix();
    glPushMatrix(); glTranslatef(-w * 0.5f + 0.04f, 0, Z_EPS);
    drawRectPlate(0.08f, h * 1.02f, t * 1.05f, BOOT_RIM, BOOT_BACK, BOOT_METAL); glPopMatrix();
    glPushMatrix(); glTranslatef(w * 0.5f - 0.04f, 0, Z_EPS);
    drawRectPlate(0.08f, h * 1.02f, t * 1.05f, BOOT_RIM, BOOT_BACK, BOOT_METAL); glPopMatrix();

    // rivets (sides + top/bottom)
    float yTop = h * 0.5f - 0.16f, yBot = -h * 0.5f + 0.16f;
    rivetColumn(-w * 0.5f, yTop, yBot, 0.22f);
    rivetColumn(w * 0.5f, yTop, yBot, 0.22f);
    for (float x = -w * 0.45f; x <= w * 0.45f; x += 0.22f) {
        glPushMatrix(); glTranslatef(x, h * 0.5f, Z_EPS); drawRivet2(0.024f, 0.018f, BOOT_GOLD); glPopMatrix();
        glPushMatrix(); glTranslatef(x, -h * 0.5f, Z_EPS); drawRivet2(0.024f, 0.018f, BOOT_GOLD); glPopMatrix();
    }

    // 3 spikes at the bottom edge
    float gx = w * 0.28f, gy = -h * 0.5f - 0.03f;
    glPushMatrix(); glTranslatef(-gx, gy, Z_EPS); drawSpike(); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, gy, Z_EPS); drawSpike(); glPopMatrix();
    glPushMatrix(); glTranslatef(gx, gy, Z_EPS); drawSpike(); glPopMatrix();
}

// Knee-cap (slightly domed hex plate)
static void drawKneeCap(float w = 0.60f, float h = 0.40f, float t = 0.08f)
{
    GLfloat v[6][2] = {
        {-w * 0.50f,  0.00f}, {-w * 0.30f,  h * 0.50f}, {0.0f, h * 0.60f},
        { w * 0.30f,  h * 0.50f}, {w * 0.50f, 0.00f}, {0.0f,-h * 0.55f}
    };

    // outline pass
    float outline[3] = { 0,0,0 };
    glPushMatrix();
    glScalef(1.05f, 1.05f, 1.05f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    drawPolygonPlate(v, 6, t, outline, outline, outline);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // fill pass
    drawPolygonPlate(v, 6, t, BOOT_METAL, BOOT_BACK, BOOT_RIM);

    // corner rivets
    float z = Z_EPS * 1.2f;
    glPushMatrix(); glTranslatef(-w * 0.42f, 0.02f, z); drawRivet2(0.022f, 0.018f, BOOT_GOLD); glPopMatrix();
    glPushMatrix(); glTranslatef(w * 0.42f, 0.02f, z); drawRivet2(0.022f, 0.018f, BOOT_GOLD); glPopMatrix();
}

// Outer side strap + two buckles (mirrors for left/right)
static void drawOuterStraps(bool left, float y0 = 0.55f, float sep = 0.28f)
{
    float s = left ? -1.0f : +1.0f;
    // two leather bands
    for (int i = 0; i < 2; ++i) {
        float y = y0 + i * sep;
        glPushMatrix();
        glTranslatef(s * 0.26f, y, 0.00f);
        glRotatef(90, 0, 1, 0);  // make ring encircle calf
        glColor3fv(BOOT_LEATHER);
        drawRing(0.20f, 0.24f, 0.04f, 18);
        glPopMatrix();

        // simple rectangular buckle on outer-most side
        glPushMatrix();
        glTranslatef(s * 0.36f, y, 0.03f);
        glScalef(0.18f, 0.10f, 0.04f);
        drawRectPlate(1.0f, 1.0f, 0.5f, BOOT_RIM, BOOT_BACK, BOOT_METAL);
        glPopMatrix();
    }
}

// --------------------
// === Legs ===
// --------------------
void drawUpperLeg()
{
    GLUquadric* quad = gluNewQuadric();
    glColor3f(1.0f, 0.8f, 0.6f);
    gluCylinder(quad, 0.25f, 0.22f, 2.0f, 20, 20);
    gluDeleteQuadric(quad);
}

void drawLowerLeg()
{
    GLUquadric* quad = gluNewQuadric();
    glColor3f(0.95f, 0.75f, 0.6f);
    gluCylinder(quad, 0.22f, 0.18f, 1.8f, 20, 20);
    gluDeleteQuadric(quad);
}

// --------------------
// ===== War Boot =====
// --------------------
// --- Base foot only (no shin plate) ---
void drawFootBase()
{
    // Sole (slimmer + seated under ankle)
    glColor3f(0.2f, 0.1f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, -0.10f, 0.02f);
    glScalef(0.58f, 0.12f, 0.95f);
    drawCube(1.0f);
    glPopMatrix();

    // Boot body (ankle tube)
    glColor3f(0.25f, 0.15f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, 0.10f, -0.05f);
    glRotatef(-90.0f, 1, 0, 0);
    GLUquadric* quad = gluNewQuadric();
    gluCylinder(quad, 0.25f, 0.22f, 0.65f, 20, 1);
    gluDeleteQuadric(quad);
    glPopMatrix();

    // Toe cap
    glColor3f(0.3f, 0.2f, 0.1f);
    glPushMatrix();
    glTranslatef(0.0f, -0.03f, 0.47f);
    glScalef(0.48f, 0.16f, 0.38f);
    drawCube(1.0f);
    glPopMatrix();
}

// Full long boot (foot + greave + knee + cuffs)
static void drawLongBoot(bool left = false)
{
    glPushMatrix();
    // LEG SPACE → BOOT SPACE
    // Leg local frame (after thigh rotate): +Z = down.
    // Our boot module expects +Y = up. Convert once here.
    glRotatef(-90.0f, 1, 0, 0);

    // 1) ankle strap
    glPushMatrix();
    glTranslatef(0.0f, 0.18f, 0.0f);
    glColor3fv(BOOT_LEATHER);
    drawRing(0.23f, 0.31f, 0.05f, 24);
    glPopMatrix();

    // 2) tall greave wrap (metal cylinder, vertical along Y)
    glPushMatrix();
    glTranslatef(0.0f, 0.15f, 0.0f);
    glRotatef(-90.0f, 1, 0, 0);              // GLU cylinder is Z-axis → rotate to Y
    // === Load silverMetal.bmp (same steps as IceCream.cpp) ===
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    HBITMAP h = (HBITMAP)LoadImageA(GetModuleHandleA(NULL), "silverMetal.bmp",
        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    BITMAP bmp = {};
    GLuint tex = 0;
    bool haveTexture = false;

    if (h && GetObject(h, sizeof(bmp), &bmp) == sizeof(bmp) && bmp.bmBits &&
        (bmp.bmBitsPixel == 24 || bmp.bmBitsPixel == 32))
    {
        GLenum srcFormat = (bmp.bmBitsPixel == 32) ? GL_BGRA_EXT : GL_BGR_EXT;
        GLint  internal = (bmp.bmBitsPixel == 32) ? GL_RGBA : GL_RGB;

        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glColor3f(1.0f, 1.0f, 1.0f); // avoid tinting
        glTexImage2D(GL_TEXTURE_2D, 0, internal,
            bmp.bmWidth, bmp.bmHeight, 0,
            srcFormat, GL_UNSIGNED_BYTE, bmp.bmBits);

        haveTexture = true;
    }

    // === Draw the greave cylinder with texture coords like in IceCream.cpp ===
    GLUquadric* q = gluNewQuadric();
    if (haveTexture) gluQuadricTexture(q, GL_TRUE);  // generate (s,t) for the quadric

    // (keep your dimensions)
    gluCylinder(q, 0.30f, 0.27f, 1.70f, 28, 1);

    // Cleanup (exactly like your cone/sauce code)
    if (haveTexture) {
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (tex) glDeleteTextures(1, &tex);
    if (h)   DeleteObject(h);

    gluDeleteQuadric(q);
    glPopMatrix();

    // 3) front shin plate (sit slightly IN FRONT, not behind)
    glPushMatrix();
    glTranslatef(0.0f, 0.95f, +0.18f);       // was -0.18f: flip to +Z (boot-space front)
    glRotatef(-10.0f, 1, 0, 0);
    drawShinPlate();
    glPopMatrix();

    // 4) outer side straps + buckles (auto-mirrored)
    drawOuterStraps(left, /*y0*/0.55f, /*sep*/0.32f);

    // 5) top cuff ring
    glPushMatrix();
    glTranslatef(0.0f, 1.85f, 0.20f);
    glRotatef(180, 0, 1, 0);
    glColor3fv(BOOT_METAL);
    drawRing(0.28f, 0.36f, 0.08f, 28);
    glPopMatrix();

    // 6) knee cap (slight front offset)
    glPushMatrix();
    glTranslatef(0.0f, 1.95f, +0.20f);       // was -0.04f
    glRotatef(180, 0, 1, 0);
    drawKneeCap();
    glPopMatrix();

    glPopMatrix(); // close frame adapter
}

// ======================
// === Silver Shoes  ===
// ======================

static float COL_SOLE[3] = { 0.10f, 0.10f, 0.12f };   // dark rubber
static float COL_TRIM[3] = { 0.45f, 0.47f, 0.55f };   // darker metal trim

// Y-up local shoe model (origin ≈ ankle center)
static void drawShoe_Yup(bool left)
{
    // --- sole ---
    glColor3fv(COL_SOLE);
    glPushMatrix();
    glTranslatef(0.00f, -0.08f, 0.04f);
    glScalef(0.8f, 0.10f, 0.42f);
    drawCube(1.0f);
    glPopMatrix();

    // --- heel block ---
    glPushMatrix();
    glTranslatef(-0.26f, -0.02f, 0.00f);
    glScalef(0.18f, 0.18f, 0.34f);
    drawCube(1.0f);
    glPopMatrix();

    // --- upper (rounded vamp) ---
    glColor3fv(COL_METAL);
    glPushMatrix();
    glTranslatef(0.08f, 0.12f, 0.00f);
    glScalef(0.58f, 0.22f, 0.44f);

    GLUquadric* joint1 = gluNewQuadric();
    glColor3f(0.776f, 0.776f, 0.8f); // soft rounded hump
    glScalef(0.8f, 1.0f, 0.5f);
    gluSphere(joint1, 1.0f, 16, 5);
    gluDeleteQuadric(joint1);

    glPopMatrix();

    // --- silver toe cap ---
    glPushMatrix();
    glTranslatef(0.33f, 0.05f, 0.00f);
    glScalef(0.30f, 0.16f, 0.42f);

    GLUquadric* joint2 = gluNewQuadric();
    glColor3f(0.49f, 0.318f, 0.239f); // soft rounded hump
    glScalef(0.8f, 1.0f, 0.5f);
    gluSphere(joint2, 1.0f, 16, 16);
    gluDeleteQuadric(joint2);

    glPopMatrix();

    // --- ankle cuff ring ---
    glPushMatrix();
    glTranslatef(0.0f, 0.28f, 0.0f);
    glRotatef(90.0f, 1, 0, 0);
    glColor3fv(COL_METAL);
    drawRing(0.25f, 0.31f, 0.06f, 24);
    glPopMatrix();

    // --- strap band around the shoe ---
    glPushMatrix();
    glTranslatef(0.0f, 0.18f, 0.0f);
    drawRectPlate(0.82f, 0.08f, 0.04f, COL_METAL_D, COL_BACK, COL_METAL);
    glPopMatrix();

    // --- front panel with two rivets (matches your sketch) ---
    glPushMatrix();
    glTranslatef(0.00f, 0.21f, 0.20f);                // sit slightly forward
    drawRectPlate(0.28f, 0.30f, 0.05f, COL_METAL, COL_BACK, COL_TRIM);
    glTranslatef(0, 0, Z_EPS);
    glPushMatrix(); glTranslatef(0.0f, -0.065f, 0.0f); drawRivet2(0.022f, 0.02f); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, 0.065f, 0.0f); drawRivet2(0.022f, 0.02f); glPopMatrix();
    glPopMatrix();
}

// Wrapper that converts your leg space (+Z down) → shoe space (Y-up)
// and adds a small toe-out so L/R are slightly angled like in the sketch.
static void drawShoe(bool left)
{
    glPushMatrix();
    glRotatef(-90.0f, 1, 0, 0);                   // leg Z-down -> Y-up
    glRotatef(-90.0f, 0, 1, 0);
    glRotatef(left ? -6.0f : 6.0f, 0, 1, 0);      // toe-out
    drawShoe_Yup(left);
    glPopMatrix();
}

void drawLeftLeg()
{
    // ===== Walk/Idle gait (your originals) =====
    const float ampMul = running ? kRunAmpMul : 1.0f;
    const float hipSwingAmp = 30.0f * ampMul;
    const float kneeFlexAmp = 30.0f * ampMul;

    const float phase = walkPhase;                 // left leg in-phase with walkPhase
    const float hipFX = hipSwingAmp * sinf(phase); // X swing
    const float kneeRaw = kneeFlexAmp * (sinf(phase) + 0.2f);
    const float kneeWalk = (kneeRaw > 0.0f) ? kneeRaw : 0.0f;

    // ---- Idle pose targets (translate + Euler) ----
    const float hipTx_idle = -0.4f, hipTy_idle = 0.3f, hipTz_idle = 2.0f;
    const float hipRX_idle = 90.0f + hipFX;  // leg points down (90°) + swing
    const float hipRY_idle = 0.0f;
    const float hipRZ_idle = 0.0f;
    const float kneeRX_idle = kneeWalk;
    const float kneeRY_idle = 0.0f;
    const float kneeRZ_idle = 0.0f;

    // ===== Combat targets (your previous combat branch) =====
    const float hipTx_cb = -0.6f, hipTy_cb = 0.3f, hipTz_cb = 2.0f; // wider stance
    const float hipRX_cb = 90.0f + 40.0f;  // bend forward
    const float hipRY_cb = 0.0f;
    const float hipRZ_cb = +18.0f;         // abduction (spread)
    const float kneeRX_cb = -40.0f;        // deep bend
    const float kneeRY_cb = +20.0f;
    const float kneeRZ_cb = +8.0f;

    // ===== Blend factor =====
    const float t = CurrentLowerBodyCombatT();

    // ---- Lerp helpers (require Lerp from earlier step) ----
    const float hipTx = Lerp(hipTx_idle, hipTx_cb, t);
    const float hipTy = Lerp(hipTy_idle, hipTy_cb, t);
    const float hipTz = Lerp(hipTz_idle, hipTz_cb, t);

    float hipRX = Lerp(hipRX_idle, hipRX_cb, t);
    const float hipRY = Lerp(hipRY_idle, hipRY_cb, t);
    const float hipRZ = Lerp(hipRZ_idle, hipRZ_cb, t);

    float kneeRX = Lerp(kneeRX_idle, kneeRX_cb, t);
    const float kneeRY = Lerp(kneeRY_idle, kneeRY_cb, t);
    const float kneeRZ = Lerp(kneeRZ_idle, kneeRZ_cb, t);

    // === Manual leg offsets (final layer) ===
    if (gUpperLegLevelL > 0) hipRX -= kHipLiftDeg[gUpperLegLevelL];
    if (gLowerLegLevelL > 0) kneeRX += kKneeFlexDeg[gLowerLegLevelL];

    // ===== Draw =====
    glPushMatrix();
    // Hip joint
    glTranslatef(hipTx, hipTy, hipTz);
    drawJoint(0.2f);

    // Apply hip rotations (keep order consistent with your branches)
    glRotatef(hipRX, 1, 0, 0);
    glRotatef(hipRZ, 0, 0, 1);
    glRotatef(hipRY, 0, 1, 0);
    drawUpperLeg();

    // Knee / lower leg
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.0f);
    drawJoint(0.2f);
    glRotatef(kneeRX, 1, 0, 0);
    glRotatef(kneeRY, 0, 1, 0);
    glRotatef(kneeRZ, 0, 0, 1);
    drawLowerLeg();

    // Ankle / boot + shoe
    glTranslatef(0.0f, 0.0f, 1.8f);
    drawJoint(0.15f);
    drawLongBoot(true);

    // Foot tip / ankle pitch (LEFT)
    float footPitchL = 0.0f;
    if (gFootTipLevelL > 0) footPitchL += kFootTipDeg[gFootTipLevelL];
    glRotatef(footPitchL, 1, 0, 0);

    drawShoe(true);

    glPopMatrix(); // knee
    glPopMatrix(); // hip
}

void drawRightLeg()
{
    // ===== Walk/Idle gait (opposite phase) =====
    const float ampMul = running ? kRunAmpMul : 1.0f;
    const float hipSwingAmp = 30.0f * ampMul;
    const float kneeFlexAmp = 30.0f * ampMul;

    const float PI = 3.1415926f;
    const float phase = walkPhase + PI;            // 180° out of phase
    const float hipFX = hipSwingAmp * sinf(phase); // X swing
    const float kneeRaw = kneeFlexAmp * (sinf(phase) + 0.2f);
    const float kneeWalk = (kneeRaw > 0.0f) ? kneeRaw : 0.0f;

    // ---- Idle targets ----
    const float hipTx_idle = 0.4f, hipTy_idle = 0.3f, hipTz_idle = 2.0f;
    const float hipRX_idle = 90.0f + hipFX;
    const float hipRY_idle = 0.0f;
    const float hipRZ_idle = 0.0f;
    const float kneeRX_idle = kneeWalk;
    const float kneeRY_idle = 0.0f;
    const float kneeRZ_idle = 0.0f;

    // ===== Combat targets (straighter back leg, spread right) =====
    const float hipTx_cb = 0.6f, hipTy_cb = 0.3f, hipTz_cb = 2.0f;
    const float hipRX_cb = 90.0f - 55.0f;
    const float hipRY_cb = +28.0f;
    const float hipRZ_cb = -18.0f;
    const float kneeRX_cb = 48.0f;
    const float kneeRY_cb = 0.0f;
    const float kneeRZ_cb = 0.0f;

    // ===== Blend =====
    const float t = CurrentLowerBodyCombatT();

    const float hipTx = Lerp(hipTx_idle, hipTx_cb, t);
    const float hipTy = Lerp(hipTy_idle, hipTy_cb, t);
    const float hipTz = Lerp(hipTz_idle, hipTz_cb, t);

    float hipRX = Lerp(hipRX_idle, hipRX_cb, t);   // <-- mutable
    const float hipRY = Lerp(hipRY_idle, hipRY_cb, t);
    const float hipRZ = Lerp(hipRZ_idle, hipRZ_cb, t);

    float kneeRX = Lerp(kneeRX_idle, kneeRX_cb, t); // <-- mutable
    const float kneeRY = Lerp(kneeRY_idle, kneeRY_cb, t);
    const float kneeRZ = Lerp(kneeRZ_idle, kneeRZ_cb, t);

    // === Manual leg offsets (final layer; use RIGHT levels) ===
    if (gUpperLegLevelR > 0) hipRX -= kHipLiftDeg[gUpperLegLevelR];
    if (gLowerLegLevelR > 0) kneeRX += kKneeFlexDeg[gLowerLegLevelR];

    // ===== Draw =====
    glPushMatrix();
    // Hip joint
    glTranslatef(hipTx, hipTy, hipTz);
    drawJoint(0.2f);

    // Hip rotations (keep same order as left/combat)
    glRotatef(hipRX, 1, 0, 0);
    glRotatef(hipRZ, 0, 0, 1);
    glRotatef(hipRY, 0, 1, 0);
    drawUpperLeg();

    // Knee / lower leg
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 2.0f);
    drawJoint(0.2f);
    glRotatef(kneeRX, 1, 0, 0);
    glRotatef(kneeRY, 0, 1, 0);
    glRotatef(kneeRZ, 0, 0, 1);
    drawLowerLeg();

    // Ankle / boot + shoe (apply toe/ankle pitch HERE)
    glTranslatef(0.0f, 0.0f, 1.8f);
    drawJoint(0.15f);
    drawLongBoot(false);

    float footPitchR = 0.0f;
    if (gFootTipLevelR > 0) footPitchR += kFootTipDeg[gFootTipLevelR];
    glRotatef(footPitchR, 1, 0, 0);          // <-- toe tip on the ankle joint
    
    drawShoe(false);

    glPopMatrix(); // knee
    glPopMatrix(); // hip
}

//==========================
// === Character Weapon ===
//==========================
// --------------- WEAPON HELPER ----------------
static void drawHexPlate(float r, float thick) {
    const int N = 6;
    const float PI = 3.1415926f, step = 2.0f * PI / N;
    // front
    glBegin(GL_POLYGON);
    for (int i = 0; i < N; ++i) { float a = i * step; glVertex3f(r * cosf(a), r * sinf(a), 0.0f); }
    glEnd();
    // back
    glBegin(GL_POLYGON);
    for (int i = 0; i < N; ++i) { float a = i * step; glVertex3f(r * cosf(a), r * sinf(a), -thick); }
    glEnd();
    // sides
    glBegin(GL_QUADS);
    for (int i = 0; i < N; ++i) {
        int j = (i + 1) % N; float ai = i * step, aj = j * step;
        float xi = r * cosf(ai), yi = r * sinf(ai);
        float xj = r * cosf(aj), yj = r * sinf(aj);
        glVertex3f(xi, yi, 0.0f);   glVertex3f(xj, yj, 0.0f);
        glVertex3f(xj, yj, -thick);  glVertex3f(xi, yi, -thick);
    }
    glEnd();
}

// thin triangular prism pointing +Y (knife-like)
static void drawTriPrismBlade(float halfW, float thick, float len) {
    glBegin(GL_TRIANGLES); // front tip
    glVertex3f(0.0f, len, 0.0f); glVertex3f(-halfW, 0.0f, thick); glVertex3f(halfW, 0.0f, thick);
    glEnd();
    glBegin(GL_TRIANGLES); // back tip
    glVertex3f(0.0f, len, 0.0f); glVertex3f(halfW, 0.0f, -thick); glVertex3f(-halfW, 0.0f, -thick);
    glEnd();
    glBegin(GL_QUADS);     // left, right, base
    glVertex3f(-halfW, 0.0f, thick); glVertex3f(-halfW, 0.0f, -thick); glVertex3f(0.0f, len, 0.0f); glVertex3f(0.0f, len, 0.0f);
    glVertex3f(halfW, 0.0f, -thick); glVertex3f(halfW, 0.0f, thick); glVertex3f(0.0f, len, 0.0f); glVertex3f(0.0f, len, 0.0f);
    glVertex3f(-halfW, 0.0f, thick); glVertex3f(halfW, 0.0f, thick); glVertex3f(halfW, 0.0f, -thick); glVertex3f(-halfW, 0.0f, -thick);
    glEnd();
}

// Flat-topped hex, lying in XZ plane, thickness along +Y
// keepTop = true  -> keep the top half (z >= 0)
// keepTop = false -> keep the bottom half (z <= 0)
void drawHalfHexPlate(float r, float thickness, bool keepTop = true)
{
    // Precompute √3/2 * r
    const float h = 0.866025403784f * r; // sqrt(3)/2 * r

    // 4 vertices of a "half hex" (flat-topped)
    // TOP half (z >= 0):  A(r,0), B(r/2, +h), C(-r/2, +h), D(-r,0)
    // BOTTOM half (z <= 0): D(-r,0), E(-r/2, -h), F(r/2, -h), A(r,0)
    struct V { float x, z; };
    V v[4];
    if (keepTop) {
        v[0] = { +r,   0.0f };
        v[1] = { +r / 2, +h };
        v[2] = { -r / 2, +h };
        v[3] = { -r,   0.0f };
    }
    else {
        v[0] = { -r,   0.0f };
        v[1] = { -r / 2, -h };
        v[2] = { +r / 2, -h };
        v[3] = { +r,   0.0f };
    }

    // ---- Top face (y = thickness) ----
    glBegin(GL_POLYGON);
    glNormal3f(0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 4; ++i) glVertex3f(v[i].x, thickness, v[i].z);
    glEnd();

    // ---- Bottom face (y = 0) ----
    glBegin(GL_POLYGON);
    glNormal3f(0.0f, -1.0f, 0.0f);
    for (int i = 3; i >= 0; --i) glVertex3f(v[i].x, 0.0f, v[i].z);
    glEnd();

    // ---- Side walls (quads between bottom/top rings) ----
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        // Compute a simple outward normal per edge in XZ plane
        float ex = v[j].x - v[i].x;
        float ez = v[j].z - v[i].z;
        float nx = +ez;
        float nz = -ex;
        float len = sqrtf(nx * nx + nz * nz);
        if (len > 0.0f) { nx /= len; nz /= len; }

        glNormal3f(nx, 0.0f, nz);
        glVertex3f(v[i].x, 0.0f, v[i].z);
        glVertex3f(v[j].x, 0.0f, v[j].z);
        glVertex3f(v[j].x, thickness, v[j].z);
        glVertex3f(v[i].x, thickness, v[i].z);
    }
    glEnd();
}

// Draw a ring band that wraps a cylinder aligned with +Z (after you rotate -90 on X).
// innerR = radius of the cylinder it hugs, outerR = band radius (a bit larger), h = band height.
static void drawRingBand(GLUquadric* q, float innerR, float outerR, float h) {
    // outer wall
    gluCylinder(q, outerR, outerR, h, 32, 1);

    // bottom cap (annulus)
    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);      // face outward
    gluDisk(q, innerR, outerR, 32, 1);
    glPopMatrix();

    // top cap (annulus)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, h);
    gluDisk(q, innerR, outerR, 32, 1);
    glPopMatrix();
}
// --------------------- WEAPON HELPER END ---------------------

// ========================== WEAPON ==========================
void drawWeapon() {
    GLUquadric* quad = gluNewQuadric();

    // ==== Bottom spike + band ====
    // bottom spike
    glPushMatrix();
    glColor3f(0.85f, 0.85f, 0.75f);
    glTranslatef(0.0f, -0.25f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, 0.05f, 0.1f, 0.22f, 24, 4);
    glPopMatrix();
    // bottom
    glPushMatrix();
    glColor3f(0.8f, 0.8f, 0.2f);  //brown yellow
    glTranslatef(0.0f, -0.05f, 0.0f);
    glRotatef(-90, 1, 0, 0);
    gluCylinder(quad, 0.07f, 0.1f, 0.3f, 20, 20);
    glPopMatrix();

    // ===== LONG SHAFT =====
    glPushMatrix();
    glRotatef(-90.0f, 1, 0, 0);    // Align vertically (+Z)
    float shaftR = 0.07f, shaftH = 8.20f;

    // base shaft (white)
    glColor3f(1.0f, 1.0f, 1.0f);
    gluCylinder(quad, shaftR, shaftR, shaftH, 30, 1);

    // long metal cover just below hub
    glPushMatrix();
    glColor3f(0.8f, 0.8f, 0.85f);
    // place near the top of the shaft (hub sits above at ~7.4)
    float coverHeight = 1.3f;       // adjust how tall you want the sleeve
    float coverZ = shaftH - coverHeight - 0.4f; // position right under hub
    glTranslatef(0.0f, 0.0f, coverZ);
    drawRingBand(quad, shaftR, shaftR + 0.03f, coverHeight);
    glPopMatrix();
    glPopMatrix();

    // ==== Inner upper rod (slimmer) + link ring ====
    // inner rod
    glPushMatrix();
    //glColor3f(0.80f, 0.80f, 0.80f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, 6.0f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, 0.035f, 0.035f, 0.90f, 24, 1);
    glPopMatrix();

    // link ring
    glPushMatrix();
    glColor3f(0.15f, 0.15f, 0.15f);
    glTranslatef(0.0f, 7.75f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quad, 0.07f, 0.07f, 0.12f, 24, 1);
    glPopMatrix();

    // ===== HUB CYLINDER =====
    glPushMatrix();
    glTranslatef(0.0f, 7.40f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);

    // main hub body
    glColor3f(0.55f, 0.55f, 0.55f);
    float hubR = 0.18f, hubH = 0.65f;
    gluCylinder(quad, hubR, hubR, hubH, 32, 1);

    // metal rings (slightly larger radius than hub)
    glColor3f(0.8f, 0.8f, 0.2f);
    glPushMatrix();                                 // ring near bottom
    glTranslatef(0.0f, 0.0f, 0.0f);
    drawRingBand(quad, hubR, hubR + 0.025f, 0.06f);
    glPopMatrix();

    glPushMatrix();                                 // ring near top
    glTranslatef(0.0f, 0.0f, hubH - 0.03f);
    drawRingBand(quad, hubR, hubR + 0.025f, 0.06f);
    glPopMatrix();
    glPopMatrix();



    // --- METAL BUTTONS (front & back), 3 from top to bottom ---
    {
        const float hubBaseY = 7.4f;     // hub base y (after your transforms)
        const float hubLen = 0.65f;    // hub height along +Y
        const float hubR = 0.18f;    // hub radius

        const float btnR = 0.035f;   // button radius
        const float btnDepth = 0.02f;    // how far it sticks out
        const float lift = 0.002f;   // tiny gap to avoid Z-fighting with hub surface

        // 3 y-positions from top to bottom, evenly spaced on the hub
        const float topY = hubBaseY + hubLen - 0.08f;  // small margin from top edge
        const float stepY = -(hubLen - 0.16f) / 2.0f;   // distribute 3 buttons inside margins

        glColor3f(1.0f, 1.0f, 1.0f); // slightly brighter metal for buttons

        for (int k = 0; k < 3; ++k) {
            float yPos = topY + k * stepY;  // k=0 top, k=2 bottom

            // ---------- FRONT (facing +Z) ----------
            glPushMatrix();
            glTranslatef(0.0f, yPos, hubR + lift); // sit on front surface
            // Cylinder axis is +Z by default, so it sticks outward toward +Z
            gluCylinder(quad, btnR, btnR, btnDepth, 24, 1);

            // front button caps
            // back cap (touching hub face)
            gluDisk(quad, 0.0f, btnR, 24, 1);
            // front cap
            glTranslatef(0.0f, 0.0f, btnDepth);
            gluDisk(quad, 0.0f, btnR, 24, 1);
            glPopMatrix();

            // ---------- BACK (facing -Z) ----------
            glPushMatrix();
            glTranslatef(0.0f, yPos, -(hubR + lift)); // sit on back surface
            glRotatef(180.0f, 0.0f, 1.0f, 0.0f);      // make cylinder axis point toward -Z
            gluCylinder(quad, btnR, btnR, btnDepth, 24, 1);

            // back button caps
            gluDisk(quad, 0.0f, btnR, 24, 1);         // cap touching hub
            glTranslatef(0.0f, 0.0f, btnDepth);
            gluDisk(quad, 0.0f, btnR, 24, 1);         // outer cap
            glPopMatrix();
        }
    }

    // SEMI HEX - plate that connect the knife and shaft
    glPushMatrix();
    glColor3f(0.827f, 0.827f, 0.827f); // Light gray
    glTranslatef(0.0f, 8.32f, 0.0f);
    glTranslatef(0.0f, 0.0f, -0.09f);
    glScalef(1.5f, 0.7f, 1.0f);

    // If your camera looks along +Z:
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    // draw double thickness
    drawHalfHexPlate(0.45f, 0.08f, /*keepTop=*/true);
    glTranslatef(0.0f, 0.08f, 0.0f);
    drawHalfHexPlate(0.45f, 0.08f, /*keepTop=*/true);
    glPopMatrix();


    // blades: main + two sides (all straight up)
    // main long knife
    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    glTranslatef(0.0f, 8.60f, 0.0f);
    drawTriPrismBlade(0.20f, 0.025f, 1.60f);
    glPopMatrix();

    // ===== Side blades (upright model, no tilt downward) =====
    const float BASE_Y = 9.02f;   // attach height near the collar/hub
    const float OFF = 0.35f;   // lateral offset from center
    const float CONN_H = 0.20f;   // connector height

    // LEFT blade — up-left (65°)
    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    glTranslatef(-0.15, 8.65f, 0.0f);          // move left side
    glRotatef(65.0f, 0.0f, 0.0f, 1.0f);        // rotate in XY: up-left
    drawTriPrismBlade(0.14f, 0.022f, 0.90f);   // (halfWidth, thickness, length)
    glPopMatrix();

    // RIGHT blade — up-right (-45°)
    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    glTranslatef(0.15f, 8.65f, 0.0f);          // move right side
    glRotatef(-65.0f, 0.0f, 0.0f, 1.0f);       // rotate in XY: up-right
    drawTriPrismBlade(0.14f, 0.022f, 0.90f);
    glPopMatrix();

    // ====== Connectors for side blades ======
    // LEFT connector polygon
    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    glBegin(GL_POLYGON);
    glVertex3f(-0.50f, BASE_Y - 0.05f, 0.05f);   // top left
    glVertex3f(-0.95f, BASE_Y, 0.05f);   // blade root left
    glVertex3f(-0.65f, BASE_Y - CONN_H, 0.0f); // bottom left
    glVertex3f(-0.25f, BASE_Y - CONN_H, 0.0f); // bottom hub attach
    glEnd();
    glPopMatrix();

    // RIGHT connector polygon
    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    //glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_POLYGON);
    glVertex3f(0.50f, BASE_Y - 0.05f, 0.05f);   // top right
    glVertex3f(0.95f, BASE_Y, 0.05f);   // blade root right
    glVertex3f(0.65f, BASE_Y - CONN_H, 0.0f); // bottom right
    glVertex3f(0.25f, BASE_Y - CONN_H, 0.0f); // bottom hub attach
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glColor3f(0.97f, 0.97f, 0.97f);
    glBegin(GL_POLYGON);
    glVertex3f(0.25f, 8.65f, 0.0f); // bottom right
    glVertex3f(-0.25f, 8.65f, 0.05f);   // top right
    glVertex3f(-0.12f, 8.25f, 0.05f);   // blade root right
    glVertex3f(0.12f, 8.25f, 0.0f); // bottom hub attach
    glEnd();
    glPopMatrix();
    gluDeleteQuadric(quad);
}

// ==========================
// ===== Weapon Display =====
// ==========================
static void drawWeaponDisplay()
{
    //if (combatBlend < 0.25f) return;
    // Early-out:
    if (combatBlend < 0.25f || weaponState != WPN_IDLE || attackState != ATK_IDLE) return;
    glPushMatrix();

    // Character root placement (display() places character at 0,+1,0)
    glTranslatef(gCharX, 1.0f, gCharZ);

    // Root combat tweaks (same as character)
    {
        float t = combatBlend;
        float yawY = -18.0f * t;
        float leanX = 10.0f * t;
        float dropY = -0.28f * t;
        glRotatef(gCharYawDeg + yawY, 0, 1, 0);
        glRotatef(leanX, 1, 0, 0);
        glTranslatef(0, dropY, 0);
    }

    // Your previous “on-body display socket” + subtle bob
    glTranslatef(-1.52f, 3.8f, -0.6f);
    glScalef(0.6f, 0.6f, 0.6f);
    glRotatef(26.5f, 1, 0, 0);
    glRotatef(-53.0f, 0, 1, 0);
    glRotatef(-85.0f, 0, 0, 1);

    // gentle bob while in combat idle
    float tC = combatBlend;
    glRotatef(3.0f * tC * sinf(combatBobPh), 1, 0, 0);
    glRotatef(2.5f * tC * sinf(0.9f * combatBobPh), 0, 1, 0);
    glRotatef(2.0f * tC * sinf(1.3f * combatBobPh), 0, 0, 1);
    glTranslatef(0.0f, 0.0f, 0.10f * sinf(1.8f * combatBobPh) * tC);

    drawWeapon();
    glPopMatrix();
}

// ==========================
// === Character Assembly ===
// ==========================
void drawCharacter()
{
    glPushMatrix();
    // === Root Transform ===
    // --- Controllable Locomotion ---
    // Place character in world
    glTranslatef(gCharX, 1.0f, gCharZ);

    // PIVOT-CORRECTED yaw so rotation happens "in place"
    glTranslatef(+kPivotX, 0.0f, +kPivotZ);
    glRotatef(gCharYawDeg, 0, 1, 0);
    glTranslatef(-kPivotX, 0.0f, -kPivotZ);

    // Uniform scale
    glScalef(0.7f, 0.7f, 0.7f);
    
    // --- Smooth body turn/lean/height using combatBlend (was a hard combatPose branch) ---
    {
        const float t = combatBlend; // 0 = normal, 1 = combat
        // From your commented combat tweak:
        //   yaw -18°, pitch +10°, translate Y -0.28
        const float yawY = -18.0f * t;
        const float leanX = 10.0f * t;
        const float dropY = -0.28f * t;

        glRotatef(yawY, 0.0f, 1.0f, 0.0f);
        glRotatef(leanX, 1.0f, 0.0f, 0.0f);
        glTranslatef(0.0f, dropY, 0.0f);
    }

    // --- Attack 'N' temporary half-squat overlay (independent of combatBlend) ---
    if (attackState != ATK_IDLE) {
        float a = 0.0f;
        if (attackState == ATK_WINDUP)      a = SmoothStep01(attackTimer / kAtkWindup);
        else if (attackState == ATK_SWEEP)  a = 1.0f;
        else if (attackState == ATK_RECOVER)a = 1.0f - SmoothStep01(attackTimer / kAtkRecover);

        // smaller than full combat: subtle yaw, lean, and drop
        glRotatef(-10.0f * a, 0, 1, 0);
        glRotatef(6.0f * a, 1, 0, 0);
        glTranslatef(0.0f, -0.18f * a, 0.0f);
    }

    // --- Gait bob/lean (dampen as we approach combat) ---
    if (walking) {
        const float t = combatBlend;
        const float baseAmp = running ? 0.10f : 0.04f;
        const float bobAmp = baseAmp * (1.0f - 0.70f * t);     // fade bob in combat
        float bobY = bobAmp * sinf(2.0f * walkPhase);           // 2× step frequency
        glTranslatef(0.0f, bobY, 0.0f);

        // keep your running forward lean, but fade out in combat
        if (running) glRotatef(6.0f * (1.0f - t), 1.0f, 0.0f, 0.0f);
    }

    // ======================
    // === Upper Body ===
    // ======================
    glPushMatrix();
    // Torso
    glPushMatrix();
    glTranslatef(0.0f, 1.2f, 2.0f);
    glScalef(1.0f, 2.0f, 0.7f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawBody();

    // Chest Plates
    glPushMatrix();
    glTranslatef(0.0f, -0.70f, 0.6f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawCenterPlate();
    glPopMatrix();

    glPushMatrix(); // Left chest
    glRotatef(90.0f, 50.0f, 0.0f, 0.0f);
    glScalef(0.8f, 0.8f, 0.8f);
    glTranslatef(-0.7f, 0.75f, 1.0f);
    drawChestPlate();
    glPopMatrix();

    glPushMatrix(); // Right chest
    glRotatef(90.0f, 50.0f, 0.0f, 0.0f);
    glScalef(-0.8f, 0.8f, 0.8f);
    glTranslatef(-0.7f, 0.75f, 1.0f);
    drawChestPlate();
    glPopMatrix();
    glPopMatrix(); // torso

    // Neck
    glPushMatrix();
    glTranslatef(0.0f, 2.9f, 2.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawNeck();
    glPopMatrix();

    // Head
    glPushMatrix();
    glTranslatef(0.0f, 3.7f, 2.1f);  // <- head anchor
    glScalef(1.40f, 1.30f, 1.35f);       // wider + bigger
    glRotatef(gHeadYawDeg, 0, 1, 0);  // shake (left/right)
    glRotatef(gHeadPitchDeg, 1, 0, 0);  // nod (down/up)
    drawHead();                      // <- spherical head now
    glPopMatrix();

    // Shoulders + Arms
    drawLeftArm();
    drawRightArm();

    // Shoulder Blocks
    glPushMatrix();  // Left block
    glTranslatef(-0.9f, 3.0f, 2.4f);
    glColor3f(0.4f, 0.4f, 0.8f);
    drawShouderBlock();
    glPopMatrix();

    glPushMatrix();  // Right block
    glTranslatef(0.9f, 3.0f, 2.4f);
    glScalef(-1.0f, 1.0f, 1.0f);
    glColor3f(0.4f, 0.4f, 0.8f);
    drawShouderBlock();
    glPopMatrix();

    // Shoulder Armors
    glPushMatrix();  // Left armor
    glTranslatef(-1.0f, 3.0f, 2.5f);
    drawShoulderArmor();
    glPopMatrix();

    glPushMatrix();  // Right armor
    glTranslatef(1.0f, 3.0f, 2.5f);
    glScalef(-1.0f, 1.0f, 1.0f);
    drawShoulderArmor();
    glPopMatrix();

    glPopMatrix(); // === End Upper Body ===

    // ======================
    // === Lower Body =======
    // ======================
    glPushMatrix();
    glTranslatef(0.0f, 1.6f, 2.5f);   // belt/hip area if you need it
    drawHipPlate();
    glPopMatrix();

    // Build default rigs; animate these per frame
    SideRig Lrig, Rrig;

    // Forward/back leg swing with sine (your originals)
    float swingAmp = 30.0f; // degrees
    float swingL = swingAmp * sin(walkPhase) * 0.5f;
    float swingR = -swingAmp * sin(walkPhase) * 0.5f; // opposite
    const float clothMul = running ? kRunAmpMul : 1.0f;

    // Trouser setup (base/idle)
    Lrig.trouser = { /*splayZ*/ 5.0f, /*toeOut*/ 6.0f,  /*swing*/ swingL * clothMul * 1.2f, /*latX*/ 0.45f };
    Rrig.trouser = { /*splayZ*/ 5.0f, /*toeOut*/ -6.0f, /*swing*/ swingR * clothMul * 1.2f, /*latX*/ 0.45f };

    // Tasset stacks (base/idle)
    TassetStackCtrl base = {
        /*sideX*/       0.57f,
        /*backDropY*/  -0.25f,
        /*backZ*/       0.03f,
        /*frontDropY*/ -0.06f,
        /*frontZ*/      0.09f,
        /*twistYdeg*/  12.0f,
        /*splayBack*/   6.0f,
        /*splayFront*/  4.0f,
        /*yawOutDeg*/   8.0f,
        /*swingDeg*/    0.0f
    };
    Lrig.tassets = base;
    Rrig.tassets = base;
    Lrig.tassets.swingDeg = swingL * 1.2f * clothMul;
    Rrig.tassets.swingDeg = swingR * 1.2f * clothMul;

    // --- Smoothly blend the cloth/trouser overrides toward combat values ---
    {
        const float t = CurrentLowerBodyCombatT();

        // Trouser combat targets (from your old combatPose branch)
        const float L_swingX_cb = +20.0f;
        const float R_swingX_cb = -30.0f;
        const float L_splayZ_cb = 10.0f, R_splayZ_cb = 14.0f;
        const float L_toe_cb = 15.0f, R_toe_cb = 15.0f;
        const float lateral_cb = 0.55f;

        // Lerp trouser fields
        Lrig.trouser.swingXDeg = Lerp(Lrig.trouser.swingXDeg, L_swingX_cb, t);
        Rrig.trouser.swingXDeg = Lerp(Rrig.trouser.swingXDeg, R_swingX_cb, t);

        Lrig.trouser.splayZDeg = Lerp(Lrig.trouser.splayZDeg, L_splayZ_cb, t);
        Rrig.trouser.splayZDeg = Lerp(Rrig.trouser.splayZDeg, R_splayZ_cb, t);
        Lrig.trouser.toeOutYDeg = Lerp(Lrig.trouser.toeOutYDeg, L_toe_cb, t);
        Rrig.trouser.toeOutYDeg = Lerp(Rrig.trouser.toeOutYDeg, R_toe_cb, t);
        Lrig.trouser.lateralX = Lerp(Lrig.trouser.lateralX, lateral_cb, t);
        Rrig.trouser.lateralX = Lerp(Rrig.trouser.lateralX, lateral_cb, t);

        // Tassets combat targets
        const float L_tSwing_cb = +12.0f, R_tSwing_cb = -35.0f;
        const float L_splayF_cb = 8.0f, R_splayF_cb = 28.0f;
        const float L_splayB_cb = 10.0f, R_splayB_cb = 10.0f;
        const float yawOut_cb = 12.0f;

        Lrig.tassets.swingDeg = Lerp(Lrig.tassets.swingDeg, L_tSwing_cb, t);
        Rrig.tassets.swingDeg = Lerp(Rrig.tassets.swingDeg, R_tSwing_cb, t);
        Lrig.tassets.splayZdegFront = Lerp(Lrig.tassets.splayZdegFront, L_splayF_cb, t);
        Rrig.tassets.splayZdegFront = Lerp(Rrig.tassets.splayZdegFront, R_splayF_cb, t);
        Lrig.tassets.splayZdegBack = Lerp(Lrig.tassets.splayZdegBack, L_splayB_cb, t);
        Rrig.tassets.splayZdegBack = Lerp(Rrig.tassets.splayZdegBack, R_splayB_cb, t);
        Lrig.tassets.yawOutDeg = Lerp(Lrig.tassets.yawOutDeg, yawOut_cb, t);
        Rrig.tassets.yawOutDeg = Lerp(Rrig.tassets.yawOutDeg, yawOut_cb, t);

        // === Link manual hip lift to cloth (final layer) ===
        float hipL = (gUpperLegLevelL > 0) ? kHipLiftDeg[gUpperLegLevelL] : 0.0f;
        float hipR = (gUpperLegLevelR > 0) ? kHipLiftDeg[gUpperLegLevelR] : 0.0f;

        // Trousers: forward/back swing tracks hip pitch 1:1
        Lrig.trouser.swingXDeg -= hipL * 0.7f;
        Rrig.trouser.swingXDeg -= hipR * 0.7f;

        // Tassets: slightly reduced so plates don't over-swing
        Lrig.tassets.swingDeg -= hipL * 0.8f;
        Rrig.tassets.swingDeg -= hipR * 0.8f;
    }

    // Draw center + both sides
    glPushMatrix();
    glTranslatef(0.0f, 0.35f, 2.56f); // center/apron anchor
    drawFrontCenter();
    glPopMatrix();

    glPushMatrix(); drawLowerBodySide(true, Lrig); glPopMatrix();
    glPushMatrix(); drawLowerBodySide(false, Rrig); glPopMatrix();

    // Legs (these now lerp their own poses internally)
    drawLeftLeg();
    drawRightLeg();

    glPopMatrix(); // === End Lower Body ===

    glPopMatrix(); // === End Character Root ===

    //special effect
    glPushMatrix();
    // place/rotate your character root here
    FX_Thunder_DrawBehind_NoGlobals();  // <- behind the back, additive lightning
    // draw the character meshes after
    glPopMatrix();
}

//--------------------------------------------------------------------
// === Helper: Draw x, y, z axes ===
void drawAxes(float length) {
    glLineWidth(2.0f);  // Make the lines a bit thicker
    glBegin(GL_LINES);

    // X axis (Red)
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    glVertex3f(-length, 0.0f, 0.0f);
    glVertex3f(length, 0.0f, 0.0f);

    // Y axis (Green)
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    glVertex3f(0.0f, -length, 0.0f);
    glVertex3f(0.0f, length, 0.0f);

    // Z axis (Blue)
    glColor3f(0.0f, 0.0f, 1.0f); // Blue
    glVertex3f(0.0f, 0.0f, -length);
    glVertex3f(0.0f, 0.0f, length);

    glEnd();
}

static void DrawPureShadow(const GLfloat lightPos[4], float groundHalfSize, float groundY)
{
    // Build projection to the actual ground plane: y = groundY
    const GLfloat plane[4] = { 0.0f, 1.0f, 0.0f, -groundY };
    GLfloat shadowMat[16];
    makeShadowMatrix_Plane(lightPos, plane, shadowMat);

    // Save a bit of state we touch
    GLboolean wasStencil = glIsEnabled(GL_STENCIL_TEST);
    GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean wasTex = glIsEnabled(GL_TEXTURE_2D);
    GLboolean wasLight = glIsEnabled(GL_LIGHTING);

    glEnable(GL_STENCIL_TEST);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    // -----------------------------------------------------------
    // 1) Stamp the ground into stencil = 1  (no color/depth writes)
    //    Disable depth test so re-drawing ground always marks stencil.
    // -----------------------------------------------------------
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    drawGroundPlane(groundHalfSize, /*tileU=*/4.0f);

    // -----------------------------------------------------------
    // 2) Project casters with shadowMat, marking stencil = 2
    //    (still no color/depth writes; depth test OFF)
    // -----------------------------------------------------------
    glStencilFunc(GL_EQUAL, 1, 0xFF);        // only where ground exists
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);  // 1 -> 2 where casters land

    glPushMatrix();
    glMultMatrixf(shadowMat);
    glTranslatef(0.0f, 0.001f, 0.0f);        // bias to avoid z-fight
    glScalef(0.4f, 0.7f, 0.92f);            // slight shrink

    // Draw casters normally — no colors will be written (stencil only)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -1.0f);
    drawCharacter();
    glPopMatrix();
    drawWeaponDisplay();

    glPopMatrix();

    // -----------------------------------------------------------
    // 3) Fill black where stencil == 2
    //    (we can keep depth test OFF so the quad always draws)
    // -----------------------------------------------------------
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    glStencilFunc(GL_EQUAL, 2, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    // Choose ONE: soft alpha darken (looks good) or multiply (very dark)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);      // pure black, 85% opacity
    // Alternative:
    // glBlendFunc(GL_DST_COLOR, GL_ZERO);   // multiply
    // glColor3f(0.0f, 0.0f, 0.0f);

    const float lift = 0.0015f;
    glBegin(GL_QUADS);
    glVertex3f(-groundHalfSize, groundY + lift, -groundHalfSize);
    glVertex3f(+groundHalfSize, groundY + lift, -groundHalfSize);
    glVertex3f(+groundHalfSize, groundY + lift, +groundHalfSize);
    glVertex3f(-groundHalfSize, groundY + lift, +groundHalfSize);
    glEnd();

    // Restore state
    glDisable(GL_BLEND);
    if (wasLight) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
    if (wasTex)   glEnable(GL_TEXTURE_2D);   else glDisable(GL_TEXTURE_2D);
    if (wasDepth) glEnable(GL_DEPTH_TEST);   else glDisable(GL_DEPTH_TEST);
    if (wasStencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}

void display()
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Frame delta time and animation update ---
    static DWORD last = timeGetTime();
    DWORD now = timeGetTime();
    float dt = (now - last) * (1.0f / 1000.0f);
    last = now;

    gRibbonTime += dt;   // always ticking

    UpdateAnimation(dt);

    // ===== Camera / ModelView =====
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(gCamX, gCamY, gCamZ,
        gCamTargetX, gCamTargetY, gCamTargetZ,
        0.0, 1.0, 0.0);

    // Model rotation (keep your arrow-key behavior)
    glRotatef(angleY, 1.0f, 0.0f, 0.0f); // pitch
    glRotatef(angleX, 0.0f, 1.0f, 0.0f); // yaw

    // ===== Light component toggles (J/K/L) =====
    GLfloat amb[] = { gAmbientOn ? 0.5f : 0.0f, gAmbientOn ? 0.5f : 0.0f, gAmbientOn ? 0.5f : 0.0f, 1.0f };
    GLfloat dif[] = { gDiffuseOn ? 0.8f : 0.0f, gDiffuseOn ? 0.8f : 0.0f, gDiffuseOn ? 0.8f : 0.0f, 1.0f };
    GLfloat spec[] = { gSpecularOn ? 1.0f : 0.0f, gSpecularOn ? 1.0f : 0.0f, gSpecularOn ? 1.0f : 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, spec);

    // ===== Orbiting spotlight: compute ONCE per frame =====
    // Hoist lightPos so it’s in scope for the shadow call later.
    GLfloat lightPos4[4];
    {
        const float yawRad = gLightYawDeg * 3.1415926f / 180.0f;
        const float lx = gCharX + gLightRadius * sinf(yawRad);
        const float lz = gCharZ + gLightRadius * cosf(yawRad);
        const float ly = gLightHeight;

        lightPos4[0] = lx; lightPos4[1] = ly; lightPos4[2] = lz; lightPos4[3] = 1.0f;

        glLightfv(GL_LIGHT0, GL_POSITION, lightPos4);

        GLfloat spotDir[] = { gCharX - lx, 3.0f - ly, gCharZ - lz };
        glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, spotDir);
        glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, gLightCutoff);
        glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 16.0f);
    }

    // ===== Environment =====
    DrawEnvironment();

    // ===== Shadow (call ONCE, using the same lightPos) =====
    DrawPureShadow(lightPos4, 60.0f, gGroundY);

    // Axes (optional)
    if (gShowAxes) drawAxes(5.0f);

    // Character
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -1.0f);
    drawCharacter();
    glPopMatrix();

    // Weapon
    drawWeaponDisplay();

    if (gShowManual) {
        DrawUserManualOverlay();
    }
    else {
        DrawManualHint();
    }

    glFlush();
}

//--------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpfnWndProc = WindowProcedure;
    wc.lpszClassName = WINDOW_TITLE;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassEx(&wc)) return false;

    HWND hWnd = CreateWindow(WINDOW_TITLE, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, wc.hInstance, NULL);

    //--------------------------------
    //	Initialize window for OpenGL
    //--------------------------------
    HDC hdc = GetDC(hWnd);
    if (!hdc) return false;

    //	initialize pixel format for the window
    initPixelFormat(hdc);

    //	get an openGL context
    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc) return false;

    //	make context current
    if (!wglMakeCurrent(hdc, hglrc)) return false;

    // -------- One-time GL state --------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // Spotlight falloff (one-time setup)
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.9f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.02f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.002f);

    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    // Light component defaults (J/K/L will override per-frame in display())
    {
        GLfloat ambientLight[] = { 0.3f, 0.3f, 0.3f, 1.0f };
        GLfloat diffuseLight[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat specularLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
        glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

        GLfloat globalAmb[] = { 0.12f, 0.12f, 0.12f, 1.0f }; // gentle baseline
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);
    }

    // Material specular for nicer highlights
    {
        GLfloat matSpec[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glMaterialfv(GL_FRONT, GL_SPECULAR, matSpec);
        glMaterialf(GL_FRONT, GL_SHININESS, 48.0f);
    }

    // Background color
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

    // Initial viewport/projection setup
    RECT rc;
    GetClientRect(hWnd, &rc);
    OnResize(rc.right - rc.left, rc.bottom - rc.top);

    //--------------------------------
    //	End initialization
    //--------------------------------
    ShowWindow(hWnd, nCmdShow);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    // Simple animation clock
    static DWORD lastTime = timeGetTime();

    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // ======== Animation ========
        // Simple timer increment
        // static DWORD lastTime = timeGetTime();
        DWORD now = timeGetTime();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        const float gaitSpeedMul = running ? kRunSpeedMul : 1.0f;

        if (walking) {
            // full cycle = 2π
            walkPhase += (walkSpeed * gaitSpeedMul) * dt * 3.1415926f * 2.0f;
        }
        if (waving) {
            wavePhase += dt * 6.0f;
        }

        display();

        SwapBuffers(hdc);
    }

    UnregisterClass(WINDOW_TITLE, wc.hInstance);

    return true;
}