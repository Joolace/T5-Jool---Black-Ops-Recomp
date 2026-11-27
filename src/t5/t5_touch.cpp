// t5_touch.cpp (bo1_touch.cpp in T5Jool 0.8) - T5Jool: comandi touch per Android.
//
// In partita lo schermo diventa un controller virtuale: levetta di movimento dinamica a sinistra, mira
// trascinando a destra, pulsanti rotondi (fuoco, mira, salto, ...). I pulsanti e la levetta finiscono in un
// controller XInput virtuale (sdl_xinput.cpp li unisce al pad vero dello slot 0), la mira in movimenti del
// mouse (sdl_user32.cpp). Nei menu il touch resta un mouse (SDL lo converte da solo) e compaiono i tasti del pad
// che i menu chiedono (A B X Y, LB RB, LT RT, Start, Back).
// Se si usa un controller vero i comandi spariscono; tornano al primo tocco.
// Dvar: bo1_touch (0/1), bo1_touch_opacity, bo1_touch_scale, bo1_touch_look (sensibilita' della mira).
#include "t5_touch.h"

#include <client/client.h>
#include <client/cl_keys.h>
#include <client_mp/cl_main_mp.h>
#include <cgame_mp/cg_newDraw_mp.h>
#include <gfx_d3d/r_font.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_rendercmds.h>
#include <qcommon/common.h>
#include <universal/dvar.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <cstdio>
#include <sys/stat.h>

#ifndef _WIN32
#include <SDL.h>
#include <windows.h>
#include <XInput.h>

extern "C" void bo1_inject_mouse_delta(int dx, int dy); // sdl_user32.cpp (dentro extern "C")

