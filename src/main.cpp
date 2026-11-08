#include <BleKeyboard.h>

constexpr int IR_PIN = 4;
constexpr uint32_t HB = 408;          // half-bit slot, µs
constexpr size_t MAX_LINE_LEN = 256;
constexpr uint32_t BLE_KEY_MS = 10;   // pacing between BLE keystrokes, ms
BleKeyboard kb("HP50g Keys", "DIY", 100);

// 256-entry ring buffer indexed by uint8_t (wraps naturally at 256)
volatile uint32_t ts[256];
volatile uint8_t head = 0;
uint8_t tail = 0;

void IRAM_ATTR onBurst() { ts[head++] = micros(); }

// Non-blocking BLE keystroke queue so BLE transmits never block IR reception
uint8_t bleQueue[512];
uint16_t bleHead = 0;
uint16_t bleTail = 0;
uint32_t lastBleT = 0;

void enqueueBle(uint8_t k) {
  if ((uint16_t)(bleHead - bleTail) < sizeof(bleQueue))
    bleQueue[bleHead++ & (sizeof(bleQueue) - 1)] = k;
}

bool inFrame = false;
uint32_t startT, lastT;
uint16_t bits;
int nbits;
int nStart;                           // start bursts seen in current frame
int missedIdx;                        // first bit index where a slot jump occurred
String line;

struct Map { const char* code; uint8_t key; };
// HP 50g key codes are "RC.P": row, column, shift plane (.1 = unshifted)
constexpr Map keymap[] = {
  // arrows
  {"25.1", KEY_UP_ARROW},   {"35.1", KEY_DOWN_ARROW},
  {"34.1", KEY_LEFT_ARROW}, {"36.1", KEY_RIGHT_ARROW},
  // soft-menu keys
  {"11.1", KEY_F1}, {"12.1", KEY_F2}, {"13.1", KEY_F3},
  {"14.1", KEY_F4}, {"15.1", KEY_F5}, {"16.1", KEY_F6},
  // number pad
  {"72.1", '7'}, {"73.1", '8'}, {"74.1", '9'},
  {"82.1", '4'}, {"83.1", '5'}, {"84.1", '6'},
  {"92.1", '1'}, {"93.1", '2'}, {"94.1", '3'},
  {"102.1", '0'}, {"103.1", '.'}, {"104.1", ' '},
  // operators
  {"65.1", '/'}, {"75.1", '*'}, {"85.1", '-'}, {"95.1", '+'},
  {"51.1", '^'}, {"61.1", 'e'}, {"63.1", 'x'}, {"43.1", '\''},
  {"62.1", '-'}, // +/-
  // editing
  {"105.1", KEY_RETURN}, {"45.1", KEY_BACKSPACE},
  // unmapped: APPS 21, MODE 22, TOOL 23, VAR 31, STO 32, NXT 33, HIST 41,
  // EVAL 42, SYMB 44, sqrt 52, SIN 53, COS 54, TAN 55, 1/X 64
};

// Key presses arrive as "K<code>" (e.g. "K36.1") from the TECLA program on the
// calculator; anything else is ordinary printed output and is typed as text.
// Returns the code part, or "" if the line is not a key press.
String keyCodeOf(const String& s) {
  int start = 0;
  int end = s.length();
  if (end >= 2 && s[0] == '"' && s[end - 1] == '"') {
    start++;
    end--;                                           // in case PR1 kept the quotes
  }
  return (end - start > 1 && s[start] == 'K') ? s.substring(start + 1, end) : String();
}

// Translate common HP 50g / HP 82240B extended bytes (>= 0x80) to ASCII
const char* roman8ToAscii(uint8_t c) {
  switch (c) {
    case 0x83: case 0x86: return "sqrt";
    case 0x84: return "integral";
    case 0x85: return "sigma";
    case 0x87: case 0xA3: return "pi";
    case 0x89: return "<=";
    case 0x8A: return ">=";
    case 0x8B: return "!=";
    case 0x8D: return "->";
    case 0x8E: return "<-";
    case 0x9B: return "*";
    case 0x9F: return "inf";
    case 0xAB: case 0xAE: return "<<";
    case 0xBB: case 0xAF: return ">>";
    case 0xF7: return "/";
    default:   return nullptr;
  }
}

void gotChar(uint8_t c) {
  if (c >= 0x20 && c < 0x7F) {
    if (line.length() < MAX_LINE_LEN) line += (char)c;
    return;
  }
  if (c >= 0x80) {
    const char* mapped = roman8ToAscii(c);
    if (mapped && line.length() + strlen(mapped) <= MAX_LINE_LEN) line += mapped;
    return;
  }
  // control character (line feed / end of print line): line is complete
  if (line.length() == 0) return;
  bool bt = kb.isConnected();
  Serial.printf("%s  [BLE %s]\n", line.c_str(), bt ? "connected" : "NOT connected");
  if (bt) {
    String code = keyCodeOf(line);
    if (code.length()) {               // key press: send mapped key, ignore unmapped
      for (const auto& m : keymap)
        if (code == m.code) { enqueueBle(m.key); break; }
    } else {                           // printed output: type it as text
      int start = 0, end = line.length();
      // Strip outer '...' or "..." delimiters added by HP 50g PR1 on symbolic/string objects
      if (end >= 2 && ((line[0] == '\'' && line[end - 1] == '\'') ||
                       (line[0] == '"'  && line[end - 1] == '"'))) {
        start++;
        end--;
      }
      for (int i = start; i < end; i++)
        enqueueBle((uint8_t)line[i]);
      enqueueBle(KEY_RETURN);
    }
  }
  line = "";
}

