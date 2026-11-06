// Gravity-well lander/shooter. Compiled to wasm32 with no libc.
typedef unsigned int u32;
#define EXPORT(n) __attribute__((export_name(n)))
#define PI 3.14159265f
#define SEGS 64
#define SEG 56.0f
#define WW (SEGS * SEG)
#define MAXL 3000
#define NPB 8
#define NEB 48
#define NBUNK 12
#define NTANK 6
#define NPART 500
#define NSTAR 150

enum { ST_TITLE, ST_PLAY, ST_DYING, ST_CLEAR, ST_OVER };
enum { C_SHIP, C_TERR, C_BUNK, C_FUEL, C_PBUL, C_EBUL, C_BOOM, C_SHLD, C_STAR, C_FLAME, C_BEAM };
enum { IN_L = 1, IN_R = 2, IN_T = 4, IN_F = 8, IN_S = 16 };
enum { EV_FIRE = 1, EV_EFIRE = 2, EV_BOOM = 4, EV_DIE = 8, EV_FUEL = 16, EV_CLEAR = 32, EV_THRUST = 64, EV_SHIELD = 128, EV_LIFE = 256 };

// ---------- tiny math ----------
static float absf(float x) { return x < 0 ? -x : x; }
static float sqrtf_(float x) { return __builtin_sqrtf(x); }
static float sin_(float x) {
  while (x > PI) x -= 2 * PI;
  while (x < -PI) x += 2 * PI;
  if (x > PI / 2) x = PI - x; else if (x < -PI / 2) x = -PI - x;
  float x2 = x * x;
  return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72))));
}
static float cos_(float x) { return sin_(x + PI / 2); }

static u32 rng_s = 12345;
static u32 rnd(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s >> 4; }
static float frand(void) { return (float)(rnd() & 0xFFFFFF) * (1.0f / 16777216.0f); }
static float frange(float a, float b) { return a + (b - a) * frand(); }

static float wrapx(float x) { while (x < 0) x += WW; while (x >= WW) x -= WW; return x; }
static float wrapd(float d) { while (d > WW * 0.5f) d -= WW; while (d < -WW * 0.5f) d += WW; return d; }

// ---------- world ----------
static float th[SEGS];
static float ground(float x) {
  x = wrapx(x);
  int i = (int)(x / SEG);
  if (i >= SEGS) i = SEGS - 1;
  float t = (x - i * SEG) / SEG;
  return th[i] + (th[(i + 1) % SEGS] - th[i]) * t;
}

typedef struct { float x, y, vx, vy, life; } Bul;
typedef struct { float x, y, timer, ax, ay; int alive; } Bunker;
typedef struct { float x, y; int alive; } Tank;
typedef struct { float x, y, vx, vy, life, max; int col; } Part;

static Bul pb[NPB], eb[NEB];
static Bunker bk[NBUNK]; static int nbk;
static Tank tk[NTANK]; static int ntk;
static Part pt[NPART]; static int ptn;
static float starx[NSTAR], stary[NSTAR];

static float sx, sy, svx, svy, sa, sinv;
static int thrusting, shielding;
static int state, level, lives, score, events, prevIn, nextLife;
static float fuel, timer, firecd, grav;

// ---------- render buffer ----------
static float L[MAXL * 5];
static int nl, nterr;
static float halfW = 400, halfH = 300, camx, camy;

static void add_rel(float x1, float y1, float x2, float y2, int c) {
  if (nl >= MAXL) return;
  float *p = &L[nl * 5];
  p[0] = x1; p[1] = y1; p[2] = x2; p[3] = y2; p[4] = (float)c;
  nl++;
}
static void add_w(float x1, float y1, float x2, float y2, int c) {
  float dx = wrapd(x1 - camx);
  if (dx < -halfW - 80 || dx > halfW + 80) return;
  float dy = y1 - camy;
  if (dy < -halfH - 80 || dy > halfH + 80) return;
  add_rel(dx, dy, dx + (x2 - x1), dy + (y2 - y1), c);
}