namespace
{
const dvar_s *bo1_touch, *bo1_touch_opacity, *bo1_touch_scale, *bo1_touch_look, *bo1_touch_autohide;

enum Role
{
    ROLE_NONE,
    ROLE_STICK,
    ROLE_LOOK,
    ROLE_BUTTON,
};

enum Icon
{
    ICON_FIRE,
    ICON_ADS,
    ICON_RELOAD,
    ICON_JUMP,
    ICON_CROUCH,
    ICON_SWITCH,
    ICON_KNIFE,
    ICON_NADE,
    ICON_TACT,
    ICON_PAUSE,
    ICON_SCORE,
    ICON_UP,    // croce direzionale (Moon: maschera P.E.S., oggetti e armi speciali)
    ICON_DOWN,
    ICON_LEFT,
    ICON_RIGHT,
    ICON_COUNT
};

struct Button
{
    Icon icon;
    WORD xbutton;    // bottone XInput
    int trigger;     // 1 = grilletto sinistro (mira), 2 = destro (fuoco)
    float cx, cy, r; // centro dall'angolo in basso a destra (x) / in basso (y), in unita' da 720p; cx < 0 = da sinistra
    bool top;        // cy misurato dall'alto
    bool lookToo;    // trascinando sul pulsante si mira anche (fuoco, mira)
};

// Disposizione ad arco intorno al fuoco (in basso a destra), come negli sparatutto per telefono:
// mira a sinistra, ricarica / salto / accovacciati sopra, cambio arma sotto; coltello e granate piu' in la'.
const Button s_buttons[] = {
    { ICON_FIRE, 0, 2, 150, 165, 76, false, true },
    { ICON_ADS, 0, 1, 292, 150, 48, false, true },
    { ICON_RELOAD, XINPUT_GAMEPAD_X, 0, 252, 268, 40, false, false },
    { ICON_JUMP, XINPUT_GAMEPAD_A, 0, 150, 318, 44, false, false },
    { ICON_CROUCH, XINPUT_GAMEPAD_B, 0, 52, 268, 38, false, false },
    { ICON_SWITCH, XINPUT_GAMEPAD_Y, 0, 270, 52, 34, false, false },
    { ICON_KNIFE, XINPUT_GAMEPAD_RIGHT_THUMB, 0, 405, 240, 36, false, false },
    { ICON_NADE, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 418, 128, 36, false, false },
    { ICON_TACT, XINPUT_GAMEPAD_LEFT_SHOULDER, 0, 395, 42, 30, false, false },
    { ICON_PAUSE, XINPUT_GAMEPAD_START, 0, 52, 48, 28, true, false },
    { ICON_SCORE, XINPUT_GAMEPAD_BACK, 0, -52, 48, 28, true, false },
    // T5Jool: secondo fuoco a sinistra (sopra la levetta), per sparare con l'altra mano
    { ICON_FIRE, 0, 2, -120, 400, 52, false, true },
    // croce direzionale piccola in alto a sinistra, sotto il tabellone
    { ICON_UP, XINPUT_GAMEPAD_DPAD_UP, 0, -140, 112, 24, true, false },
    { ICON_DOWN, XINPUT_GAMEPAD_DPAD_DOWN, 0, -140, 204, 24, true, false },
    { ICON_LEFT, XINPUT_GAMEPAD_DPAD_LEFT, 0, -94, 158, 24, true, false },
    { ICON_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT, 0, -186, 158, 24, true, false },
};
const int BUTTON_COUNT = (int)(sizeof(s_buttons) / sizeof(s_buttons[0]));

// T5Jool: in the menus a tap works the item under the finger; these pad buttons cover what menus
// ask for on a controller (back, the X / Y prompts, tabs, start). Same style as the in-game controls:
// A / B / X / Y diamond bottom right, shoulders in the top corners.
struct MenuButton
{
    const char *label;
    WORD xbutton;
    float cx, cy, r; // as Button: from the right (cx >= 0) or the left (cx < 0), from the bottom or the top
    bool top;
};
const MenuButton s_menuButtons[] = {
    { "A", XINPUT_GAMEPAD_A, 120, 62, 34, false },
    { "B", XINPUT_GAMEPAD_B, 52, 130, 34, false },
    { "X", XINPUT_GAMEPAD_X, 188, 130, 34, false },
    { "Y", XINPUT_GAMEPAD_Y, 120, 198, 34, false },
    { "LB", XINPUT_GAMEPAD_LEFT_SHOULDER, -56, 44, 30, true },
    { "RB", XINPUT_GAMEPAD_RIGHT_SHOULDER, 56, 44, 30, true },
    { "LT", 0, -130, 44, 26, true },
    { "RT", 0, 130, 44, 26, true },
    { "START", XINPUT_GAMEPAD_START, 214, 44, 26, true },
    { "BACK", XINPUT_GAMEPAD_BACK, -214, 44, 26, true },
};
const int MENU_BUTTON_COUNT = (int)(sizeof(s_menuButtons) / sizeof(s_menuButtons[0]));
const int MENU_LT = 6, MENU_RT = 7;

struct Finger
{
    SDL_FingerID id;
    Role role;
    int button;
    float x0, y0; // inizio (levetta: centro)
    float x, y;   // posizione attuale in pixel
};

std::mutex s_lock;
Finger s_fingers[10];
int s_fingerCount;
bool s_inGame;     // aggiornato a ogni frame da BO1Touch_Draw
bool s_inMenu;     // T5Jool: a menu is up (not in game, not loading, no console): the menu buttons show
int s_menuHeld[10];     // fingers on menu buttons: button index, -1 = none
SDL_FingerID s_menuFinger[10];
int s_menuFingerCount;
bool s_menuMouseHeld;   // the mouse SDL made from a touch started on a menu button: dropped until released
bool s_hidden;     // controller vero in uso
float s_W = 1280.0f, s_H = 720.0f;
float s_lookRemX, s_lookRemY;

float U() { return s_H / 720.0f * (bo1_touch_scale ? bo1_touch_scale->current.value : 1.0f); }

// ---- disposizione personalizzata (editor: menu di pausa > Touch controls layout)
// Salvata in <fs_h>/t5jool/touch_layout.txt: righe "indice x y scala" (x, y = centro in frazioni dello schermo;
// indice BUTTON_COUNT = levetta). Senza riga si usa la posizione predefinita.
struct Placement
{
    bool moved;
    float nx, ny;  // centro in frazioni dello schermo
    float scale;   // dimensione rispetto alla predefinita
};
Placement s_place[32];
bool s_layoutLoaded;
bool s_editing;
int s_selected = -1;            // pulsante (o BUTTON_COUNT = levetta) selezionato nell'editor
SDL_FingerID s_dragFinger = -1; // dito che trascina
float s_dragOffX, s_dragOffY;
SDL_FingerID s_pinchFinger = -1; // secondo dito: pizzico per la dimensione
float s_pinchDist0, s_pinchScale0;
float s_dragX, s_dragY;          // ultima posizione del dito che trascina

void LayoutPath(char *out, size_t size)
{
    const char *home = Dvar_FindVar("fs_h") ? Dvar_GetString("fs_h") : nullptr;
    snprintf(out, size, "%s/t5jool/touch_layout.txt", home && *home ? home : ".");
}

// nel file la levetta resta all'indice 11 (le disposizioni salvate prima dei pulsanti nuovi restano valide);
// i pulsanti aggiunti dopo (secondo fuoco, croce) vengono dopo: 12, 13, ...
constexpr int kLegacyButtons = 11;
int FileIndex(int mem) { return mem == BUTTON_COUNT ? kLegacyButtons : mem < kLegacyButtons ? mem : mem + 1; }
int MemIndex(int file)
{
    if (file == kLegacyButtons)
        return BUTTON_COUNT;
    const int m = file < kLegacyButtons ? file : file - 1;
    return m >= 0 && m < BUTTON_COUNT ? m : -1;
}

void ResetLayout()
{
    for (auto &p : s_place)
        p = { false, 0.0f, 0.0f, 1.0f };
}

void LoadLayout()
{
    if (s_layoutLoaded || !Dvar_FindVar("fs_h"))
        return;
    s_layoutLoaded = true;
    ResetLayout();
    char path[512];
    LayoutPath(path, sizeof(path));
    if (FILE *f = fopen(path, "r"))
    {
        int i;
        float x, y, sc;
        while (fscanf(f, "%d %f %f %f", &i, &x, &y, &sc) == 4)
        {
            const int m = MemIndex(i);
            if (m >= 0 && m <= BUTTON_COUNT)
                s_place[m] = { x >= 0.0f, x, y, sc > 0.3f && sc < 3.0f ? sc : 1.0f };
        }
        fclose(f);
    }
}

void SaveLayout()
{
    char path[512];
    LayoutPath(path, sizeof(path));
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    if (char *slash = strrchr(dir, '/'))
    {
        *slash = 0;
        mkdir(dir, 0775);
    }
    if (FILE *f = fopen(path, "w"))
    {
        for (int i = 0; i <= BUTTON_COUNT; ++i)
            if (s_place[i].moved || s_place[i].scale != 1.0f)
                fprintf(f, "%d %.4f %.4f %.3f\n", FileIndex(i), s_place[i].moved ? s_place[i].nx : -1.0f, s_place[i].ny,
                        s_place[i].scale);
        fclose(f);
        Com_Printf(0, "T5Jool: disposizione touch salvata in %s\n", path);
    }
}

void ButtonCenter(int i, float *x, float *y, float *r)
{
    const Button &b = s_buttons[i];
    const float u = U();
    *x = b.cx >= 0.0f ? s_W - b.cx * u : -b.cx * u;
    *y = b.top ? b.cy * u : s_H - b.cy * u;
    *r = b.r * u * s_place[i].scale;
    if (s_place[i].moved)
    {
        *x = s_place[i].nx * s_W;
        *y = s_place[i].ny * s_H;
    }
}

// levetta: raggio e posizione del suggerimento (dove compare quando non la si tocca)
float StickRadius() { return 110.0f * U() * s_place[BUTTON_COUNT].scale; }
void StickHint(float *x, float *y)
{
    const Placement &p = s_place[BUTTON_COUNT];
    *x = p.moved ? p.nx * s_W : 210.0f * U();
    *y = p.moved ? p.ny * s_H : s_H - 200.0f * U();
}

int ButtonAt(float x, float y)
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        float cx, cy, r;
        ButtonCenter(i, &cx, &cy, &r);
        const float dx = x - cx, dy = y - cy, rr = r * 1.15f; // un po' di tolleranza
        if (dx * dx + dy * dy <= rr * rr)
            return i;
    }
    return -1;
}

void MenuButtonCenter(int i, float *x, float *y, float *r)
{
    const MenuButton &b = s_menuButtons[i];
    const float u = U();
    *x = b.cx >= 0.0f ? s_W - b.cx * u : -b.cx * u;
    *y = b.top ? b.cy * u : s_H - b.cy * u;
    *r = b.r * u;
}

