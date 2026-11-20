#include "face_player.h"

#include <string.h>

namespace face {

bool live = false;
float batteryLevel = -1;
void (*onCue)(const char*) = nullptr;

void Scene::background(Sc& S, float) { S.face.drawBackground(); }

double Player::length() {
  if (!scene_) return 0;
  const double own = scene_->length(S);
  if (own >= 0) return own;
  const SceneInfo* in = info();
  return in ? in->dur : 0;
}

int findScene(const char* id) {
  for (int i = 0; i < SCENE_COUNT; i++)
    if (strcmp(SCENES[i].id, id) == 0) return i;
  return -1;
}

void Sc::at(double when, std::function<void(Sc&)> fn) {
  // keep the list sorted; equal times run in the order they were added
  auto it = timers.begin();
  while (it != timers.end() && it->t <= when) ++it;
  timers.insert(it, Cue{when, std::move(fn)});
}

void Sc::say(const char* s, const SayOpt& o) {
  cap.on = true;
  strncpy(cap.str, s, sizeof(cap.str) - 1);
  cap.str[sizeof(cap.str) - 1] = 0;
  cap.t0 = t;
  cap.out = -1;
  cap.o = o;
  if (o.glance) {
    face.look(0, 0.75f, 300);
    const bool keepLook = o.keepLook;
    after(0.5, [keepLook](Sc& S) {
      if (!keepLook) S.face.look(0, 0, 300);
    });
  }
}

void Player::begin(uint16_t* frame) {
  scr.setFrame(frame, frame + Screen::W * Screen::HALF);
  S.scr = &scr;
  S.player = this;
}

void Player::play(int sceneIndex) {
  index = sceneIndex;
  hostScene_ = nullptr;
  reset();
}

void Player::play(Scene* scene) {
  index = -1;
  hostScene_ = scene;
  reset();
}

void Player::reseed(uint32_t seed) {
  S.r = Rng(seed);
  S.face.reseed(seed * 2654435761u + 7);
  S.fx.reseed(seed * 40503u + 99);
}

void Player::reset() {
  S.timers.clear();
  if (scene_ != hostScene_) delete scene_;
  scene_ = nullptr;
  scr.dim = 1;
  S.face.begin(&scr, 7);
  S.fx.begin(&scr);
  S.L = S.face.L;
  S.skin = S.face.skin;
  S.t = 0;
  S.dt = 0;
  S.cap = Caption();
  S.r = Rng(1234);
  if (hostScene_) {
    scene_ = hostScene_;
  } else {
    if (index < 0 || index >= SCENE_COUNT) return;
    scene_ = SCENES[index].make();
  }
  scene_->setup(S);
}

void Player::step(double dt) {
  if (dt > 0.1) dt = 0.1;  // after a stall, slow down rather than run thousands of 4 ms spring steps
  S.dt = dt;
  S.t += dt;
  while (!S.timers.empty() && S.timers.front().t <= S.t) {
    auto fn = std::move(S.timers.front().fn);
    S.timers.erase(S.timers.begin());
    fn(S);
  }
  if (scene_) scene_->frame(S, (float)S.t);
  S.face.update(dt);
  S.fx.update((float)dt);
  if (S.cap.on && S.cap.out >= 0 && S.t - S.cap.out > 0.25) S.cap.on = false;
}

void Player::draw() {
  const float t = (float)S.t;
  ProfMark pm;
  if (scene_) scene_->background(S, t);
  else S.face.drawBackground();
  pm.to(PR_BG);
  if (scene_) scene_->under(S, t);
  pm.to(PR_UNDER);
  S.face.draw(true);
  pm.to(PR_FACE);
  if (scene_) scene_->mid(S, t);
  pm.to(PR_MID);
  S.fx.draw();
  pm.to(PR_FX);
  if (scene_) scene_->over(S, t);
  pm.to(PR_OVER);
  if (S.cap.on) drawCaption();
  pm.to(PR_CAPTION);
  if (scene_) scene_->post(S, t);
  pm.to(PR_POST);
}

void Player::renderAt(double t) {
  reset();
  const double dt = 1.0 / 30;
  double tt = 0;
  while (tt + dt <= t) {
    step(dt);
    tt += dt;
  }
  draw();
}

void Player::tap() {
  if (scene_) scene_->tap(S);
}
void Player::button() {
  if (scene_) scene_->button(S);
}
void Player::event(const char* name) {
  if (scene_) scene_->event(S, name);
}

// Captions: up to three lines, auto-fit to the width, popped in with overshoot. Shouts use the display face.
void Player::drawCaption() {
  const Caption& c = S.cap;
  const SayOpt& o = c.o;
  const Layout& L = S.L;
  const float t = (float)S.t;
  const float age = (float)(S.t - c.t0);
  const float out = c.out >= 0 ? clampf((float)(S.t - c.out) / 0.25f, 0, 1) : 0;
  char buf[sizeof(c.str)];
  memcpy(buf, c.str, sizeof(buf));
  const char* lines[4];
  int n = 0;
  lines[n++] = buf;
  for (char* p = buf; *p && n < 4; p++)
    if (*p == '\n') {
      *p = 0;
      lines[n++] = p + 1;
    }
  const Font& font = o.shout ? FONT_SHOUT : FONT_SAY;
  const float maxW = o.width ? o.width : L.w - 10;
  float size = o.size ? o.size : (o.shout ? 30 : 19);
  const C3 col = o.hasColor ? o.color : S.skin->text, oc = o.hasOc ? o.oc : S.skin->textOutline;
  const float outline = o.outline >= 0 ? o.outline : (o.shout ? 2.5f : 2);
  for (int k = 0; k < 6; k++) {
    float wmax = 0;
    for (int i = 0; i < n; i++)
      wmax = maxf(wmax, ceilf(Screen::textWidth(font, size, lines[i])) + 2 * (ceilf(outline) - outline));
    if (wmax <= maxW) break;
    size = floorf(size * maxW / wmax);
  }
  const float lh = size * (o.shout ? 1.0f : 1.08f);
  const float cy = (o.y > -1e8f ? o.y : L.capY) - (n - 1) * lh / 2;
  for (int i = 0; i < n; i++) {
    const float delay = i * 0.12f;
    const float p = o.noPop ? 1 : Ease::outBack(clampf((age - delay) / 0.32f, 0, 1));
    const float wob = o.wobble ? sinf(t * 9 + i) * 0.05f : 0;
    const float y = cy + i * lh + (o.bounce ? -fabsf(sinf(t * 6 + i)) * 3 : 0);
    TextOpt to;
    to.font = &font;
    to.size = size;
    to.color = col;
    to.outline = outline;
    to.oc = oc;
    to.scale = maxf(0, p) * (1 - out);
    to.rot = wob;
    to.alpha = 1 - out;
    S.scr->text(lines[i], L.cx, y, to);
  }
}

}  // namespace face
