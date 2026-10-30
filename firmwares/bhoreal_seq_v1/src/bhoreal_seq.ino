// Bhoreal SLIM step sequencer
//
// Grid layout (row 0 = top, the edge with the USB connector):
//   row 0      controls: col 0 = tempo down, col 1 = tempo up,
//              col 3 = delete, col 4 = copy, col 6 = play, col 7 = stop
//   row 1      modes: col 5 = drums + melody, col 6 = drums only,
//              col 7 = melody only. The tracks below show the layer being
//              edited; in drums + melody mode, pressing col 5 again swaps it.
//   row 2      8 pattern memories, each holding a drum and a melody layer.
//              While playing, the chosen one is queued and loaded when the
//              loop wraps back to step 1.
//              Hold delete + press a memory to clear it.
//              Hold copy + press a source memory, then a destination.
//   rows 3-7   five tracks, columns = 8 steps
//
// Sounds live on the Mac: drums go out as MIDI notes on channel 10
// (General MIDI drums), the melody on channel 1 (C major pentatonic).
//
// Memories, tempo and mode are saved to EEPROM a couple of seconds after
// the last change, so they survive unplugging.

#include <Arduino.h>
#include <EEPROM.h>
#include "BhorealHw.h"
#include "UsbMidi.h"

const uint8_t STEPS = 8;
const uint8_t TRACKS = 5;
const uint8_t FIRST_TRACK_ROW = 3;
const uint8_t VELOCITY = 100;

enum Layer : uint8_t { DRUMS, MELODY, LAYERS };
enum Mode : uint8_t { MODE_BOTH, MODE_DRUMS, MODE_MELODY };

const uint8_t layerChannel[LAYERS] = {9, 0};  // MIDI channels 10 and 1
const uint8_t layerNotes[LAYERS][TRACKS] = {
  {36, 38, 42, 46, 39},  // kick, snare, closed hi-hat, open hi-hat, clap
  {69, 67, 64, 62, 60},  // A4 G4 E4 D4 C4, highest note on top
};
const uint8_t drumColor[TRACKS][3] = {
  {60, 0, 0},
  {60, 22, 0},
  {45, 45, 0},
  {0, 40, 50},
  {50, 0, 50},
};
const uint8_t MELODY_COLOR[3] = {0, 70, 20};

const uint8_t CONTROL_ROW = 0;
const uint8_t TEMPO_DOWN_COL = 0;
const uint8_t TEMPO_UP_COL = 1;
const uint8_t DELETE_COL = 3;
const uint8_t COPY_COL = 4;
const uint8_t PLAY_COL = 6;
const uint8_t STOP_COL = 7;
const uint8_t BPM_MIN = 40;
const uint8_t BPM_MAX = 240;
const uint8_t BPM_STEP = 5;

const uint8_t MODE_ROW = 1;
const uint8_t BOTH_COL = 5;
const uint8_t DRUMS_COL = 6;
const uint8_t MELODY_COL = 7;

const uint8_t MEMORY_ROW = 2;
const uint8_t MEMORIES = 8;
const int8_t NONE = -1;

// bit n set = step n active
uint8_t memories[MEMORIES][LAYERS][TRACKS];
uint8_t current = 0;
int8_t queued = NONE;     // memory to load at the end of the loop
int8_t copySource = NONE; // memory picked while holding copy

Mode mode = MODE_DRUMS;
Layer editLayer = DRUMS;

// EEPROM layout, after the bytes the original firmware used (0-32).
const int EE_BASE = 64;
const uint8_t EE_MAGIC[2] = {'B', 'S'};  // "Bhoreal Seq"
const int EE_VERSION = EE_BASE + 2;
const uint8_t EE_LAYOUT = 2;
const int EE_BPM = EE_BASE + 3;
const int EE_V1_MEMORIES = EE_BASE + 4;  // version 1: drums only, 8 x 5 bytes
const int EE_MODE = EE_BASE + 4;
const int EE_EDIT = EE_BASE + 5;
const int EE_MEMORIES = EE_BASE + 6;     // version 2: 8 x 2 x 5 bytes
const uint16_t SAVE_DELAY_MS = 2000;
bool unsaved = false;
uint32_t lastEditMs;