int MenuButtonAt(float x, float y)
{
    for (int i = 0; i < MENU_BUTTON_COUNT; ++i)
    {
        float cx, cy, r;
        MenuButtonCenter(i, &cx, &cy, &r);
        const float dx = x - cx, dy = y - cy, rr = r * 1.2f;
        if (dx * dx + dy * dy <= rr * rr)
            return i;
    }
    return -1;
}

Finger *FindFinger(SDL_FingerID id)
{
    for (int i = 0; i < s_fingerCount; ++i)
        if (s_fingers[i].id == id)
            return &s_fingers[i];
    return nullptr;
}

void RemoveFinger(SDL_FingerID id)
{
    for (int i = 0; i < s_fingerCount; ++i)
        if (s_fingers[i].id == id)
        {
            s_fingers[i] = s_fingers[--s_fingerCount];
            return;
        }
}

bool Enabled()
{
    return bo1_touch && bo1_touch->current.enabled;
}

void Look(float dx, float dy)
{
    const float sens = bo1_touch_look ? bo1_touch_look->current.value : 1.0f;
    s_lookRemX += dx * sens;
    s_lookRemY += dy * sens;
    const int ix = (int)s_lookRemX, iy = (int)s_lookRemY;
    s_lookRemX -= (float)ix;
    s_lookRemY -= (float)iy;
    if (ix || iy)
        bo1_inject_mouse_delta(ix, iy);
}

// ------------------------------------------------------------------------------------- texture
// Disco sfumato, anello e icone dei pulsanti, disegnati qui con campi di distanza (bordi antialias, 128x128),
// bianchi su trasparente: il colore lo da' il disegno. Materiali copiati da "white" con la nostra immagine.
struct OwnMaterial
{
    GfxImage *image = nullptr;
    alignas(16) unsigned char mat[sizeof(Material)];
    MaterialTextureDef *tex = nullptr;
    int texCount = 0;
};
OwnMaterial s_disc, s_ring, s_glow, s_icons[ICON_COUNT];
bool s_texTried;

const int TEX = 128;

float Seg(float px, float py, float ax, float ay, float bx, float by)
{
    const float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    float h = (pax * bax + pay * bay) / (bax * bax + bay * bay);
    h = h < 0.0f ? 0.0f : h > 1.0f ? 1.0f : h;
    const float dx = pax - bax * h, dy = pay - bay * h;
    return sqrtf(dx * dx + dy * dy);
}

float Tri(float px, float py, float ax, float ay, float bx, float by, float cx, float cy)
{
    // distanza dal triangolo pieno (negativa dentro)
    const float d = fminf(Seg(px, py, ax, ay, bx, by), fminf(Seg(px, py, bx, by, cx, cy), Seg(px, py, cx, cy, ax, ay)));
    const float s1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    const float s2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
    const float s3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
    const bool inside = (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
    return inside ? -d : d;
}

float Box(float px, float py, float cx, float cy, float hx, float hy)
{
    const float dx = fabsf(px - cx) - hx, dy = fabsf(py - cy) - hy;
    const float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
    return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(dx, dy), 0.0f);
}

float Ring(float px, float py, float r, float w)
{
    return fabsf(sqrtf(px * px + py * py) - r) - w;
}

// distanza dall'icona nel punto (x, y) in [-1, 1], y verso il basso
float IconDist(int icon, float x, float y)
{
    const float W = 0.085f; // mezza larghezza delle linee
    float d = 1e9f;
    auto U = [&](float v) { d = fminf(d, v); };
    switch (icon)
    {
    case ICON_FIRE: // mirino
        U(Ring(x, y, 0.5f, 0.06f));
        U(Seg(x, y, 0, -0.85f, 0, -0.28f) - 0.06f);
        U(Seg(x, y, 0, 0.28f, 0, 0.85f) - 0.06f);
        U(Seg(x, y, -0.85f, 0, -0.28f, 0) - 0.06f);
        U(Seg(x, y, 0.28f, 0, 0.85f, 0) - 0.06f);
        U(sqrtf(x * x + y * y) - 0.1f);
        break;
    case ICON_ADS: // ottica
        U(Ring(x, y, 0.62f, 0.07f));
        U(Ring(x, y, 0.3f, 0.05f));
        U(Seg(x, y, -0.62f, 0, -0.38f, 0) - 0.045f);
        U(Seg(x, y, 0.38f, 0, 0.62f, 0) - 0.045f);
        U(Seg(x, y, 0, -0.62f, 0, -0.38f) - 0.045f);
        U(Seg(x, y, 0, 0.38f, 0, 0.62f) - 0.045f);
        break;
    case ICON_RELOAD: // freccia circolare
    {
        const float a = atan2f(y, x); // -pi..pi
        const float r = sqrtf(x * x + y * y);
        if (!(a > -2.2f && a < -0.9f)) // apertura in alto a destra
            U(fabsf(r - 0.5f) - W);
        U(Tri(x, y, 0.12f, -0.82f, 0.12f, -0.18f, 0.52f, -0.5f));
        break;
    }
    case ICON_JUMP: // doppia freccia in su
        U(Seg(x, y, -0.5f, -0.02f, 0, -0.5f) - W);
        U(Seg(x, y, 0, -0.5f, 0.5f, -0.02f) - W);
        U(Seg(x, y, -0.5f, 0.45f, 0, -0.03f) - W);
        U(Seg(x, y, 0, -0.03f, 0.5f, 0.45f) - W);
        break;
    case ICON_CROUCH: // doppia freccia in giu'
        U(Seg(x, y, -0.5f, 0.02f, 0, 0.5f) - W);
        U(Seg(x, y, 0, 0.5f, 0.5f, 0.02f) - W);
        U(Seg(x, y, -0.5f, -0.45f, 0, 0.03f) - W);
        U(Seg(x, y, 0, 0.03f, 0.5f, -0.45f) - W);
        break;
    case ICON_SWITCH: // due frecce opposte
        U(Seg(x, y, -0.6f, -0.25f, 0.3f, -0.25f) - W);
        U(Tri(x, y, 0.25f, -0.52f, 0.25f, 0.02f, 0.65f, -0.25f));
        U(Seg(x, y, -0.3f, 0.25f, 0.6f, 0.25f) - W);
        U(Tri(x, y, -0.25f, -0.02f, -0.25f, 0.52f, -0.65f, 0.25f));
        break;
    case ICON_KNIFE: // coltello in diagonale
        U(Tri(x, y, 0.62f, -0.62f, -0.22f, -0.04f, 0.04f, 0.22f));
        U(Seg(x, y, -0.32f, -0.02f, -0.02f, 0.28f) - 0.06f);
        U(Seg(x, y, -0.15f, 0.12f, -0.55f, 0.52f) - 0.1f);
        break;
    case ICON_NADE: // granata
        U(sqrtf(x * x + (y - 0.15f) * (y - 0.15f)) - 0.45f);
        U(Box(x, y, -0.05f, -0.4f, 0.16f, 0.1f));
        U(Seg(x, y, 0.1f, -0.42f, 0.45f, -0.2f) - 0.06f);
        U(Ring(x - 0.45f, y + 0.55f, 0.13f, 0.045f));
        break;
    case ICON_TACT: // fulmine (tattica)
        U(Tri(x, y, 0.2f, -0.75f, -0.4f, 0.12f, 0.08f, 0.12f));
        U(Tri(x, y, -0.08f, -0.12f, 0.4f, -0.12f, -0.2f, 0.75f));
        break;
    case ICON_UP: // freccia piena
    case ICON_DOWN:
    case ICON_LEFT:
    case ICON_RIGHT:
    {
        // triangolo verso l'alto, ruotato per le altre direzioni
        float ax = x, ay = y;
        if (icon == ICON_DOWN) ay = -y;
        if (icon == ICON_LEFT) { ax = y; ay = x; }
        if (icon == ICON_RIGHT) { ax = y; ay = -x; }
        U(Seg(ax, ay, -0.45f, 0.25f, 0, -0.35f) - W);
        U(Seg(ax, ay, 0, -0.35f, 0.45f, 0.25f) - W);
        U(Seg(ax, ay, -0.45f, 0.25f, 0.45f, 0.25f) - W);
        break;
    }
    case ICON_PAUSE:
        U(Box(x, y, -0.25f, 0, 0.11f, 0.42f));
        U(Box(x, y, 0.25f, 0, 0.11f, 0.42f));
        break;
    case ICON_SCORE: // elenco
        U(Seg(x, y, -0.45f, -0.35f, 0.45f, -0.35f) - 0.07f);
        U(Seg(x, y, -0.45f, 0, 0.45f, 0) - 0.07f);
        U(Seg(x, y, -0.45f, 0.35f, 0.45f, 0.35f) - 0.07f);
        break;
    default:
        break;
    }
    return d;
}