// ---------- effects ----------
static void explode(float x, float y, int n, int col, float spd) {
  for (int i = 0; i < n; i++) {
    Part *p = &pt[ptn]; ptn = (ptn + 1) % NPART;
    float a = frange(0, 2 * PI), s = frange(0.2f, 1.0f) * spd;
    p->x = x; p->y = y; p->vx = cos_(a) * s; p->vy = sin_(a) * s;
    p->max = p->life = frange(0.4f, 1.3f); p->col = col;
  }
}

// ---------- level setup ----------
static void spawn_ship(void) {
  sx = 2 * SEG; sy = 60; svx = svy = 0; sa = 0; sinv = 2.0f; firecd = 0;
  for (int i = 0; i < NEB; i++) eb[i].life = 0;
  for (int i = 0; i < NPB; i++) pb[i].life = 0;
  camx = sx;
}

static void gen_level(void) {
  rng_s = 2654435761u * (u32)(level + 3);
  float amp = 38 + level * 16; if (amp > 160) amp = 160;
  float w[SEGS + 1]; w[0] = 0;
  for (int i = 1; i <= SEGS; i++) w[i] = w[i - 1] + frange(-amp, amp);
  float drift = w[SEGS], mean = 0;
  for (int i = 0; i < SEGS; i++) { th[i] = w[i] - drift * i / SEGS; mean += th[i]; }
  mean /= SEGS;
  for (int i = 0; i < SEGS; i++) {
    th[i] = th[i] - mean + 470;
    if (th[i] < 300) th[i] = 300 + (300 - th[i]) * 0.3f;
    if (th[i] > 640) th[i] = 640;
  }
  int nsp = level / 2; if (nsp > 5) nsp = 5;
  for (int k = 0; k < nsp; k++) {
    int i = 7 + (int)(rnd() % (SEGS - 12));
    th[i] -= frange(90, 170); if (th[i] < 190) th[i] = 190;
  }
  // flatten the spawn area
  int used[SEGS];
  for (int i = 0; i < SEGS; i++) used[i] = 0;
  for (int i = SEGS - 3; i < SEGS; i++) used[i] = 1;
  for (int i = 0; i <= 4; i++) used[i] = 1;

  nbk = 3 + level; if (nbk > NBUNK) nbk = NBUNK;
  ntk = level < 4 ? 3 : 4;
  int placed = 0;
  for (int tries = 0; tries < 600 && placed < nbk + ntk; tries++) {
    int s = (int)(rnd() % SEGS);
    if (used[s] || used[(s + 1) % SEGS] || used[(s + SEGS - 1) % SEGS]) continue;
    th[(s + 1) % SEGS] = th[s];
    used[s] = used[(s + 1) % SEGS] = used[(s + SEGS - 1) % SEGS] = 1;
    float x = s * SEG + SEG / 2, y = th[s];
    if (placed < nbk) {
      Bunker *b = &bk[placed];
      b->x = x; b->y = y; b->alive = 1; b->timer = frange(1.0f, 3.5f); b->ax = 0; b->ay = -1;
    } else {
      Tank *t = &tk[placed - nbk];
      t->x = x; t->y = y; t->alive = 1;
    }
    placed++;
  }
  if (placed < nbk) { nbk = placed; ntk = 0; }
  else ntk = placed - nbk;
}

static void start_level(void) {
  gen_level();
  grav = 34 + level * 7; if (grav > 105) grav = 105;
  spawn_ship();
}

static void new_game(void) {
  level = 1; lives = 3; score = 0; fuel = 10000; nextLife = 10000;
  start_level();
  state = ST_PLAY;
}

static void kill_ship(void) {
  explode(sx, sy, 70, C_BOOM, 160);
  explode(sx, sy, 25, C_SHIP, 90);
  state = ST_DYING; timer = 2.2f; events |= EV_DIE;
  thrusting = shielding = 0;
}

static void add_score(int n) {
  score += n;
  if (score >= nextLife) { lives++; nextLife += 10000; events |= EV_LIFE; }
}

// ---------- ship ----------
static void ship_pt(float px, float py, float *ox, float *oy) {
  float c = cos_(sa), s = sin_(sa);
  *ox = sx + px * c - py * s;
  *oy = sy + px * s + py * c;
}