uint8_t bpm = 120;
uint8_t step = 0;
uint8_t sounding[LAYERS];  // bit t set = track t note is on
bool playing = false;
uint32_t lastStepUs;
bool dirty = true;

void markEdited();

uint32_t stepIntervalUs() {
  return 15000000UL / bpm;  // sixteenth notes
}

bool layerAudible(Layer layer) {
  return mode == MODE_BOTH || (layer == DRUMS) == (mode == MODE_DRUMS);
}

void releaseNotes() {
  for (uint8_t l = 0; l < LAYERS; l++) {
    for (uint8_t t = 0; t < TRACKS; t++)
      if (sounding[l] & (1 << t)) midi.send(0x80 | layerChannel[l], layerNotes[l][t], 0);
    sounding[l] = 0;
  }
}

void playStep() {
  releaseNotes();
  for (uint8_t l = 0; l < LAYERS; l++) {
    if (!layerAudible((Layer)l)) continue;
    for (uint8_t t = 0; t < TRACKS; t++) {
      if (memories[current][l][t] & (1 << step)) {
        midi.send(0x90 | layerChannel[l], layerNotes[l][t], VELOCITY);
        sounding[l] |= 1 << t;
      }
    }
  }
  midi.flush();
}

void loadMemory(uint8_t m) {
  current = m;
  queued = NONE;
}

bool memoryUsed(uint8_t m) {
  for (uint8_t l = 0; l < LAYERS; l++)
    for (uint8_t t = 0; t < TRACKS; t++)
      if (memories[m][l][t]) return true;
  return false;
}

void loadSaved() {
  for (uint8_t i = 0; i < sizeof(EE_MAGIC); i++)
    if (EEPROM.read(EE_BASE + i) != EE_MAGIC[i]) return;  // nothing saved yet
  uint8_t version = EEPROM.read(EE_VERSION);
  uint8_t savedBpm = EEPROM.read(EE_BPM);
  if (savedBpm >= BPM_MIN && savedBpm <= BPM_MAX) bpm = savedBpm;

  if (version == 1) {
    // Older firmware saved only drums; bring them over and rewrite as v2.
    for (uint8_t m = 0; m < MEMORIES; m++)
      for (uint8_t t = 0; t < TRACKS; t++)
        memories[m][DRUMS][t] = EEPROM.read(EE_V1_MEMORIES + m * TRACKS + t);
    markEdited();
    return;
  }
  if (version != EE_LAYOUT) return;

  uint8_t savedMode = EEPROM.read(EE_MODE);
  if (savedMode <= MODE_MELODY) mode = (Mode)savedMode;
  uint8_t savedEdit = EEPROM.read(EE_EDIT);
  if (savedEdit < LAYERS) editLayer = (Layer)savedEdit;
  uint8_t* bytes = &memories[0][0][0];
  for (uint16_t i = 0; i < sizeof(memories); i++) bytes[i] = EEPROM.read(EE_MEMORIES + i);
}

// EEPROM.update only writes bytes that changed, to spare the EEPROM.
void save() {
  for (uint8_t i = 0; i < sizeof(EE_MAGIC); i++) EEPROM.update(EE_BASE + i, EE_MAGIC[i]);
  EEPROM.update(EE_VERSION, EE_LAYOUT);
  EEPROM.update(EE_BPM, bpm);
  EEPROM.update(EE_MODE, mode);
  EEPROM.update(EE_EDIT, editLayer);
  const uint8_t* bytes = &memories[0][0][0];
  for (uint16_t i = 0; i < sizeof(memories); i++) EEPROM.update(EE_MEMORIES + i, bytes[i]);
  unsaved = false;
}

void markEdited() {
  unsaved = true;
  lastEditMs = millis();
}

void start() {
  if (playing) return;
  playing = true;
  step = 0;
  lastStepUs = micros();
  playStep();
}

void stop() {
  playing = false;
  step = 0;
  if (queued != NONE) loadMemory(queued);
  releaseNotes();
  midi.flush();
}