enum TexKind
{
    TEX_DISC, // disco pieno, piu' chiaro al bordo
    TEX_RING, // anello sottile
    TEX_GLOW, // alone morbido (pulsante premuto)
    TEX_ICON,
};

GfxImage *MakeImage(TexKind kind, int icon, const char *name)
{
    IDirect3DTexture9 *tex = nullptr;
    if (!dx.device || FAILED(dx.device->CreateTexture(TEX, TEX, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)) || !tex)
        return nullptr;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, 0)))
    {
        const float px = 2.0f / TEX; // un pixel in coordinate [-1, 1]
        for (int yy = 0; yy < TEX; ++yy)
        {
            uint32_t *row = (uint32_t *)((char *)lr.pBits + (size_t)yy * lr.Pitch);
            for (int xx = 0; xx < TEX; ++xx)
            {
                const float x = ((float)xx + 0.5f) * px - 1.0f, y = ((float)yy + 0.5f) * px - 1.0f;
                const float r = sqrtf(x * x + y * y);
                float a = 0.0f;
                switch (kind)
                {
                case TEX_DISC:
                {
                    const float edge = fminf(fmaxf((0.97f - r) / px + 0.5f, 0.0f), 1.0f);
                    a = edge * (0.75f + 0.25f * r * r); // piu' scuro al centro
                    break;
                }
                case TEX_RING:
                    a = fminf(fmaxf(0.5f - (fabsf(r - 0.93f) - 0.035f) / px, 0.0f), 1.0f);
                    break;
                case TEX_GLOW:
                    a = r < 1.0f ? (1.0f - r) * (1.0f - r) : 0.0f;
                    break;
                case TEX_ICON:
                    a = fminf(fmaxf(0.5f - IconDist(icon, x * 1.6f, y * 1.6f) / (px * 1.6f), 0.0f), 1.0f);
                    break;
                }
                row[xx] = ((uint32_t)(a * 255.0f + 0.5f) << 24) | 0x00FFFFFFu;
            }
        }
        tex->UnlockRect(0);
    }
    GfxImage *image = (GfxImage *)calloc(1, sizeof(GfxImage));
    image->texture.map = tex;
    image->mapType = 3;
    image->width = TEX;
    image->height = TEX;
    image->depth = 1;
    image->levelCount = 1;
    image->name = name;
    return image;
}

Material *Mat(OwnMaterial &m, const char *name)
{
    if (!m.image)
        return nullptr;
    Material *white = Material_RegisterHandle((char *)"white", 0);
    if (!white || !white->textureTable || white->textureCount <= 0)
        return nullptr;
    if (m.texCount != white->textureCount)
    {
        free(m.tex);
        m.tex = (MaterialTextureDef *)calloc(white->textureCount, sizeof(MaterialTextureDef));
        m.texCount = m.tex ? white->textureCount : 0;
        if (!m.tex)
            return nullptr;
    }
    memcpy(m.tex, white->textureTable, sizeof(MaterialTextureDef) * white->textureCount);
    m.tex[0].u.image = m.image;
    Material *mat = (Material *)m.mat;
    memcpy(mat, white, sizeof(Material));
    mat->info.name = name;
    mat->textureTable = (decltype(mat->textureTable))m.tex;
    return mat;
}

void Circle(Material *m, float cx, float cy, float r, const float *color)
{
    if (m)
        R_AddCmdDrawStretchPic(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, color, m);
}

// ---- editor della disposizione
enum ToolButton { TOOL_SMALLER, TOOL_BIGGER, TOOL_RESET, TOOL_DONE, TOOL_COUNT };
const char *const s_toolLabels[TOOL_COUNT] = { "-", "+", "Reset", "Done" };