static void step_ship(float dt, int in, int pressed) {
  (void)pressed;
  if (in & IN_L) sa -= 3.6f * dt;
  if (in & IN_R) sa += 3.6f * dt;
  if (sa > PI) sa -= 2 * PI; if (sa < -PI) sa += 2 * PI;
  float dx = sin_(sa), dy = -cos_(sa);
  thrusting = (in & IN_T) && fuel > 0;
  shielding = (in & IN_S) && fuel > 0;
  if (thrusting) { svx += dx * 150 * dt; svy += dy * 150 * dt; fuel -= 55 * dt; events |= EV_THRUST; }
  if (shielding) { fuel -= 40 * dt; events |= EV_SHIELD; }
  if (fuel < 0) fuel = 0;
  svy += grav * dt;
  svx *= 1 - 0.08f * dt; svy *= 1 - 0.04f * dt;
  float sp = sqrtf_(svx * svx + svy * svy);
  if (sp > 340) { svx *= 340 / sp; svy *= 340 / sp; }
  sx = wrapx(sx + svx * dt); sy += svy * dt;
  if (sy < 20) { sy = 20; if (svy < 0) svy = 0; }
  if (sinv > 0) sinv -= dt;

  // fire
  firecd -= dt;
  if ((in & IN_F) && firecd <= 0) {
    for (int i = 0; i < NPB; i++) if (pb[i].life <= 0) {
      float nx, ny; ship_pt(0, -11, &nx, &ny);
      pb[i].x = nx; pb[i].y = ny; pb[i].vx = svx + dx * 430; pb[i].vy = svy + dy * 430; pb[i].life = 1.1f;
      firecd = 0.17f; events |= EV_FIRE; break;
    }
  }

  // tractor beam
  if (shielding) {
    for (int i = 0; i < ntk; i++) if (tk[i].alive) {
      float ddx = wrapd(tk[i].x - sx), ddy = tk[i].y - sy;
      if (absf(ddx) < 45 && ddy > 0 && ddy < 135) {
        tk[i].alive = 0; fuel += 2200; add_score(150); events |= EV_FUEL;
        explode(tk[i].x, tk[i].y - 7, 18, C_FUEL, 60);
      }
    }
  }

  // collisions with terrain and bunkers
  static const float vx_[3] = {0, -8, 8}, vy_[3] = {-11, 8, 8};
  for (int k = 0; k < 3; k++) {
    float px, py; ship_pt(vx_[k], vy_[k], &px, &py);
    if (py >= ground(px)) { kill_ship(); return; }
  }
  for (int i = 0; i < nbk; i++) if (bk[i].alive) {
    float ddx = wrapd(bk[i].x - sx), ddy = (bk[i].y - 6) - sy;
    if (ddx * ddx + ddy * ddy < 16 * 16) { bk[i].alive = 0; explode(bk[i].x, bk[i].y - 6, 40, C_BUNK, 130); kill_ship(); return; }
  }
}