constexpr uint16_t REDEYE_CORR_MASK[16] = {
  0x000, 0x100, 0x200, 0x001, 0x400, 0x002, 0x004, 0x080,
  0x800, 0x008, 0x010, 0x000, 0x020, 0x000, 0x040, 0x000
};

uint8_t redEyeSyndrome(uint16_t raw) {
  constexpr uint16_t masks[4] = {0x878, 0x4E6, 0x2D5, 0x18B};
  uint8_t syndrome = 0;
  for (int i = 0; i < 4; i++)
    syndrome = (syndrome << 1) | __builtin_parity(raw & masks[i]);
  return syndrome;
}

// Verify (and 1-bit error-correct) a 12-bit RedEye frame (H1..H4 in bits 11..8, data in 7..0)
bool decodeRedEye(uint16_t raw, uint8_t& outByte) {
  uint8_t syndrome = redEyeSyndrome(raw);
  if (syndrome != 0) {
    uint16_t flip = REDEYE_CORR_MASK[syndrome];
    if (flip == 0) return false;       // uncorrectable multi-bit error
    raw ^= flip;
  }
  outByte = raw & 0xFF;
  return true;
}

// Recover a frame where 1 of the 12 bursts was missed (11 bursts received)
bool recover11BitFrame(uint8_t& outByte) {
  int m = (missedIdx >= 0 && missedIdx <= 11) ? missedIdx : 11;
  int pos = 11 - m;
  uint16_t bitMask = 1U << pos;
  uint16_t raw12 = ((bits >> pos) << (pos + 1)) | (bits & (bitMask - 1));
  uint8_t syndrome = redEyeSyndrome(raw12);
  if (syndrome != 0) {
    if (REDEYE_CORR_MASK[syndrome] != bitMask) return false;
    raw12 ^= bitMask;
  }
  outByte = raw12 & 0xFF;
  return true;
}

void beginFrame(uint32_t t) {
  inFrame = true; startT = lastT = t; bits = 0; nbits = 0; nStart = 1; missedIdx = -1;
}

void handleBurst(uint32_t t) {
  // a gap longer than 4 slots can't happen inside a frame, so it's a new one
  uint32_t gap = t - lastT;
  if (inFrame && gap < HB / 2) return;     // glitch: real bursts are >= 1 slot apart
  if (!inFrame || gap > 4 * HB) {
    if (inFrame && nbits > 0) {
      uint8_t decoded;
      if (nbits == 11 && recover11BitFrame(decoded))
        gotChar(decoded);
      else
        Serial.printf("! frame cut after %d bits (gap %lu us)\n", nbits, (unsigned long)gap);
    }
    beginFrame(t);
    return;
  }
  if (nStart < 3) {                        // start = 3 bursts, one slot apart
    if (gap > HB + HB / 2) { beginFrame(t); return; }   // stray pulse before: restart here
    nStart++; lastT = t;
    return;
  }
  lastT = t;
  int slot = (t - startT + HB / 2) / HB;   // half-bit slot number
  if (slot < 3) return;                    // still in the start bursts
  slot -= 3;
  if (slot / 2 > nbits && missedIdx < 0) missedIdx = nbits;
  bits = (bits << 1) | ((slot & 1) == 0);  // burst in first half = 1
  if (++nbits == 12) {
    uint8_t decoded;
    if (decodeRedEye(bits, decoded))
      gotChar(decoded);
    else
      Serial.printf("! parity error (raw 0x%03X)\n", bits);
    inFrame = false;
  }
}

void setup() {
  Serial.begin(115200);
  line.reserve(MAX_LINE_LEN);
  pinMode(IR_PIN, INPUT_PULLUP);
  attachInterrupt(IR_PIN, onBurst, FALLING);
  kb.begin();
}

void loop() {
  while (tail != head) handleBurst(ts[tail++]);
  uint32_t idleUs = micros() - lastT;
  if (inFrame && idleUs > 8 * HB) {                             // drop or recover broken frames
    uint8_t decoded;
    if (nbits == 11 && recover11BitFrame(decoded))
      gotChar(decoded);
    else if (nbits > 0)
      Serial.printf("! frame timed out after %d bits\n", nbits);
    inFrame = false;
  }
  // Drain queued BLE keys only when IR receiver is idle so BLE never interrupts IR timing
  if (bleTail != bleHead && !inFrame && idleUs > 6 * HB && millis() - lastBleT >= BLE_KEY_MS) {
    if (kb.isConnected()) kb.write(bleQueue[bleTail & (sizeof(bleQueue) - 1)]);
    bleTail++;
    lastBleT = millis();
  }
}