void ToolRect(int t, float *x, float *y, float *w, float *h)
{
    const float u = s_H / 720.0f;
    const float widths[TOOL_COUNT] = { 70.0f, 70.0f, 130.0f, 130.0f };
    float total = 0.0f;
    for (float wd : widths)
        total += wd * u + 14.0f * u;
    float cx = (s_W - total) * 0.5f;
    for (int i = 0; i < t; ++i)
        cx += widths[i] * u + 14.0f * u;
    *x = cx;
    *y = 20.0f * u;
    *w = widths[t] * u;
    *h = 56.0f * u;
}

int ToolAt(float x, float y)
{
    for (int t = 0; t < TOOL_COUNT; ++t)
    {
        float tx, ty, tw, th;
        ToolRect(t, &tx, &ty, &tw, &th);
        if (x >= tx && x < tx + tw && y >= ty && y < ty + th)
            return t;
    }
    return -1;
}

// pulsante o levetta sotto il dito (BUTTON_COUNT = levetta), -1 = niente
int EditItemAt(float x, float y)
{
    const int b = ButtonAt(x, y);
    if (b >= 0)
        return b;
    float hx, hy;
    StickHint(&hx, &hy);
    const float r = StickRadius();
    if ((x - hx) * (x - hx) + (y - hy) * (y - hy) <= r * r)
        return BUTTON_COUNT;
    return -1;
}

void ItemCenter(int item, float *x, float *y, float *r)
{
    if (item == BUTTON_COUNT)
    {
        StickHint(x, y);
        *r = StickRadius();
    }
    else
        ButtonCenter(item, x, y, r);
}

void Rescale(int item, float factor)
{
    if (item < 0)
        return;
    float &sc = s_place[item].scale;
    sc *= factor;
    if (sc < 0.5f)
        sc = 0.5f;
    if (sc > 2.5f)
        sc = 2.5f;
}

void EditEvent(const SDL_Event *e, float x, float y)
{
    const SDL_FingerID id = e->tfinger.fingerId;
    if (e->type == SDL_FINGERDOWN)
    {
        if (s_dragFinger != -1 && s_selected >= 0 && s_pinchFinger == -1)
        {
            // secondo dito: pizzico
            s_pinchFinger = id;
            s_pinchDist0 = sqrtf((x - s_dragX) * (x - s_dragX) + (y - s_dragY) * (y - s_dragY));
            s_pinchScale0 = s_place[s_selected].scale;
            return;
        }
        const int tool = ToolAt(x, y);
        if (tool >= 0)
        {
            if (tool == TOOL_SMALLER)
                Rescale(s_selected, 1.0f / 1.1f);
            else if (tool == TOOL_BIGGER)
                Rescale(s_selected, 1.1f);
            else if (tool == TOOL_RESET)
            {
                ResetLayout();
                s_selected = -1;
            }
            else
            {
                SaveLayout();
                s_editing = false;
                s_selected = -1;
            }
            return;
        }
        const int item = EditItemAt(x, y);
        s_selected = item;
        if (item >= 0)
        {
            float cx, cy, r;
            ItemCenter(item, &cx, &cy, &r);
            s_dragFinger = id;
            s_dragOffX = cx - x;
            s_dragOffY = cy - y;
            s_dragX = x;
            s_dragY = y;
        }
        return;
    }
    if (e->type == SDL_FINGERUP)
    {
        if (id == s_dragFinger)
            s_dragFinger = -1;
        if (id == s_pinchFinger || s_dragFinger == -1)
            s_pinchFinger = -1;
        return;
    }
    // movimento
    if (id == s_pinchFinger && s_selected >= 0 && s_pinchDist0 > 1.0f)
    {
        const float d = sqrtf((x - s_dragX) * (x - s_dragX) + (y - s_dragY) * (y - s_dragY));
        s_place[s_selected].scale = s_pinchScale0;
        Rescale(s_selected, d / s_pinchDist0);
        return;
    }
    if (id == s_dragFinger && s_selected >= 0)
    {
        s_dragX = x;
        s_dragY = y;
        if (s_pinchFinger != -1)
            return; // durante il pizzico il pulsante resta fermo
        float nx = (x + s_dragOffX) / s_W, ny = (y + s_dragOffY) / s_H;
        nx = nx < 0.02f ? 0.02f : nx > 0.98f ? 0.98f : nx;
        ny = ny < 0.02f ? 0.02f : ny > 0.98f ? 0.98f : ny;
        s_place[s_selected].moved = true;
        s_place[s_selected].nx = nx;
        s_place[s_selected].ny = ny;
    }
}

} // namespace

// ------------------------------------------------------------------------------------------- API
void BO1Touch_Init()
{
    if (bo1_touch)
        return;
#ifdef __ANDROID__
    const bool def = true;
#else
    const bool def = false;
#endif
    bo1_touch = _Dvar_RegisterBool("bo1_touch", def, 1u, "T5Jool: comandi touch in partita");
    bo1_touch_opacity = _Dvar_RegisterFloat("bo1_touch_opacity", 0.45f, 0.1f, 1.0f, 1u, "T5Jool: opacita' dei comandi touch");
    bo1_touch_scale = _Dvar_RegisterFloat("bo1_touch_scale", 1.0f, 0.6f, 1.6f, 1u, "T5Jool: dimensione dei comandi touch");
    bo1_touch_look = _Dvar_RegisterFloat("bo1_touch_look", 1.0f, 0.2f, 4.0f, 1u, "T5Jool: sensibilita' della mira touch");
    bo1_touch_autohide = _Dvar_RegisterBool("bo1_touch_autohide", true, 1u, "T5Jool: nasconde i comandi touch quando si usa un controller");
}