// ---------- bullets & enemies ----------
static void step_bullets(float dt) {
  for (int i = 0; i < NPB; i++) {
    Bul *b = &pb[i]; if (b->life <= 0) continue;
    b->life -= dt; b->x = wrapx(b->x + b->vx * dt); b->y += b->vy * dt;
    if (b->y >= ground(b->x)) { b->life = 0; explode(b->x, b->y, 4, C_TERR, 50); continue; }
    if (b->y < -200) { b->life = 0; continue; }
    for (int k = 0; k < nbk; k++) if (bk[k].alive) {
      float dx = wrapd(bk[k].x - b->x), dy = (bk[k].y - 6) - b->y;
      if (dx * dx + dy * dy < 17 * 17) {
        bk[k].alive = 0; b->life = 0; add_score(250 + 25 * level); events |= EV_BOOM;
        explode(bk[k].x, bk[k].y - 6, 45, C_BUNK, 140); explode(bk[k].x, bk[k].y - 6, 20, C_BOOM, 90);
        break;
      }
    }
    if (b->life <= 0) continue;
    for (int k = 0; k < ntk; k++) if (tk[k].alive) {
      float dx = wrapd(tk[k].x - b->x), dy = (tk[k].y - 7) - b->y;
      if (dx * dx + dy * dy < 13 * 13) {
        tk[k].alive = 0; b->life = 0; add_score(50); events |= EV_BOOM;
        explode(tk[k].x, tk[k].y - 7, 30, C_FUEL, 110);
        break;
      }
    }
  }
  for (int i = 0; i < NEB; i++) {
    Bul *b = &eb[i]; if (b->life <= 0) continue;
    b->life -= dt; b->x = wrapx(b->x + b->vx * dt); b->y += b->vy * dt;
    if (b->y >= ground(b->x) || b->y < -200) { b->life = 0; continue; }
    if (state == ST_PLAY) {
      float dx = wrapd(sx - b->x), dy = sy - b->y;
      float r = shielding ? 18 : 10;
      if (dx * dx + dy * dy < r * r) {
        b->life = 0;
        if (shielding) explode(b->x, b->y, 6, C_SHLD, 60);
        else if (sinv <= 0) { kill_ship(); }
      }
    }
  }
}

static void step_bunkers(float dt) {
  float speed = 140 + level * 14; if (speed > 300) speed = 300;
  float rate = 2.8f - level * 0.18f; if (rate < 0.9f) rate = 0.9f;
  for (int i = 0; i < nbk; i++) {
    Bunker *b = &bk[i]; if (!b->alive) continue;
    float dx = wrapd(sx - b->x), dy = sy - (b->y - 10);
    float d = sqrtf_(dx * dx + dy * dy);
    if (d > 1 && dy < 0) {  // slowly track the ship
      float tx = dx / d, ty = dy / d;
      b->ax += (tx - b->ax) * 3 * dt; b->ay += (ty - b->ay) * 3 * dt;
      float m = sqrtf_(b->ax * b->ax + b->ay * b->ay); if (m > 0.01f) { b->ax /= m; b->ay /= m; }
    }
    b->timer -= dt;
    if (b->timer > 0) continue;
    b->timer = rate + frange(0, 1.2f);
    if (state != ST_PLAY || d > 560 || dy > -15) continue;
    float lead = level >= 3 ? 0.55f : 0.0f;
    float t = d / speed;
    float ax = dx + svx * t * lead, ay = dy + svy * t * lead;
    float m = sqrtf_(ax * ax + ay * ay); ax /= m; ay /= m;
    float err = frange(-0.12f, 0.12f);
    float nx = ax - ay * err, ny = ay + ax * err;
    for (int k = 0; k < NEB; k++) if (eb[k].life <= 0) {
      eb[k].x = b->x + b->ax * 10; eb[k].y = b->y - 10 + b->ay * 10;
      eb[k].vx = nx * speed; eb[k].vy = ny * speed; eb[k].life = 3.2f;
      events |= EV_EFIRE; break;
    }
  }
}

static void step_parts(float dt) {
  for (int i = 0; i < NPART; i++) {
    Part *p = &pt[i]; if (p->life <= 0) continue;
    p->life -= dt; p->vy += grav * 0.5f * dt;
    p->x = wrapx(p->x + p->vx * dt); p->y += p->vy * dt;
    p->vx *= 1 - 0.6f * dt; p->vy *= 1 - 0.6f * dt;
    if (p->y > ground(p->x)) { p->y = ground(p->x) - 0.5f; p->vy *= -0.3f; p->vx *= 0.5f; }
  }
}

static void step_camera(float dt, int snap) {
  if (state == ST_TITLE) {
    camx = wrapx(camx + 45 * dt);
    float target = 380;
    camy += (target - camy) * (snap ? 1 : 2 * dt);
    return;
  }
  if (state == ST_DYING || state == ST_OVER) return;
  float dx = wrapd(sx - camx);
  camx = wrapx(camx + dx * (snap ? 1 : (8 * dt > 1 ? 1 : 8 * dt)));
  float g = ground(sx);
  float target = (sy + g) * 0.5f;
  float lim = halfH * 0.72f;
  if (target > sy + lim) target = sy + lim;
  if (target < sy - lim) target = sy - lim;
  camy += (target - camy) * (snap ? 1 : (4 * dt > 1 ? 1 : 4 * dt));
}