void selectMode(uint8_t col) {
  if (col == BOTH_COL) {
    if (mode == MODE_BOTH) editLayer = (editLayer == DRUMS) ? MELODY : DRUMS;
    mode = MODE_BOTH;
  } else if (col == DRUMS_COL) {
    mode = MODE_DRUMS;
    editLayer = DRUMS;
  } else if (col == MELODY_COL) {
    mode = MODE_MELODY;
    editLayer = MELODY;
  } else {
    return;
  }
  markEdited();
}

void onMemoryPress(uint8_t m) {
  if (bhoreal::isDown(CONTROL_ROW, DELETE_COL)) {
    memset(memories[m], 0, sizeof(memories[m]));
    markEdited();
  } else if (bhoreal::isDown(CONTROL_ROW, COPY_COL)) {
    if (copySource == NONE) {
      if (memoryUsed(m)) copySource = m;
    } else if (m != copySource) {
      memcpy(memories[m], memories[copySource], sizeof(memories[m]));
      copySource = NONE;
      markEdited();
    }
  } else if (!playing) {
    loadMemory(m);
  } else {
    queued = (m == current) ? NONE : m;
  }
}

void onPress(uint8_t row, uint8_t col) {
  if (row == CONTROL_ROW) {
    if (col == TEMPO_DOWN_COL && bpm > BPM_MIN) { bpm -= BPM_STEP; markEdited(); }
    else if (col == TEMPO_UP_COL && bpm < BPM_MAX) { bpm += BPM_STEP; markEdited(); }
    else if (col == PLAY_COL) start();
    else if (col == STOP_COL) stop();
  } else if (row == MODE_ROW) {
    selectMode(col);
  } else if (row == MEMORY_ROW) {
    onMemoryPress(col);
  } else if (row >= FIRST_TRACK_ROW) {
    memories[current][editLayer][row - FIRST_TRACK_ROW] ^= 1 << col;
    markEdited();
  }
  dirty = true;
}

void render() {
  bhoreal::clear();

  // Controls hold steady colours; held buttons and the active transport
  // button are brighter. Nothing static goes below ~30: these LEDs flicker
  // visibly at very low levels each time a new frame arrives.
  bool deleting = bhoreal::isDown(CONTROL_ROW, DELETE_COL);
  bool copying = bhoreal::isDown(CONTROL_ROW, COPY_COL);
  bhoreal::setPixel(CONTROL_ROW, TEMPO_DOWN_COL, 0, 0, 40);
  bhoreal::setPixel(CONTROL_ROW, TEMPO_UP_COL, 40, 14, 0);
  if (deleting) bhoreal::setPixel(CONTROL_ROW, DELETE_COL, 90, 0, 30);
  else bhoreal::setPixel(CONTROL_ROW, DELETE_COL, 36, 0, 12);
  if (copying) bhoreal::setPixel(CONTROL_ROW, COPY_COL, 80, 80, 0);
  else bhoreal::setPixel(CONTROL_ROW, COPY_COL, 30, 30, 0);
  bhoreal::setPixel(CONTROL_ROW, PLAY_COL, 0, playing ? 70 : 30, 0);
  bhoreal::setPixel(CONTROL_ROW, STOP_COL, playing ? 30 : 70, 0, 0);

  // Modes: the active one is bright; in drums + melody mode the layer being
  // edited is lit at medium brightness.
  uint8_t both = mode == MODE_BOTH ? 80 : 30;
  uint8_t drums = mode == MODE_DRUMS ? 80 : (mode == MODE_BOTH && editLayer == DRUMS ? 55 : 30);
  uint8_t melody = mode == MODE_MELODY ? 80 : (mode == MODE_BOTH && editLayer == MELODY ? 55 : 30);
  bhoreal::setPixel(MODE_ROW, BOTH_COL, both, both, both);
  bhoreal::setPixel(MODE_ROW, DRUMS_COL, drums, 0, 0);
  bhoreal::setPixel(MODE_ROW, MELODY_COL, 0, melody, melody / 4);

  // Memories: current = bright white, queued = medium white, copy source =
  // yellow, with notes = dim cyan, empty = off.
  for (uint8_t m = 0; m < MEMORIES; m++) {
    if (m == copySource) bhoreal::setPixel(MEMORY_ROW, m, 50, 50, 0);
    else if (m == current) bhoreal::setPixel(MEMORY_ROW, m, 50, 50, 50);
    else if (m == queued) bhoreal::setPixel(MEMORY_ROW, m, 30, 30, 30);
    else if (memoryUsed(m)) bhoreal::setPixel(MEMORY_ROW, m, 0, 30, 40);
  }

  const uint8_t* pattern = memories[current][editLayer];
  for (uint8_t t = 0; t < TRACKS; t++) {
    const uint8_t* c = editLayer == DRUMS ? drumColor[t] : MELODY_COLOR;
    for (uint8_t s = 0; s < STEPS; s++) {
      bool on = pattern[t] & (1 << s);
      if (playing && s == step) {
        if (on) bhoreal::setPixel(FIRST_TRACK_ROW + t, s, 180, 180, 180);
        else bhoreal::setPixel(FIRST_TRACK_ROW + t, s, 30, 30, 30);
      } else if (on) {
        bhoreal::setPixel(FIRST_TRACK_ROW + t, s, c[0], c[1], c[2]);
      }
    }
  }
  bhoreal::show();
}