bool BO1Touch_HandleEvent(const SDL_Event *e)
{
    if (!bo1_touch)
        BO1Touch_Init();
    if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERMOTION && e->type != SDL_FINGERUP)
        return false;
    std::lock_guard<std::mutex> l(s_lock);
    const float x = e->tfinger.x * s_W, y = e->tfinger.y * s_H;
    if (s_editing)
    {
        EditEvent(e, x, y);
        return true;
    }
    if (!s_inGame && s_inMenu && Enabled())
    {
        // T5Jool: menus. A finger on a menu button presses it; elsewhere the touch stays a mouse (tap = click)
        s_fingerCount = 0;
        int slot = -1;
        for (int i = 0; i < s_menuFingerCount; ++i)
            if (s_menuFinger[i] == e->tfinger.fingerId)
                slot = i;
        if (e->type == SDL_FINGERDOWN)
        {
            const bool wasHidden = s_hidden;
            s_hidden = false;
            const int b = wasHidden ? -1 : MenuButtonAt(x, y);
            if (b < 0 || s_menuFingerCount >= 10)
                return false;
            s_menuFinger[s_menuFingerCount] = e->tfinger.fingerId;
            s_menuHeld[s_menuFingerCount++] = b;
            return true;
        }
        if (slot < 0)
            return false;
        if (e->type == SDL_FINGERUP)
        {
            --s_menuFingerCount;
            s_menuFinger[slot] = s_menuFinger[s_menuFingerCount];
            s_menuHeld[slot] = s_menuHeld[s_menuFingerCount];
        }
        return true;
    }
    s_menuFingerCount = 0;
    if (e->type == SDL_FINGERUP)
    {
        RemoveFinger(e->tfinger.fingerId);
        return s_inGame && Enabled();
    }
    if (!s_inGame || !Enabled())
    {
        s_fingerCount = 0;
        return false;
    }
    s_hidden = false;
    if (e->type == SDL_FINGERDOWN)
    {
        if (s_fingerCount >= 10)
            return true;
        Finger f{ e->tfinger.fingerId, ROLE_NONE, -1, x, y, x, y };
        const int b = ButtonAt(x, y);
        if (b >= 0)
        {
            f.role = ROLE_BUTTON;
            f.button = b;
        }
        else if (x < s_W * 0.45f && y > s_H * 0.25f)
        {
            bool stickTaken = false;
            for (int i = 0; i < s_fingerCount; ++i)
                stickTaken |= s_fingers[i].role == ROLE_STICK;
            f.role = stickTaken ? ROLE_LOOK : ROLE_STICK;
        }
        else
            f.role = ROLE_LOOK;
        s_fingers[s_fingerCount++] = f;
        return true;
    }
    // movimento
    Finger *f = FindFinger(e->tfinger.fingerId);
    if (!f)
        return true;
    const float dx = x - f->x, dy = y - f->y;
    f->x = x;
    f->y = y;
    if (f->role == ROLE_LOOK || (f->role == ROLE_BUTTON && s_buttons[f->button].lookToo))
        Look(dx, dy);
    return true;
}

bool BO1Touch_DropTouchMouse(const SDL_Event *e)
{
    if (s_editing || (s_inGame && Enabled()))
        return true;
    // T5Jool: menus. SDL sends the mouse it makes from a touch before the touch itself, so a press is
    // checked by where it lands: on a menu button it is not also a click on the menu behind it.
    if (e->type == SDL_MOUSEBUTTONDOWN)
    {
        s_menuMouseHeld = false;
        if (s_inMenu && Enabled() && !s_hidden)
        {
            int ww = 0, wh = 0;
            if (SDL_Window *w = SDL_GetWindowFromID(e->button.windowID))
                SDL_GetWindowSize(w, &ww, &wh);
            if (ww > 0 && wh > 0)
            {
                std::lock_guard<std::mutex> l(s_lock);
                s_menuMouseHeld = MenuButtonAt(e->button.x * s_W / ww, e->button.y * s_H / wh) >= 0;
            }
        }
        return s_menuMouseHeld;
    }
    if (e->type == SDL_MOUSEBUTTONUP)
    {
        const bool held = s_menuMouseHeld;
        s_menuMouseHeld = false;
        return held;
    }
    return s_menuMouseHeld;
}

void BO1Touch_StartEdit()
{
    if (!bo1_touch)
        BO1Touch_Init();
    std::lock_guard<std::mutex> l(s_lock);
    LoadLayout();
    s_editing = true;
    s_selected = -1;
    s_dragFinger = s_pinchFinger = -1;
    s_fingerCount = 0;
}

bool BO1Touch_IsEditing()
{
    return s_editing;
}

void BO1Touch_StopEdit()
{
    std::lock_guard<std::mutex> l(s_lock);
    if (s_editing)
        SaveLayout();
    s_editing = false;
    s_selected = -1;
}

bool BO1Touch_GetPad(XINPUT_GAMEPAD *pad)
{
    memset(pad, 0, sizeof(*pad));
    if (Enabled() && !s_inGame && s_inMenu && !s_hidden && !s_editing)
    {
        // T5Jool: the menu buttons
        std::lock_guard<std::mutex> l(s_lock);
        for (int i = 0; i < s_menuFingerCount; ++i)
        {
            const int b = s_menuHeld[i];
            pad->wButtons |= s_menuButtons[b].xbutton;
            if (b == MENU_LT)
                pad->bLeftTrigger = 255;
            if (b == MENU_RT)
                pad->bRightTrigger = 255;
        }
        return true;
    }
    if (!Enabled() || !s_inGame || s_hidden)
        return false;
    std::lock_guard<std::mutex> l(s_lock);
    const float stickR = StickRadius();
    for (int i = 0; i < s_fingerCount; ++i)
    {
        const Finger &f = s_fingers[i];
        if (f.role == ROLE_BUTTON)
        {
            const Button &b = s_buttons[f.button];
            pad->wButtons |= b.xbutton;
            if (b.trigger == 1)
                pad->bLeftTrigger = 255;
            if (b.trigger == 2)
                pad->bRightTrigger = 255;
        }
        else if (f.role == ROLE_STICK)
        {
            float vx = (f.x - f.x0) / stickR, vy = (f.y - f.y0) / stickR;
            const float len = sqrtf(vx * vx + vy * vy);
            // oltre l'anello = scatto (pressione della levetta sinistra)
            if (len > 1.35f && vy < 0.0f)
                pad->wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
            if (len > 1.0f)
            {
                vx /= len;
                vy /= len;
            }
            pad->sThumbLX = (SHORT)(vx * 32767.0f);
            pad->sThumbLY = (SHORT)(-vy * 32767.0f);
        }
    }
    return true;
}

void BO1Touch_PhysicalPadUsed()
{
    if (!bo1_touch_autohide || bo1_touch_autohide->current.enabled)
        s_hidden = true;
}