// ---------- rendering ----------
static void render(void) {
  nl = 0;
  float f = (camx - halfW) / SEG;
  int i0 = (int)f; if (f < 0 && (float)i0 != f) i0--; i0 -= 1;
  int n = (int)((2 * halfW) / SEG) + 4;
  for (int k = i0; k < i0 + n; k++) {
    int a = ((k % SEGS) + SEGS) % SEGS, b = (a + 1) % SEGS;
    float x1 = k * SEG - camx, x2 = x1 + SEG;
    add_rel(x1, th[a] - camy, x2, th[b] - camy, C_TERR);
  }
  nterr = nl;

  for (int i = 0; i < NSTAR; i++)
    if (stary[i] < ground(starx[i]) - 12) add_w(starx[i], stary[i], starx[i] + 1.2f, stary[i], C_STAR);

  for (int i = 0; i < ntk; i++) if (tk[i].alive) {
    float x = tk[i].x, y = tk[i].y;
    add_w(x - 9, y, x - 9, y - 15, C_FUEL); add_w(x - 9, y - 15, x + 9, y - 15, C_FUEL);
    add_w(x + 9, y - 15, x + 9, y, C_FUEL); add_w(x + 9, y, x - 9, y, C_FUEL);
    add_w(x - 3, y - 3, x - 3, y - 12, C_FUEL); add_w(x - 3, y - 12, x + 4, y - 12, C_FUEL);
    add_w(x - 3, y - 8, x + 2, y - 8, C_FUEL);
  }

  for (int i = 0; i < nbk; i++) if (bk[i].alive) {
    float x = bk[i].x, y = bk[i].y;
    add_w(x - 15, y, x - 10, y - 9, C_BUNK); add_w(x - 10, y - 9, x + 10, y - 9, C_BUNK);
    add_w(x + 10, y - 9, x + 15, y, C_BUNK); add_w(x - 6, y - 9, x - 4, y - 13, C_BUNK);
    add_w(x - 4, y - 13, x + 4, y - 13, C_BUNK); add_w(x + 4, y - 13, x + 6, y - 9, C_BUNK);
    add_w(x, y - 11, x + bk[i].ax * 13, y - 11 + bk[i].ay * 13, C_BUNK);
  }

  for (int i = 0; i < NPB; i++) if (pb[i].life > 0) {
    float s = 0.012f;
    add_w(pb[i].x, pb[i].y, pb[i].x - pb[i].vx * s, pb[i].y - pb[i].vy * s, C_PBUL);
  }
  for (int i = 0; i < NEB; i++) if (eb[i].life > 0) {
    float x = eb[i].x, y = eb[i].y;
    add_w(x - 2.5f, y, x, y - 2.5f, C_EBUL); add_w(x, y - 2.5f, x + 2.5f, y, C_EBUL);
    add_w(x + 2.5f, y, x, y + 2.5f, C_EBUL); add_w(x, y + 2.5f, x - 2.5f, y, C_EBUL);
  }

  if (state == ST_PLAY || state == ST_CLEAR) {
    int blink = sinv > 0 && ((int)(sinv * 10) & 1);
    if (!blink) {
      float ax, ay, bx, by, cx, cy, nx, ny;
      ship_pt(0, -11, &ax, &ay); ship_pt(-8, 8, &bx, &by); ship_pt(0, 4, &nx, &ny); ship_pt(8, 8, &cx, &cy);
      add_w(ax, ay, bx, by, C_SHIP); add_w(bx, by, nx, ny, C_SHIP);
      add_w(nx, ny, cx, cy, C_SHIP); add_w(cx, cy, ax, ay, C_SHIP);
    }
    if (thrusting || state == ST_CLEAR) {
      float l = 13 + frand() * 9;
      float ax, ay, bx, by, cx, cy;
      ship_pt(-4, 6, &ax, &ay); ship_pt(0, 6 + l, &bx, &by); ship_pt(4, 6, &cx, &cy);
      add_w(ax, ay, bx, by, C_FLAME); add_w(bx, by, cx, cy, C_FLAME);
    }
    if (shielding) {
      for (int k = 0; k < 18; k++) {
        float a1 = k * 2 * PI / 18, a2 = (k + 1) * 2 * PI / 18;
        add_w(sx + cos_(a1) * 17, sy + sin_(a1) * 17, sx + cos_(a2) * 17, sy + sin_(a2) * 17, C_SHLD);
      }
      add_w(sx - 8, sy + 14, sx - 42, sy + 135, C_BEAM);
      add_w(sx + 8, sy + 14, sx + 42, sy + 135, C_BEAM);
    }
  }

  for (int i = 0; i < NPART; i++) if (pt[i].life > 0) {
    float s = 0.035f;
    add_w(pt[i].x, pt[i].y, pt[i].x - pt[i].vx * s - 0.8f, pt[i].y - pt[i].vy * s, pt[i].col);
  }
}