// Sweep a rainbow down the grid, one row at a time, so it is obvious the
// board has started.
void startupAnimation() {
  for (uint8_t row = 0; row < 8; row++) {
    bhoreal::clear();
    for (uint8_t col = 0; col < 8; col++) {
      uint8_t phase = col * 32;
      uint8_t r = phase < 128 ? 60 - phase / 3 : 0;
      uint8_t g = phase < 128 ? phase / 3 : 60 - (phase - 128) / 3;
      uint8_t b = phase < 128 ? 0 : (phase - 128) / 3;
      bhoreal::setPixel(row, col, r, g, b);
    }
    bhoreal::show();
    delay(80);
  }
  bhoreal::clear();
  bhoreal::show();
}

void reportStatus() {
  static uint32_t lastReport;
  if (millis() - lastReport < 2000) return;
  lastReport = millis();
  Serial.print(F("bhoreal-seq helper="));
  Serial.print(bhoreal::helperAnswered() ? F("ok") : F("no-answer"));
  Serial.print(F(" power="));
  Serial.print(bhoreal::helperPowerAcked() ? F("ack") : F("no-ack"));
  Serial.print(playing ? F(" playing") : F(" stopped"));
  Serial.print(F(" bpm="));
  Serial.print(bpm);
  Serial.print(F(" mode="));
  Serial.print(mode == MODE_BOTH ? F("both") : mode == MODE_DRUMS ? F("drums") : F("melody"));
  Serial.print(F(" edit="));
  Serial.print(editLayer == DRUMS ? F("drums") : F("melody"));
  Serial.print(F(" memory="));
  Serial.print(current + 1);
  for (uint8_t l = 0; l < LAYERS; l++) {
    Serial.print(l == DRUMS ? F(" drums=") : F(" melody="));
    for (uint8_t t = 0; t < TRACKS; t++) {
      Serial.print(memories[current][l][t], HEX);
      Serial.print(t < TRACKS - 1 ? ',' : ' ');
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  bhoreal::begin();
  loadSaved();
  startupAnimation();
}

void loop() {
  bhoreal::scanButtons(onPress);

  // Redraw when delete/copy is released, and forget a half-done copy.
  static uint8_t wasHeld = 0;
  bool copyHeld = bhoreal::isDown(CONTROL_ROW, COPY_COL);
  uint8_t held = copyHeld | (bhoreal::isDown(CONTROL_ROW, DELETE_COL) << 1);
  if (!copyHeld) copySource = NONE;
  if (held != wasHeld) {
    wasHeld = held;
    dirty = true;
  }

  uint32_t interval = stepIntervalUs();
  if (playing && micros() - lastStepUs >= interval) {
    lastStepUs += interval;
    step = (step + 1) % STEPS;
    if (step == 0 && queued != NONE) loadMemory(queued);
    playStep();
    dirty = true;
  }

  if (dirty) {
    render();
    dirty = false;
  }

  if (unsaved && millis() - lastEditMs >= SAVE_DELAY_MS) save();

  midi.drain();
  if (Serial) reportStatus();
}