namespace
{
// T5Jool: the menu buttons, drawn like the in-game ones with a letter in place of the icon
void DrawMenuButtons()
{
    Material *disc = Mat(s_disc, "t5jool_touch_disc"), *ring = Mat(s_ring, "t5jool_touch_ring"), *glow = Mat(s_glow, "t5jool_touch_glow");
    static Font_s *font;
    if (!font)
        font = R_RegisterFont("fonts/normalFont", 0);
    const float alpha = bo1_touch_opacity->current.value;
    const float clampA = alpha * 1.5f > 1.0f ? 1.0f : alpha * 1.5f;
    const float fill[4] = { 0.04f, 0.04f, 0.05f, alpha * 0.7f };
    const float fillOn[4] = { 0.87f, 0.2f, 0.16f, clampA * 0.9f };
    const float glowOn[4] = { 1.0f, 0.3f, 0.2f, clampA * 0.6f };
    const float edge[4] = { 1.0f, 1.0f, 1.0f, alpha * 0.55f };
    const float text[4] = { 1.0f, 1.0f, 1.0f, clampA };
    std::lock_guard<std::mutex> l(s_lock);
    for (int i = 0; i < MENU_BUTTON_COUNT; ++i)
    {
        bool pressed = false;
        for (int f = 0; f < s_menuFingerCount; ++f)
            pressed |= s_menuHeld[f] == i;
        float cx, cy, r;
        MenuButtonCenter(i, &cx, &cy, &r);
        const float rr = pressed ? r * 0.93f : r;
        if (pressed)
            Circle(glow, cx, cy, r * 1.45f, glowOn);
        Circle(disc, cx, cy, rr, pressed ? fillOn : fill);
        Circle(ring, cx, cy, rr, edge);
        if (font)
        {
            const char *label = s_menuButtons[i].label;
            const int fh = R_TextHeight(font);
            const float px = (strlen(label) > 2 ? 0.5f : strlen(label) > 1 ? 0.75f : 1.0f) * rr, sc = fh > 0 ? px / (float)fh : 1.0f;
            const float tw = (float)R_TextWidth(label, 0x7FFFFFFF, font) * sc;
            R_AddCmdDrawText((char *)label, 0x7FFFFFFF, font, cx - tw * 0.5f, cy + px * 0.38f, sc, sc, 0.0f, text, 0);
        }
    }
}
} // namespace