// ---------- exports ----------
EXPORT("init") void init(void) {
  rng_s = 777;
  for (int i = 0; i < NSTAR; i++) { starx[i] = frange(0, WW); stary[i] = frange(-700, 600); }
  level = 1; gen_level(); grav = 40;
  state = ST_TITLE; camx = 0; camy = 380; prevIn = 0;
  render();
}

EXPORT("set_view") void set_view(float w, float h) { halfW = w * 0.5f; halfH = h * 0.5f; }

EXPORT("update") void update(float dt, int in) {
  if (dt > 0.05f) dt = 0.05f;
  if (dt < 0) dt = 0;
  events = 0;
  int pressed = in & ~prevIn;
  prevIn = in;
  int snap = 0;

  switch (state) {
    case ST_TITLE:
      if (pressed & (IN_F | IN_T)) { new_game(); snap = 1; }
      break;
    case ST_PLAY: {
      step_ship(dt, in, pressed);
      int alive = 0;
      for (int i = 0; i < nbk; i++) alive += bk[i].alive;
      if (state == ST_PLAY && alive == 0) {
        state = ST_CLEAR; timer = 3.0f; add_score(1000 * level); fuel += 1500; events |= EV_CLEAR;
        shielding = 0;
      }
    } break;
    case ST_DYING:
      timer -= dt;
      if (timer <= 0) {
        lives--;
        if (lives <= 0) { state = ST_OVER; timer = 0; }
        else { spawn_ship(); state = ST_PLAY; snap = 1; }
      }
      break;
    case ST_CLEAR:
      timer -= dt;
      thrusting = 0;
      svy -= 300 * dt; svx *= 1 - 2 * dt;
      sa *= 1 - 4 * dt;
      sx = wrapx(sx + svx * dt); sy += svy * dt;
      if (timer <= 0) { level++; start_level(); state = ST_PLAY; snap = 1; }
      break;
    case ST_OVER:
      timer += dt;
      if (timer > 1.0f && (pressed & (IN_F | IN_T))) { new_game(); snap = 1; }
      break;
  }
  step_bullets(dt);
  step_bunkers(dt);
  step_parts(dt);
  step_camera(dt, snap);
  render();
}

EXPORT("lines_ptr") float *lines_ptr(void) { return L; }
EXPORT("line_count") int line_count(void) { return nl; }
EXPORT("terrain_count") int terrain_count(void) { return nterr; }
EXPORT("get_state") int get_state(void) { return state; }
EXPORT("get_score") int get_score(void) { return score; }
EXPORT("get_lives") int get_lives(void) { return lives; }
EXPORT("get_level") int get_level(void) { return level; }
EXPORT("get_fuel") int get_fuel(void) { return (int)fuel; }
EXPORT("get_events") int get_events(void) { return events; }
EXPORT("get_gravity") int get_gravity(void) { return (int)grav; }
EXPORT("get_bunkers") int get_bunkers(void) { int a = 0; for (int i = 0; i < nbk; i++) a += bk[i].alive; return a; }