void BO1Touch_Draw(int localClientNum)
{
    if (!bo1_touch)
        BO1Touch_Init();
    s_W = (float)cls.vidConfig.displayWidth;
    s_H = (float)cls.vidConfig.displayHeight;
    // T5Jool: nel multiplayer in partita restano accesi altri bit dei catcher (non menu ne' console): contano solo
    // console (1) e menu (0x10), altrimenti i comandi touch non comparivano mai nel multiplayer
    const bool zombies = Dvar_GetBool("zombiemode");
    const bool inGame = CL_GetLocalClientConnectionState(localClientNum) == CA_ACTIVE && !Com_IsMenuLevel(nullptr) /* the SP front end is the "frontend" map */
                        && !Key_IsCatcherActive(localClientNum, zombies ? -1 : 0x11);
    {
        const int state = CL_GetLocalClientConnectionState(localClientNum);
        const bool menu = !inGame && !Key_IsCatcherActive(localClientNum, 1) && cls.uiStarted
                          && (state == CA_DISCONNECTED || state == CA_ACTIVE);
        if (menu != s_inMenu)
        {
            std::lock_guard<std::mutex> l(s_lock);
            s_inMenu = menu;
            s_menuFingerCount = 0;
        }
    }
    if (inGame != s_inGame)
    {
        Com_Printf(0, "T5Jool touch: %s (catcher 0x%x)\n", inGame ? "in partita" : "spenti",
                   (unsigned)CL_GetLocalClientUIGlobals(localClientNum)->keyCatchers);
        std::lock_guard<std::mutex> l(s_lock);
        s_inGame = inGame;
        s_fingerCount = 0;
    }
    if (!s_layoutLoaded)
    {
        std::lock_guard<std::mutex> l(s_lock);
        LoadLayout();
    }
    const bool menuButtons = !s_inGame && !s_editing && s_inMenu && Enabled() && !s_hidden && s_W > 0.0f && s_H > 0.0f;
    if (!menuButtons && ((!s_inGame && !s_editing) || !Enabled() || (s_hidden && !s_editing) || s_W <= 0.0f || s_H <= 0.0f))
        return;
    if (!s_texTried && dx.device)
    {
        s_texTried = true;
        s_disc.image = MakeImage(TEX_DISC, 0, "t5jool_touch_disc");
        s_ring.image = MakeImage(TEX_RING, 0, "t5jool_touch_ring");
        s_glow.image = MakeImage(TEX_GLOW, 0, "t5jool_touch_glow");
        static const char *names[ICON_COUNT] = { "t5jool_touch_fire", "t5jool_touch_ads", "t5jool_touch_reload", "t5jool_touch_jump",
                                                 "t5jool_touch_crouch", "t5jool_touch_switch", "t5jool_touch_knife", "t5jool_touch_nade",
                                                 "t5jool_touch_tact", "t5jool_touch_pause", "t5jool_touch_score",
                                                 "t5jool_touch_up", "t5jool_touch_down", "t5jool_touch_left", "t5jool_touch_right" };
        for (int i = 0; i < ICON_COUNT; ++i)
            s_icons[i].image = MakeImage(TEX_ICON, i, names[i]);
    }
    if (menuButtons)
    {
        DrawMenuButtons();
        return;
    }
    Material *disc = Mat(s_disc, "t5jool_touch_disc"), *ring = Mat(s_ring, "t5jool_touch_ring"), *glow = Mat(s_glow, "t5jool_touch_glow");
    const float alpha = bo1_touch_opacity->current.value;
    const float clampA = alpha * 1.5f > 1.0f ? 1.0f : alpha * 1.5f;
    const float fill[4] = { 0.04f, 0.04f, 0.05f, alpha * 0.7f };
    const float fillOn[4] = { 0.87f, 0.2f, 0.16f, clampA * 0.9f };
    const float glowOn[4] = { 1.0f, 0.3f, 0.2f, clampA * 0.6f };
    const float edge[4] = { 1.0f, 1.0f, 1.0f, alpha * 0.55f };
    const float edgeFire[4] = { 1.0f, 0.35f, 0.25f, clampA * 0.8f };
    const float icon[4] = { 1.0f, 1.0f, 1.0f, clampA };
    const float u = U();

    std::lock_guard<std::mutex> l(s_lock);
    if (s_editing)
    {
        // sfondo scuro, tutti i comandi, quello selezionato evidenziato, barra in alto
        Material *white = Material_RegisterHandle((char *)"white", 0);
        const float shade[4] = { 0.0f, 0.0f, 0.0f, 0.55f };
        if (white)
            R_AddCmdDrawStretchPic(0.0f, 0.0f, s_W, s_H, 0.0f, 0.0f, 1.0f, 1.0f, shade, white);
        const float efill[4] = { 0.04f, 0.04f, 0.05f, 0.8f };
        const float eedge[4] = { 1.0f, 1.0f, 1.0f, 0.8f };
        const float eicon[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        for (int i = 0; i <= BUTTON_COUNT; ++i)
        {
            float cx, cy, r;
            ItemCenter(i, &cx, &cy, &r);
            if (i == s_selected)
                Circle(glow, cx, cy, r * 1.45f, glowOn);
            Circle(disc, cx, cy, r, i == s_selected ? fillOn : efill);
            Circle(ring, cx, cy, r, i == s_selected ? edgeFire : eedge);
            if (i < BUTTON_COUNT)
                Circle(Mat(s_icons[s_buttons[i].icon], "t5jool_touch_icon"), cx, cy, r, eicon);
            else
                Circle(disc, cx, cy, r * 0.42f, eedge); // levetta
        }
        static Font_s *font;
        if (!font)
            font = R_RegisterFont("fonts/normalFont", 0);
        const float uu = s_H / 720.0f;
        for (int t = 0; t < TOOL_COUNT; ++t)
        {
            float tx, ty, tw, th;
            ToolRect(t, &tx, &ty, &tw, &th);
            const bool off = (t == TOOL_SMALLER || t == TOOL_BIGGER) && s_selected < 0;
            const float bg[4] = { t == TOOL_DONE ? 0.87f : 0.1f, t == TOOL_DONE ? 0.2f : 0.1f, t == TOOL_DONE ? 0.16f : 0.11f, off ? 0.4f : 0.92f };
            if (white)
                R_AddCmdDrawStretchPic(tx, ty, tw, th, 0.0f, 0.0f, 1.0f, 1.0f, bg, white);
            if (font)
            {
                const int fh = R_TextHeight(font);
                const float px = 30.0f * uu, sc = fh > 0 ? px / (float)fh : 1.0f;
                const float textW = (float)R_TextWidth(s_toolLabels[t], 0x7FFFFFFF, font) * sc;
                const float tc[4] = { 1.0f, 1.0f, 1.0f, off ? 0.4f : 1.0f };
                R_AddCmdDrawText((char *)s_toolLabels[t], 0x7FFFFFFF, font, tx + (tw - textW) * 0.5f, ty + th * 0.5f + px * 0.38f, sc, sc,
                                 0.0f, tc, 0);
            }
        }
        if (font)
        {
            const char *hint = "Drag to move  -  pinch or -/+ to resize  -  Done to save";
            const int fh = R_TextHeight(font);
            const float px = 22.0f * uu, sc = fh > 0 ? px / (float)fh : 1.0f;
            const float textW = (float)R_TextWidth(hint, 0x7FFFFFFF, font) * sc;
            const float tc[4] = { 1.0f, 1.0f, 1.0f, 0.85f };
            R_AddCmdDrawText((char *)hint, 0x7FFFFFFF, font, (s_W - textW) * 0.5f, 110.0f * uu, sc, sc, 0.0f, tc, 0);
        }
        return;
    }
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        bool pressed = false;
        for (int f = 0; f < s_fingerCount; ++f)
            pressed |= s_fingers[f].role == ROLE_BUTTON && s_fingers[f].button == i;
        float cx, cy, r;
        ButtonCenter(i, &cx, &cy, &r);
        const float rr = pressed ? r * 0.93f : r; // si "schiaccia" un po'
        if (pressed)
            Circle(glow, cx, cy, r * 1.45f, glowOn);
        Circle(disc, cx, cy, rr, pressed ? fillOn : fill);
        Circle(ring, cx, cy, rr, s_buttons[i].icon == ICON_FIRE && !pressed ? edgeFire : edge);
        Material *im = Mat(s_icons[s_buttons[i].icon], "t5jool_touch_icon");
        Circle(im, cx, cy, rr, icon);
    }
    // levetta: base dove la si tocca, oppure un suggerimento fisso in basso a sinistra
    const float stickR = StickRadius();
    bool stick = false;
    for (int f = 0; f < s_fingerCount; ++f)
    {
        const Finger &fg = s_fingers[f];
        if (fg.role != ROLE_STICK)
            continue;
        stick = true;
        float vx = fg.x - fg.x0, vy = fg.y - fg.y0;
        const float len = sqrtf(vx * vx + vy * vy);
        const bool sprint = len > stickR * 1.35f && vy < 0.0f;
        Circle(disc, fg.x0, fg.y0, stickR, fill);
        Circle(ring, fg.x0, fg.y0, stickR, sprint ? edgeFire : edge);
        if (len > stickR)
        {
            vx *= stickR / len;
            vy *= stickR / len;
        }
        const float knobOn[4] = { 1.0f, 1.0f, 1.0f, clampA * 0.85f };
        if (sprint)
            Circle(glow, fg.x0 + vx, fg.y0 + vy, stickR * 0.8f, glowOn);
        Circle(disc, fg.x0 + vx, fg.y0 + vy, stickR * 0.42f, sprint ? fillOn : knobOn);
        Circle(ring, fg.x0 + vx, fg.y0 + vy, stickR * 0.42f, edge);
    }
    if (!stick)
    {
        float hx, hy;
        StickHint(&hx, &hy);
        const float faintFill[4] = { 0.04f, 0.04f, 0.05f, alpha * 0.35f };
        const float faint[4] = { 1.0f, 1.0f, 1.0f, alpha * 0.35f };
        Circle(disc, hx, hy, stickR, faintFill);
        Circle(ring, hx, hy, stickR, faint);
        Circle(disc, hx, hy, stickR * 0.42f, faint);
    }
}

#else // _WIN32: niente touch
void BO1Touch_Init() {}
void BO1Touch_Draw(int) {}
void BO1Touch_StartEdit() {}
bool BO1Touch_IsEditing() { return false; }
void BO1Touch_StopEdit() {}
#endif
