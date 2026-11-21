/*
  宿舍智能机械关灯装置 — 固件 v3.0.0（双舵机 + SU-03T 语音 + 夜间深眠）
  NodeMCU(ESP8266) + 左灯 SG90(D2/GPIO4) + 右灯 MG90S(D5/GPIO14)（PRD D2 + D8 定案）
  语音：SU-03T @ D7(RX)/D6(TX) 9600bps；不猜厂商协议——页面"学习绑定"实测帧（PRD D9）
  电源：夜间深眠窗口（默认 19:30~02:30），窗口外主控深睡 + 语音门控断电（PRD D10）

  实现依据（本文件所有编号对应设计文档）：
    FIRMWARE_DESIGN.md  §3 动作状态机 / §4 角度限位 / §5 并发去重 / §6 参数存储
                        §8 指令契约 / §9 错误目录 / §10 承载层 / §13 OTA
                        §十六 双通道改订（v2.0.0）
    WEB_DESIGN.md       D-1~D-12 + 改订附录（全关主按钮 + 左右灯单控）
    PRD.md              D8（双舵机双通道，v1.5 定案）
    PLAN.md             §四 校准 SOP（左右各一遍）

  ===================== 必读 =====================
  1) 工具链已内置：双击 ../../build_firmware.bat 编译；
     build_firmware.bat upload COM5 烧录（eesz=4M1M 双 OTA 分区）。
  2) 【F-6】板子实为 1MB 时：OTA 整段视为不存在（PRD R7），其余功能不受影响。
  3) 本 sketch 零第三方库：只用核心自带 ESP8266WiFi / ESP8266WebServer /
     ESP8266mDNS / DNSServer / Servo / EEPROM / Update。
  4) v2.0.0 参数结构换代（P_VER=2）：老板子升级后校准值自动回默认，
     必须重新走一遍左右灯校准（安装本来就变了，合理）。
  5) 上电顺序/供电纪律/接线：README §2.5（左 D2 右 D5，共地，单电源纪律不变）。
  =====================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>          // 核心自带：captive portal 手法（D7 定案）
#include <SoftwareSerial.h>     // 核心自带：SU-03T 语音串口（D9）
#include <Servo.h>
#include <EEPROM.h>
#include <Updater.h>

#define FIRMWARE_VERSION "3.0.2"   // v3.0.2: 诊断强化——页面禁缓存（防旧 JS 吞掉时区修复）+ /status 自报装置时钟 t

// ============================================================
// 1. 承载层常量（FIRMWARE_DESIGN §10 / PRD OA-2、OA-5、OA-7 定案）
//    全工程唯一一处，推翻定案只改这里
// ============================================================
static const char    AP_SSID[]   = "GUANGDENG";
static const char    AP_PASS[]   = "111222345";      // WPA2（PRD OA-2）
static const uint8_t AP_CHANNEL  = 1;                // 固定信道（PRD §7.1）
static const char    HOSTNAME[]  = "guangdeng";      // mDNS 加分项，说明只印 IP
static const uint16_t HTTP_PORT  = 80;               // 纯 AP：装置即网关，无撞车（PRD §7.1）

// 双通道舵机（PRD D8）：0=左灯 1=右灯。
// GPIO4 有启动电平约束的是 GPIO2/GPIO5，D5=GPIO14 完全干净（无 LED、无 SD 复用冲突、无 boot 约束）。
static const uint8_t NUM_CH      = 2;
static const uint8_t CH_L        = 0;
static const uint8_t CH_R        = 1;
static const uint8_t SERVO_PINS[NUM_CH] = {4, 14};   // 左 D2/GPIO4，右 D5/GPIO14
static const uint8_t LED_PIN     = 2;                // 板载 LED（低电平亮），仅状态指示

// --- v3.0.0：语音（D9）与电源调度（D10）引脚/常量 ---
static const uint8_t  VOICE_RX_PIN = 13;             // D7 ← SU-03T TX
static const uint8_t  VOICE_TX_PIN = 12;             // D6 → SU-03T RX（v3 空闲，预留回执帧）
static const uint8_t  VOICE_GATE   = 15;             // D8 → 三极管基极(1kΩ)：HIGH=语音上电；开机天然 LOW 安全
static const uint32_t VOICE_BAUD   = 9600;           // 配置工具必须把 SU-03T 波特率改成 9600
static const uint32_t FRAME_GAP_MS = 40;             // 帧间静默 = 判帧边界（协议无关学习法）
static const uint8_t  VSLOTS       = 6;              // 学习槽 1..6（见 VLBL 表）
static const uint8_t  VFRAME_MAX   = 8;              // 单帧最长字节数
static const uint16_t PW_START_DEF = 19 * 60 + 30;   // 窗口默认 19:30 开
static const uint16_t PW_END_DEF   = 2 * 60 + 30;    // 次日 02:30 关
static const uint32_t PW_CHUNK_S   = 1800;           // 深睡分片 ≤30min（RTC 定时器安全范围内）

// 安全常量（FIRMWARE_DESIGN §4）
static const int      DEG_MIN   = 10;                // 角度上下限（两端各留 10° 避开死区/顶死）
static const int      DEG_MAX   = 170;
static const int      MIN_GAP   = 10;                // A/B 与中位角最小差值
static const uint16_t MOVE_HARD_LIMIT_MS = 3000;     // 单段看门狗下限 → E-6 强制断电
static const uint16_t HOLD_MIN = 150, HOLD_MAX = 800;
static const uint32_t DEDUP_MS = 5000;               // 同方向同通道去重窗口（§5.1）
static const uint32_t OTA_GRACE_MS = 45000;          // 新固件 45 秒存活窗口（§13.3）

// ============================================================
// 2. 参数块与持久化（§6 + §十六）
//    EEPROM 分区：0..99 参数块；128..147 OTA 状态；160..217 语音区；228..235 电源区
// ============================================================
static const uint16_t EEPROM_SIZE   = 256;
static const uint16_t ADDR_PARAMS   = 0;
static const uint16_t ADDR_OTASTATE = 128;

struct Params {
  uint16_t magic;
  uint8_t  ver;
  int16_t  angleOff[NUM_CH];     // 关灯角 A（每通道一套）
  int16_t  angleOn[NUM_CH];      // 开灯角 B
  int16_t  angleHome[NUM_CH];    // 中位角
  uint8_t  mirrorBits;           // 方向镜像：bit0 左 / bit1 右（各舵机安装方向独立）
  uint8_t  calibrated;           // 校准标记：bit0 左 / bit1 右
  uint8_t  speedMode;            // 1..4（全局：两舵机共用，供电侧约束一致）
  uint16_t holdMs;               // 150..800（全局）
  uint8_t  returnHome;           // 动作后回中位并松劲（全局）
  uint8_t  pad[3];
  uint16_t checksum;
};

struct OtaState {           // §13.3 的自实现回滚（只用确定存在的 API）
  uint16_t magic;           // OTA_MAGIC=已上传未确认；OTA_OKMAGIC=已确认存活
  uint8_t  boots;           // 上传后累计启动次数
  uint8_t  reserved;
};
static const uint16_t OTA_MAGIC   = 0x07A0;
static const uint16_t OTA_CONFIRM = 0x4C0F;

static const uint16_t P_MAGIC = 0x4C44;   // 'LD'
static const uint8_t  P_VER   = 2;        // v2：双通道结构换代，v1 数据自动作废回默认

Params P;
OtaState O = {0, 0, 0};

// ============================================================
// 2b. 语音区（@160）与电源区（@228）——独立于参数块存放。
//     刻意不 bump P_VER：v2→v3 升级，左右校准值原封不动（吸取 v1→v2 全报废的教训）
// ============================================================
static const uint16_t ADDR_VOICECFG = 160;
static const uint16_t ADDR_PWCFG    = 228;
static const uint16_t V_MAGIC  = 0x5156;   // 'QV'
static const uint16_t PW_MAGIC = 0x5150;   // 'QP'

struct VSlot { uint8_t len; uint8_t b[VFRAME_MAX]; };          // 动作码 = 下标+1
struct VoiceCfg { uint16_t magic; uint8_t en; VSlot slots[VSLOTS]; uint8_t csum; };
struct PwCfg    { uint16_t magic; uint8_t en; uint16_t winStart, winEnd; uint8_t csum; };
VoiceCfg VC;
PwCfg    PW;

static const char* VLBL[VSLOTS + 1] = { "-", "off-all", "off-L", "off-R", "on-all", "on-L", "on-R" };
static const char* VCNAME[VSLOTS + 1] = { "-", "全关", "关左灯", "关右灯", "全开", "开左灯", "开右灯" };

uint8_t boundCount() { uint8_t n = 0; for (uint8_t i = 0; i < VSLOTS; i++) if (VC.slots[i].len && VC.slots[i].len <= VFRAME_MAX) n++; return n; }
String hmStr(uint16_t m) { char b[6]; sprintf(b, "%02u:%02u", (unsigned)(m / 60) % 24, (unsigned)(m % 60)); return String(b); }

uint8_t csum8(const uint8_t* b, size_t n) { uint8_t s = 0; for (size_t i = 0; i + 1 < n; i++) s += b[i]; return s; }

bool voiceLoad() {
  EEPROM.get(ADDR_VOICECFG, VC);
  if (VC.magic == V_MAGIC && VC.csum == csum8((uint8_t*)&VC, sizeof(VC))) return true;
  VC.magic = V_MAGIC; VC.en = 1; memset(VC.slots, 0, sizeof(VC.slots));
  VC.csum = csum8((uint8_t*)&VC, sizeof(VC));
  return false;
}
bool voiceSave() {
  VC.csum = csum8((uint8_t*)&VC, sizeof(VC));
  EEPROM.put(ADDR_VOICECFG, VC);
  return EEPROM.commit();
}
bool pwLoad() {
  EEPROM.get(ADDR_PWCFG, PW);
  if (PW.magic == PW_MAGIC && PW.csum == csum8((uint8_t*)&PW, sizeof(PW))) return true;
  PW.magic = PW_MAGIC; PW.en = 1; PW.winStart = PW_START_DEF; PW.winEnd = PW_END_DEF;
  PW.csum = csum8((uint8_t*)&PW, sizeof(PW));
  return false;
}
bool pwSave() {
  PW.csum = csum8((uint8_t*)&PW, sizeof(PW));
  EEPROM.put(ADDR_PWCFG, PW);
  return EEPROM.commit();
}

// ---- 帧接收状态（gap 判帧；学习绑定后持久化） ----
SoftwareSerial voiceSer(VOICE_RX_PIN, VOICE_TX_PIN);
uint8_t  rxFrame[VFRAME_MAX + 1]; uint8_t rxLen = 0; uint32_t rxLastAt = 0;
char     frameRing[6][3 * VFRAME_MAX];  uint32_t frameRingAt[6]; uint8_t ringPos = 0;
uint8_t  learnArmed = 0;        uint32_t learnDeadline = 0;
char     lastVoiceHit[12] = "-";  uint32_t lastVoiceHitAt = 0;

// ---- 时间基准：RTC 内存跨深睡存活；断电丢失 → 未校准期间保持常醒（页面开一次即恢复） ----
static const uint32_t RTC_MAGIC = 0xD17E;
uint32_t epochBase = 0, anchorMs = 0; bool timeOk = false;
void rtcGet() {
  uint32_t w[2]; timeOk = false;
  if (ESP.rtcUserMemoryRead(16, w, 8) && w[0] == RTC_MAGIC) {
    epochBase = w[1]; anchorMs = millis(); timeOk = true;
  }
}
void rtcPut() { uint32_t w[2] = { RTC_MAGIC, epochBase }; ESP.rtcUserMemoryWrite(16, w, 8); }
uint32_t epochNow() { return timeOk ? epochBase + (uint32_t)((millis() - anchorMs) / 1000) : 0; }
uint32_t minuteOfDay() { return (epochNow() / 60) % 1440; }

bool inWindow() {
  if (!timeOk || !PW.en || PW.winStart == PW.winEnd) return true;    // 无时钟/未启用 = 常醒
  uint32_t m = minuteOfDay();
  if (PW.winStart < PW.winEnd) return m >= PW.winStart && m < PW.winEnd;
  return m >= PW.winStart || m < PW.winEnd;                           // 跨午夜窗口
}
int32_t minutesToStart() {                                            // 仅窗口外调用
  int32_t d = (int32_t)PW.winStart - (int32_t)minuteOfDay();
  if (d < 0) d += 1440;
  return d;
}
uint32_t lastHttpMs = 0;                                              // 深睡前 5 分钟宽限：有人正用页面就别睡

void paramsDefaults() {
  P.magic = P_MAGIC; P.ver = P_VER;
  for (uint8_t i = 0; i < NUM_CH; i++) {          // PLAN §四 起点猜测值，非标定值
    P.angleOff[i] = 138; P.angleOn[i] = 42; P.angleHome[i] = 90;
  }
  P.mirrorBits = 0; P.calibrated = 0;
  P.speedMode = 2; P.holdMs = 300; P.returnHome = 1;
  memset(P.pad, 0, sizeof(P.pad));
  P.checksum = 0;
}

uint16_t paramsCsum(const Params* q) {
  const uint8_t* b = (const uint8_t*)q;
  uint16_t s = 0;
  for (uint16_t i = 0; i < (uint16_t)offsetof(Params, checksum); i++)
    s = (uint16_t)(s + b[i] * (uint16_t)(i + 7));
  return s;
}

bool paramsLoad() {                                   // false = 校验和坏/老版本/首跑（E-5 场景）
  EEPROM.get(ADDR_PARAMS, P);
  if (P.magic != P_MAGIC || P.ver != P_VER || P.checksum != paramsCsum(&P)) {
    paramsDefaults();
    return false;
  }
  return true;
}

bool paramsSave() {                                   // 写 + 回读比对（§6.2，E-8 判定）
  P.checksum = paramsCsum(&P);
  EEPROM.put(ADDR_PARAMS, P);
  bool ok = EEPROM.commit();
  Params chk; EEPROM.get(ADDR_PARAMS, chk);
  ok = ok && chk.magic == P_MAGIC && chk.checksum == P.checksum &&
       memcmp(&chk, &P, sizeof(Params)) == 0;
  return ok;
}

void otaStateLoad() { EEPROM.get(ADDR_OTASTATE, O); }
void otaStateSave() { EEPROM.put(ADDR_OTASTATE, O); EEPROM.commit(); }

// ============================================================
// 3. 角度换算与校验（§4 + §十六：全部按通道）
// ============================================================
static inline int clampDeg(int d) { return constrain(d, DEG_MIN, DEG_MAX); }

static int effAngleCh(int stored, uint8_t ch) {
  int d = stored;
  if (P.mirrorBits & (1 << ch)) d = 2 * (int)P.angleHome[ch] - d;   // 以本通道中位角为轴镜像
  return clampDeg(d);
}
int effOff(uint8_t ch)  { return effAngleCh(P.angleOff[ch], ch); }
int effOn(uint8_t ch)   { return effAngleCh(P.angleOn[ch], ch); }
int effHome(uint8_t ch) { return clampDeg(P.angleHome[ch]); }

uint16_t speedInterval() {                            // 每 1° 间隔 ms（慢=不易拉垮电压，R1）
  switch (P.speedMode) { case 1: return 40; case 3: return 10; case 4: return 5; default: return 20; }
}
int speedStepDeg() { return (P.speedMode >= 3) ? 2 : 1; }

int paramsValidate(const Params* q) {                 // 返回 0=合法，否则 E-码（§十六 加通道循环）
  if (q->speedMode < 1 || q->speedMode > 4) return 1;
  if (q->holdMs < HOLD_MIN || q->holdMs > HOLD_MAX) return 1;
  for (uint8_t ch = 0; ch < NUM_CH; ch++) {
    if (q->angleOff[ch] < DEG_MIN || q->angleOff[ch] > DEG_MAX) return 1;
    if (q->angleOn[ch]  < DEG_MIN || q->angleOn[ch]  > DEG_MAX) return 1;
    if (q->angleHome[ch]< DEG_MIN || q->angleHome[ch]> DEG_MAX) return 1;
    if (q->angleOff[ch] == q->angleOn[ch]) return 3;
    if (abs(q->angleOff[ch] - q->angleHome[ch]) < MIN_GAP) return 3;
    if (abs(q->angleOn[ch]  - q->angleHome[ch]) < MIN_GAP) return 3;
    if ((q->angleOff[ch] - q->angleHome[ch]) * (q->angleOn[ch] - q->angleHome[ch]) >= 0) return 3;
  }
  return 0;
}

// ============================================================
// 4. 动作状态机（§3 + §十六）—— 非阻塞；单锁串行，两舵机永不并行
//    "全关" = mask 0b11：先跑低位通道，A_POST 时从队列取下一个通道重新 A_CHECK
// ============================================================
enum Act : uint8_t { A_IDLE, A_CHECK, A_ATTACH, A_SLEW, A_HOLD, A_RETURN, A_DETACH, A_POST, A_FAULT };
static const uint8_t S_NONE = 0, S_OFF = 1, S_ON = 2;
static inline const char* chName(uint8_t c) { return c == CH_L ? "L" : "R"; }

Servo srvs[NUM_CH];                   // 每通道独立实例，同一时刻只 attach 活跃通道
ESP8266WebServer server(HTTP_PORT);
DNSServer dnsServer;                  // 探测域名全部解析到装置 IP（D7 定案）

Act      act = A_IDLE;
uint8_t  actSide = S_NONE;
uint8_t  actCh = CH_L;                // 当前活跃通道
uint8_t  queueMask = 0;               // 待执行通道（全关时 = 另一半）
bool     actTrial = false;
bool     busy = false;                // 全局动作锁（页面看到 busy:true）
bool     otaLock = false;             // 上传期间全局锁
bool     otaGrace = false;            // 45 秒新固件观察窗（拒绝动作，保留页面与再上传）
uint32_t actT0 = 0, legT0 = 0, stepNextAt = 0;
uint32_t legLimitMs = 3000;           // 本段看门狗 = 行程×节拍 + 裕量（MG90S 慢速大行程不误杀）
int      curDeg = 0, tgtDeg = 0;
uint32_t dedupAt[2][NUM_CH + 1];      // 去重戳：[side-1][mask-1]（§5.1 按方向×通道集）
uint16_t countBoot = 0;               // 本次开机计数（§7.2 不落盘）
int      lastErr = 0;
uint8_t  lastResult = 0;              // 1 成功 / 2 超时中断(E-6)
uint32_t lastActAt = 0;
char     actLabel[10] = "-";          // 本次请求标签："off-L"/"on-R"/"off-all"
char     lastActLabel[10] = "-";
uint8_t  attachedBits = 0;
uint32_t rebootAt = 0, settleOffAt = 0, graceDeadline = 0;

const char* stateName() {
  if (otaLock)  return "ota";
  if (otaGrace) return "grace";
  switch (act) {
    case A_IDLE: return "idle";
    case A_CHECK: case A_ATTACH: return "attach";
    case A_SLEW: return "slew";
    case A_HOLD: return "hold";
    case A_RETURN: return "return";
    case A_DETACH: return "detach";
    case A_POST: return "post";
    default: return "fault";
  }
}

void servoOff() {                     // DETACH = 松劲：省电 + 不与手动较劲（§12.4 / PRD R3 软件侧）
  for (uint8_t i = 0; i < NUM_CH; i++) if (attachedBits & (1 << i)) { srvs[i].detach(); }
  attachedBits = 0;
}

bool startAction(uint8_t side, uint8_t mask, const char* label, bool trial) {  // 只置状态；绝不在请求里 delay
  if (busy || otaLock) return false;
  actSide = side; actTrial = trial;
  actCh = (mask & 1) ? CH_L : CH_R;               // 低位通道先动（左→右）
  queueMask = (uint8_t)(mask & (mask - 1));       // 去掉低位 = 队列剩余
  snprintf(actLabel, sizeof(actLabel), "%s", label);
  busy = true;
  act = A_CHECK;
  return true;
}

void checkLimit(uint32_t now) {                       // 每段限时=行程×节拍+裕量；全程 15 秒兜底（§3.2/E-6）
  if (act != A_IDLE && act != A_POST && act != A_FAULT &&
      (now - legT0 > legLimitMs || now - actT0 > 15000))
    act = A_FAULT;
}

void loopAction() {
  if (act == A_IDLE) return;
  uint32_t now = millis();

  switch (act) {
    case A_CHECK: {
      tgtDeg = (actSide == S_ON) ? effOn(actCh) : effOff(actCh);
      curDeg = effHome(actCh);
      actT0 = now; legT0 = now; stepNextAt = now;
      legLimitMs = (uint32_t)(abs(tgtDeg - curDeg)) * speedInterval() + 2500;
      if (legLimitMs < MOVE_HARD_LIMIT_MS) legLimitMs = MOVE_HARD_LIMIT_MS;
      Serial.printf("[act] start ch=%s side=%s home=%d target=%d speed=%d legLimit=%lums\n",
                    chName(actCh), actSide == S_ON ? "on" : "off", curDeg, tgtDeg, P.speedMode,
                    (unsigned long)legLimitMs);
      act = A_ATTACH;
      break;
    }
    case A_ATTACH: {                                  // 上电建立 PWM，等 60ms（§3.2）
      if (now < stepNextAt) { checkLimit(now); break; }
      srvs[actCh].attach(SERVO_PINS[actCh], 544, 2400);
      attachedBits |= (1 << actCh);
      srvs[actCh].write(curDeg);
      stepNextAt = now + 60;
      act = A_SLEW;
      break;
    }
    case A_SLEW: case A_RETURN: {                     // 分步转角
      if (now < stepNextAt) { checkLimit(now); break; }
      int step = speedStepDeg();
      curDeg = (curDeg < tgtDeg) ? min(tgtDeg, curDeg + step)
                                 : max(tgtDeg, curDeg - step);
      srvs[actCh].write(curDeg);
      stepNextAt = now + speedInterval();
      if (curDeg == tgtDeg) {
        if (act == A_SLEW) { act = A_HOLD; legT0 = now; stepNextAt = now + P.holdMs; legLimitMs = P.holdMs + 2000; }
        else               { act = A_DETACH; stepNextAt = now; }
      }
      checkLimit(now);
      break;
    }
    case A_HOLD: {
      if (now < stepNextAt) { checkLimit(now); break; }
      if (P.returnHome) {
        tgtDeg = effHome(actCh); act = A_RETURN; legT0 = now;
        legLimitMs = (uint32_t)(abs(tgtDeg - curDeg)) * speedInterval() + 2000;
        if (legLimitMs < MOVE_HARD_LIMIT_MS) legLimitMs = MOVE_HARD_LIMIT_MS;
      } else              { act = A_DETACH; stepNextAt = now; }
      checkLimit(now);
      break;
    }
    case A_DETACH: {
      if (now < stepNextAt) { checkLimit(now); break; }
      if (attachedBits & (1 << actCh)) { srvs[actCh].detach(); attachedBits &= ~(1 << actCh); }
      act = A_POST;
      break;
    }
    case A_POST: {
      uint32_t dur = now - actT0;
      Serial.printf("[act] done ch=%s dur=%lums trial=%d\n", chName(actCh), (unsigned long)dur, (int)actTrial);
      if (queueMask) {                               // §十六：串行队列还有下一通道
        uint8_t next = (queueMask & 1) ? CH_L : CH_R;
        queueMask &= (uint8_t)~(1 << next);
        actCh = next;
        act = A_CHECK;                               // busy 保持 true，全局动作锁不松
        break;
      }
      if (!actTrial) {
        countBoot++;
        lastActAt = actT0;
        snprintf(lastActLabel, sizeof(lastActLabel), "%s", actLabel);
      }
      lastResult = 1;
      busy = false; act = A_IDLE; actSide = S_NONE;
      break;
    }
    case A_FAULT: {
      servoOff();                                     // 全断电，队列一并放弃
      queueMask = 0;
      lastResult = 2; lastErr = 6;
      if (!actTrial) {
        lastActAt = millis();
        snprintf(lastActLabel, sizeof(lastActLabel), "%s", actLabel);
      }
      busy = false; act = A_IDLE; actSide = S_NONE;
      Serial.println("[act] FAULT: leg limit, PWM cut, queue dropped");
      break;
    }
    default: act = A_IDLE; busy = false; queueMask = 0; break;
  }
}

// 参数变更后让两通道实际位置都跟上中位（不计数、随后自动松劲）
void servoFollowHome() {
  if (busy || otaLock || otaGrace) return;
  servoOff();
  for (uint8_t ch = 0; ch < NUM_CH; ch++) {
    srvs[ch].attach(SERVO_PINS[ch], 544, 2400);
    attachedBits |= (1 << ch);
    srvs[ch].write(effHome(ch));
  }
  settleOffAt = millis() + 350;                       // 校位后也松劲，不长期保力矩
}

// ============================================================
// 4b. v3.0.0 语音链路（D9）：学习绑定 → 帧匹配 → 走同一条 startAction
//     + 夜间深眠调度（D10）。全部非阻塞。
// ============================================================
void gateUpdate() { digitalWrite(VOICE_GATE, (VC.en && inWindow() && !otaGrace) ? HIGH : LOW); }

void gotoSleep() {
  if (busy || otaLock || otaGrace) return;
  servoOff();
  digitalWrite(VOICE_GATE, LOW);
  int32_t waitS = minutesToStart() * 60;
  uint32_t chunkS = (waitS > (int32_t)PW_CHUNK_S) ? PW_CHUNK_S : (uint32_t)waitS;
  if (chunkS < 2) { gateUpdate(); return; }                     // 已到窗口点，继续醒着
  epochBase += chunkS; rtcPut();
  Serial.printf("[pw] sleep %us (window-open in %d min)\n", (unsigned)chunkS, (int)(waitS / 60));
  delay(120);
  ESP.deepSleep((uint64_t)chunkS * 1000000ULL, (waitS <= (int32_t)PW_CHUNK_S + 60) ? WAKE_RF_DEFAULT : WAKE_NO_RFCAL);
}
void loopPower() {
  static uint32_t nextCheck = 0;
  uint32_t now = millis();
  if (now < nextCheck) return;
  nextCheck = now + 2000;
  if (!PW.en || !timeOk || inWindow()) return;
  if (now - lastHttpMs < 300000) return;                        // 页面还在用：宽限 5 分钟
  if (busy || otaLock || otaGrace) return;
  gotoSleep();                                                  // 不返回，直到下个分片醒来
}

void frameHex(const uint8_t* b, uint8_t n, char* out) {
  out[0] = 0;
  for (uint8_t i = 0; i < n; i++) { sprintf(out + strlen(out), (i ? " %02X" : "%02X"), b[i]); }
}
bool slotFilled(const VSlot* s) { return s->len > 0 && s->len <= VFRAME_MAX; }

void voiceFire(uint8_t code) {                                  // 1..6 → 与网页同锁同去重（去重戳复用）
  uint8_t side = (code <= 3) ? S_OFF : S_ON;
  uint8_t mask = (code == 1 || code == 4) ? (uint8_t)0b11 : (code == 2 || code == 5) ? (uint8_t)(1 << CH_L) : (uint8_t)(1 << CH_R);
  if (otaGrace || otaLock || busy) { Serial.println("[voice] rejected (busy/grace)"); return; }
  uint32_t now = millis();
  uint32_t* ref = &dedupAt[side - 1][mask - 1];
  if (now - *ref < DEDUP_MS) { Serial.println("[voice] deduped"); return; }
  *ref = now;
  snprintf(lastVoiceHit, sizeof(lastVoiceHit), "%s", VLBL[code]); lastVoiceHitAt = epochNow();
  if (startAction(side, mask, VLBL[code], false))
    Serial.printf("[voice] %s -> %s\n", VCNAME[code], VLBL[code]);
}

void frameDone() {                                              // 一帧收满（靠 40ms 静默收尾）
  char hex[3 * VFRAME_MAX]; frameHex(rxFrame, rxLen, hex);
  Serial.printf("[voice] frame: %s\n", hex);
  ringPos = (uint8_t)((ringPos + 1) % 6);
  snprintf(frameRing[ringPos], sizeof(frameRing[0]), "%s", hex);
  frameRingAt[ringPos] = millis();
  if (learnArmed) {                                             // 学习模式：把这一帧焊死到动作槽
    VSlot* s = &VC.slots[learnArmed - 1];
    s->len = rxLen; memcpy(s->b, rxFrame, rxLen);
    if (!voiceSave()) Serial.println("[voice] save FAILED");
    Serial.printf("[voice] bound slot %u '%s' <- %s\n", learnArmed, VCNAME[learnArmed], hex);
    learnArmed = 0; rxLen = 0; return;
  }
  for (uint8_t i = 0; i < VSLOTS; i++) {
    VSlot* s = &VC.slots[i];
    if (slotFilled(s) && s->len == rxLen && memcmp(s->b, rxFrame, rxLen) == 0) { voiceFire((uint8_t)(i + 1)); break; }
  }
  rxLen = 0;
}
void loopVoice() {
  if (!VC.en) { while (voiceSer.available()) voiceSer.read(); return; }   // 停用：只排空不误触发
  while (voiceSer.available()) {
    uint8_t c = (uint8_t)voiceSer.read();
    uint32_t now = millis();
    if (rxLen && now - rxLastAt > FRAME_GAP_MS) frameDone();
    if (rxLen < VFRAME_MAX) rxFrame[rxLen++] = c;
    else { rxFrame[VFRAME_MAX - 1] = c; }                        // 超长帧：保留尾部继续
    rxLastAt = now;
  }
  uint32_t now = millis();
  if (rxLen && now - rxLastAt > FRAME_GAP_MS) frameDone();
  if (learnArmed && now > learnDeadline) { learnArmed = 0; Serial.println("[voice] learn timeout"); }
}

// ============================================================
// 5. JSON 输出（§8 契约；字符串量小且短命，§12.2 可接受）
// ============================================================
String jb(bool b) { return b ? F("true") : F("false"); }

String statusJson(bool diag = false) {
  int32_t age = lastActAt ? (int32_t)((millis() - lastActAt) / 1000) : -1;
  String s; s.reserve(420);
  s  = "{\"ok\":true,\"online\":true";
  s += ",\"fwVer\":\"" FIRMWARE_VERSION "\"";
  s += ",\"apiVer\":3";
  s += ",\"busy\":" + jb(busy || otaLock || otaGrace);
  s += ",\"state\":\"" + String(stateName()) + "\"";
  s += ",\"grace\":" + jb(otaGrace);
  if (otaGrace) s += ",\"graceLeftSec\":" + String((int)((graceDeadline > millis()) ? (graceDeadline - millis()) / 1000 : 0));
  s += ",\"calL\":" + jb(P.calibrated & (1 << CH_L));
  s += ",\"calR\":" + jb(P.calibrated & (1 << CH_R));
  s += ",\"lastAction\":\"" + String(lastActLabel) + "\"";
  s += ",\"lastActionAgeSec\":" + String(age);
  s += ",\"lastResult\":" + String(lastResult);
  s += ",\"countThisBoot\":" + String(countBoot);
  s += ",\"uptimeSec\":" + String((uint32_t)(millis() / 1000));
  s += ",\"freeHeap\":" + String(ESP.getFreeHeap());
  s += ",\"stationCount\":" + String(WiFi.softAPgetStationNum());
  s += ",\"maxSta\":4";
  s += ",\"voice\":{\"en\":" + jb(VC.en) + ",\"bound\":" + String(boundCount()) + ",\"armed\":" + jb(learnArmed != 0);
  s += ",\"hit\":\"" + String(lastVoiceHit) + "\"}";
  s += ",\"pw\":{\"en\":" + jb(PW.en) + ",\"clock\":" + jb(timeOk) + ",\"inWin\":" + jb(inWindow());
  s += ",\"t\":\"" + hmStr(minuteOfDay()) + "\"";   // v3.0.2 诊断：装置自报"现在几点"，与真实时间对比=时区/漂移一目了然
  s += ",\"win\":\"" + hmStr(PW.winStart) + "~" + hmStr(PW.winEnd) + "\"}";
  s += ",\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
  s += ",\"apChan\":" + String(AP_CHANNEL);
  if (diag) {
    s += ",\"rollbackBoots\":" + String(O.boots);
    s += ",\"otaPending\":" + jb(O.magic == OTA_MAGIC);
    s += ",\"lastErr\":" + String(lastErr);
    s += ",\"sketchSize\":" + String(ESP.getSketchSize());
    s += ",\"updateSpace\":" + String(ESP.getFreeSketchSpace());
    s += ",\"flashSize\":" + String(ESP.getFlashChipRealSize());
    s += ",\"pins\":{\"L\":" + String(SERVO_PINS[CH_L]) + ",\"R\":" + String(SERVO_PINS[CH_R]) + "}";
    s += ",\"voicePins\":{\"rx\":13,\"tx\":12,\"gate\":15}";
  }
  s += "}";
  return s;
}

String paramsJson() {
  String s; s.reserve(420);
  s  = "{\"ok\":true";
  s += ",\"angleOffL\":"  + String(P.angleOff[CH_L]);
  s += ",\"angleOnL\":"   + String(P.angleOn[CH_L]);
  s += ",\"angleHomeL\":" + String(P.angleHome[CH_L]);
  s += ",\"mirrorL\":"    + String((P.mirrorBits >> CH_L) & 1);
  s += ",\"angleOffR\":"  + String(P.angleOff[CH_R]);
  s += ",\"angleOnR\":"   + String(P.angleOn[CH_R]);
  s += ",\"angleHomeR\":" + String(P.angleHome[CH_R]);
  s += ",\"mirrorR\":"    + String((P.mirrorBits >> CH_R) & 1);
  s += ",\"speedMode\":"  + String(P.speedMode);
  s += ",\"holdMs\":"     + String(P.holdMs);
  s += ",\"returnHome\":" + String(P.returnHome);
  s += ",\"calL\":" + jb(P.calibrated & (1 << CH_L));
  s += ",\"calR\":" + jb(P.calibrated & (1 << CH_R));
  s += ",\"effL\":{\"off\":" + String(effOff(CH_L)) + ",\"on\":" + String(effOn(CH_L)) + ",\"home\":" + String(effHome(CH_L)) + "}";
  s += ",\"effR\":{\"off\":" + String(effOff(CH_R)) + ",\"on\":" + String(effOn(CH_R)) + ",\"home\":" + String(effHome(CH_R)) + "}}";
  return s;
}

String errJson(int code) {
  const char* n = "busy";
  switch (code) {
    case 1: n = "out_of_range"; break; case 2: n = "bad_request"; break;
    case 3: n = "angles_conflict"; break; case 4: n = "not_calibrated"; break;
    case 5: n = "params_corrupt"; break; case 6: n = "move_timeout"; break;
    case 8: n = "save_failed"; break; case 9: n = "ota_no_space"; break;
    case 10: n = "ota_image_bad"; break; case 12: n = "ota_grace"; break;
  }
  String s; s.reserve(64);
  s = "{\"ok\":false,\"err\":" + String(code) + ",\"errName\":\"" + String(n) + "\"}";
  return s;
}

// ============================================================
// 6. 页面（PROGMEM，WEB_DESIGN 改订版）
// ============================================================
#include "index_page.h"

// ============================================================
// 7. 请求处理（§8 + §十六：/press 新增 ch=l|r|all）
// ============================================================
bool argInt(const char* name, int& out) {
  if (!server.hasArg(name)) return false;
  out = server.arg(name).toInt();
  return true;
}

inline void touch() { lastHttpMs = millis(); }       // D10：页面活跃时间戳（深睡宽限用）

void handleIndex()   { touch(); server.sendHeader("Cache-Control", "no-store");   // v3.0.2: 禁缓存——防手机留着旧页面 JS（旧页=不带时区的校时戳，tz 修复会被缓存吞掉）
                     server.send_P(200, "text/html", PAGE_HTML); }
void handleFavicon() { server.send(204, "text/plain", ""); }
void handleStatus()  { touch(); server.send(200, "application/json", statusJson()); }

void handleDiag() {
  touch(); server.send(200, "application/json", statusJson(true));
}

void handleGetParams() { server.send(200, "application/json", paramsJson()); }

// press?side=off|on&ch=l|r|all[&trial=1]；ch 缺省 = all（兼容 v1 客户端语义）
void handlePress() {
  if (otaLock)  { server.send(409, "application/json", "{\"ok\":false,\"err\":\"ota\"}"); return; }
  if (otaGrace) { lastErr = 12; server.send(409, "application/json", errJson(12)); return; }
  if (busy)     { server.send(409, "application/json", errJson(11)); return; }        // 409 busy
  String side = server.arg("side");
  String chs  = server.hasArg("ch") ? server.arg("ch") : "all";
  bool trial  = server.hasArg("trial") && server.arg("trial").toInt();
  uint8_t s = (side == "off") ? S_OFF : (side == "on") ? S_ON : S_NONE;
  uint8_t mask = (chs == "l") ? (1 << CH_L) : (chs == "r") ? (1 << CH_R)
                  : (chs == "all") ? (uint8_t)0b11 : 0;
  if (s == S_NONE || mask == 0) { server.send(400, "application/json", errJson(2)); return; }
  const char* label = (mask == 0b11) ? (s == S_ON ? "on-all" : "off-all")
                    : (mask == (1 << CH_L)) ? (s == S_ON ? "on-L" : "off-L")
                    : (s == S_ON ? "on-R" : "off-R");

  bool warnCal = (mask & (uint8_t)~P.calibrated) && !trial;   // 任一请求通道未校准入 E-4 toast
  if (warnCal) {
    if (!startAction(s, mask, label, false)) { server.send(409, "application/json", errJson(11)); return; }
    server.send(200, "application/json", "{\"ok\":true,\"started\":true,\"warn\":\"not_calibrated\",\"busy\":true}");
    return;
  }
  if (!trial) {                                       // 同方向同通道集 5s 去重（§5.1，成功后才盖章）
    uint32_t now = millis();
    uint32_t* ref = &dedupAt[s - 1][mask - 1];
    if (now - *ref < DEDUP_MS) { server.send(200, "application/json", "{\"ok\":true,\"deduped\":true}"); return; }
    *ref = now;
  }
  if (!startAction(s, mask, label, trial)) { server.send(409, "application/json", errJson(11)); return; }
  server.send(200, "application/json",
              trial ? "{\"ok\":true,\"trial\":true,\"busy\":true}"
                    : "{\"ok\":true,\"started\":true,\"busy\":true}");
}

// setParams：任一字段 + dryRun=1 只校验（D-6：试动作不需要保存）
void handleSetParams() {
  if (otaLock)  { server.send(409, "application/json", "{\"ok\":false,\"err\":\"ota\"}"); return; }
  if (otaGrace) { server.send(409, "application/json", errJson(12)); return; }
  if (busy)     { server.send(409, "application/json", errJson(11)); return; }       // §5.3 保存与动作互斥
  Params q = P; int v;
  if (argInt("angleOffL",  v)) q.angleOff[CH_L]  = v;
  if (argInt("angleOnL",   v)) q.angleOn[CH_L]   = v;
  if (argInt("angleHomeL", v)) q.angleHome[CH_L] = v;
  if (argInt("mirrorL",    v)) q.mirrorBits = (q.mirrorBits & ~(1 << CH_L)) | ((v ? 1 : 0) << CH_L);
  if (argInt("angleOffR",  v)) q.angleOff[CH_R]  = v;
  if (argInt("angleOnR",   v)) q.angleOn[CH_R]   = v;
  if (argInt("angleHomeR", v)) q.angleHome[CH_R] = v;
  if (argInt("mirrorR",    v)) q.mirrorBits = (q.mirrorBits & ~(1 << CH_R)) | ((v ? 1 : 0) << CH_R);
  if (argInt("speedMode",  v)) q.speedMode  = v;
  if (argInt("holdMs",     v)) q.holdMs     = v;
  if (argInt("returnHome", v)) q.returnHome = v ? 1 : 0;
  int bad = paramsValidate(&q);
  bool dry = server.hasArg("dryRun") && server.arg("dryRun").toInt();
  if (bad) { server.send(400, "application/json", errJson(bad)); return; }
  if (!dry) {
    q.calibrated = (uint8_t)(q.calibrated | 0b11);   // 保存 = 两通道整体确认（左右各自试过才敢存）
    P = q;
    if (!paramsSave()) { paramsLoad(); server.send(500, "application/json", errJson(8)); return; }
    servoFollowHome();
  } else {
    P = q;               // dryRun：只预览生效角，不落盘、不动作
  }
  server.send(200, "application/json", paramsJson());  // 回读值供页面比对（§十）
}

void handleFactoryReset() {
  if (otaLock) { server.send(409, "application/json", "{\"ok\":false,\"err\":\"ota\"}"); return; }
  if (busy)    { server.send(409, "application/json", errJson(11)); return; }
  paramsDefaults();
  if (!paramsSave()) { server.send(500, "application/json", errJson(8)); return; }
  servoFollowHome();
  server.send(200, "application/json", paramsJson());
}

void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true,\"note\":\"reboot in 1s\"}");
  rebootAt = millis() + 1000;
}

// ---- v3 端点（D9/D10）----
void handleVoice() {                                   // GET：槽位 + 最近抓帧（sniffer 已内化在此）
  String s; s.reserve(520);
  s  = "{\"ok\":true,\"en\":" + jb(VC.en) + ",\"armed\":" + String((int)learnArmed);
  s += ",\"slots\":[";
  for (uint8_t i = 0; i < VSLOTS; i++) {
    VSlot* q = &VC.slots[i];
    char hex[3 * VFRAME_MAX]; hex[0] = 0;
    if (slotFilled(q)) frameHex(q->b, q->len, hex);
    if (i) s += ",";
    s += "{\"code\":" + String(i + 1) + ",\"name\":\"" + String(VCNAME[i + 1]) +
         "\",\"label\":\"" + String(VLBL[i + 1]) + "\",\"hex\":\"" + String(hex) + "\"}";
  }
  s += "],\"frames\":[";
  for (uint8_t k = 0; k < 6; k++) {
    uint8_t idx = (uint8_t)((ringPos + 1 + k) % 6);
    if (k) s += ",";
    s += "{\"hex\":\"" + String(frameRing[idx]) + "\",\"ageSec\":" +
         String((uint32_t)(millis() - frameRingAt[idx]) / 1000) + "}";
  }
  s += "]}";
  server.send(200, "application/json", s);
}
void handleVoiceLearn() {
  int code; if (!argInt("code", code) || code < 1 || code > VSLOTS) { server.send(400, "application/json", errJson(2)); return; }
  learnArmed = (uint8_t)code; learnDeadline = millis() + 15000;
  server.send(200, "application/json", "{\"ok\":true,\"armed\":\"" + String(VCNAME[code]) + "\",\"note\":\"对 SU-03T 唤醒并说该命令一次\"}");
}
void handleVoiceDel() {
  int code; if (!argInt("code", code) || code < 0 || code > VSLOTS) { server.send(400, "application/json", errJson(2)); return; }
  if (code == 0) memset(VC.slots, 0, sizeof(VC.slots));
  else memset(&VC.slots[code - 1], 0, sizeof(VSlot));
  bool ok = voiceSave();
  server.send(200, "application/json", ok ? "{\"ok\":true}" : errJson(8));
}
void handleVoiceEn() {
  int on; if (!argInt("on", on)) { server.send(400, "application/json", errJson(2)); return; }
  VC.en = on ? 1 : 0; voiceSave(); gateUpdate();
  server.send(200, "application/json", "{\"ok\":true,\"en\":" + jb(VC.en) + "}");
}
void handlePwSet() {
  int en; if (argInt("en", en)) PW.en = en ? 1 : 0;
  if (server.hasArg("start")) {
    String a = server.arg("start"); int c = a.indexOf(':');
    if (c >= 1 && c <= 2) { int v = a.substring(0, c).toInt() * 60 + a.substring(c + 1).toInt();
      if (v >= 0 && v <= 1439) PW.winStart = (uint16_t)v; }
  }
  if (server.hasArg("end")) {
    String a = server.arg("end"); int c = a.indexOf(':');
    if (c >= 1 && c <= 2) { int v = a.substring(0, c).toInt() * 60 + a.substring(c + 1).toInt();
      if (v >= 0 && v <= 1439) PW.winEnd = (uint16_t)v; }
  }
  bool ok = pwSave(); lastHttpMs = millis();
  server.send(200, "application/json", "{\"ok\":true,\"en\":" + jb(PW.en) +
    ",\"win\":\"" + hmStr(PW.winStart) + "~" + hmStr(PW.winEnd) + "\"}");
}
void handleTimeSync() {                                 // 手机页面开一次 = 装置时钟校准（无外网 NTP，D10 依赖）
  if (!server.hasArg("ts")) { server.send(400, "application/json", errJson(2)); return; }
  double t = server.arg("ts").toDouble();
  if (t > 1e12) t /= 1000.0;                            // 毫秒兼容
  epochBase = (uint32_t)t; anchorMs = millis(); timeOk = true; rtcPut();
  server.send(200, "application/json", "{\"ok\":true,\"inWin\":" + jb(inWindow()) + "}");
}

void handleNotFound() {                 // v1.1.0：系统联网探测 → 按其期望应答（让 OS 认为"有网"，不踢）
  String p = server.uri(); p.toLowerCase();
  if (p == "/generate_204" || p == "/gen_204") { server.send(204, "text/plain", ""); return; }
  if (p == "/hotspot-detect.html" || p == "/library/test/success.html") {
    server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); return; }
  if (p == "/connecttest.txt") { server.send(200, "text/plain", "Microsoft NCSI"); return; }
  if (p == "/redirect.txt")   { server.send(200, "text/plain", "http://192.168.4.1/"); return; }
  server.sendHeader("Location", "http://192.168.4.1/", true);   // 其余 HTTP 请求 → 送进控制面板（打印机式）
  server.send(302, "text/plain", "");
}

// ============================================================
// 8. OTA：浏览器上传 .bin + 45 秒存活窗（§13 / PRD OA-6）
//    只用确定存在的 API（Update/EEPROM/ESP.restart），不依赖平台回滚
// ============================================================
bool updateOk = false;

void handleOtaPage() { server.send_P(200, "text/html", PAGE_OTA); }

void handleOtaUpload() {                              // 上传回调：流式写备份分区，不整包进 RAM
  HTTPUpload& u = server.upload();                    // 核心 3.1.2：HTTPUpload（旧示例里的 Upload 已不存在）
  if (u.status == UPLOAD_FILE_START) {
    otaLock = true; updateOk = false;
    servoOff();                                       // §12.1：写 flash 前必须先停舵机 PWM
    Serial.printf("[ota] start '%s' heap=%u space=%u\n",
                  u.filename.c_str(), ESP.getFreeHeap(), ESP.getFreeSketchSpace());
    if (!Update.begin(ESP.getFreeSketchSpace(), U_FLASH)) Update.printError(Serial);  // 3.x 无 UPDATE_SIZE_UNKNOWN；end(true) 按实收字节收尾
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.write(u.buf, u.currentSize) != (size_t)u.currentSize) {
      Update.printError(Serial);        // 不 abort：让 END 里的 end(true) 统一失败收尾，旧固件保持
    }
  } else if (u.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      // 关键一步：先落 OTA_MAGIC 到 EEPROM 再重启。新固件起来后 45 秒内存活确认；
      // 若它崩在 45 秒内，重启后仍带着"待观察"状态 → 页面拒绝动作但保 HTTP，
      // 用户可直接在 /ota 重传上一版 .bin（写回另一槽），无需拆机。
      O.magic = OTA_MAGIC; O.boots = 0;
      otaStateSave();
      updateOk = true;
      Serial.printf("[ota] wrote %u bytes, slot switched\n", (unsigned)u.totalSize);
    } else {
      Update.printError(Serial);
      Serial.println("[ota] failed, old firmware still running");
    }
    otaLock = false;
  }
}

void handleOtaDone() {
  if (updateOk) { server.send(200, "text/plain", "ok, rebooting"); rebootAt = millis() + 500; }
  else          { server.send(500, "text/plain", "update failed, old firmware still running"); }
  updateOk = false;
}

// 启动时判断"我是不是刚被 OTA 换上来的新固件"（§13.3）
void otaGraceBeginOrCheck() {
  otaStateLoad();
  if (O.magic == OTA_MAGIC) {                          // 新固件的早期启动
    O.boots++;
    otaStateSave();
    otaGrace = true;
    graceDeadline = millis() + OTA_GRACE_MS;
    Serial.printf("[ota] grace window ON (boot #%u). servo locked for 45s\n", O.boots);
  } else if (O.magic == OTA_CONFIRM) {
    O.magic = 0; O.boots = 0; otaStateSave();          // 清历史标记
  }
}

void otaGraceTick() {
  if (!otaGrace) return;
  if (millis() >= graceDeadline) {                     // 活过 45 秒 → 确认，解锁舵机
    otaGrace = false;
    O.magic = OTA_CONFIRM; otaStateSave();
    Serial.println("[ota] grace passed, firmware confirmed, servo unlocked");
    blinkLed(2);
  }
}

void blinkLed(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_PIN, LOW); delay(80);
    digitalWrite(LED_PIN, HIGH); delay(120);
  }
}

// ============================================================
// 9. setup / loop
// ============================================================
void setup() {
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH);      // 灭 = 正常
  pinMode(SERVO_PINS[CH_L], INPUT);                            // 启动阶段绝不驱动信号脚（§4）
  pinMode(SERVO_PINS[CH_R], INPUT);
  Serial.begin(115200);
  Serial.println("\n=== dorm light-off device " FIRMWARE_VERSION " (dual + voice + night) ===");

  EEPROM.begin(EEPROM_SIZE);
  bool loaded = paramsLoad();
  Serial.printf("[cfg] params %s\n", loaded ? "loaded" : "corrupt/old-version/first-run -> defaults");
  if (!loaded) lastErr = 5;
  bool vFresh = voiceLoad(), pFresh = pwLoad();
  rtcGet();
  Serial.printf("[cfg] voice %s pw %s clock %s (win %s~%s)\n",
                vFresh ? "loaded" : "new-defaults", pFresh ? "loaded" : "new-defaults",
                timeOk ? "kept" : "lost(全断电)", hmStr(PW.winStart).c_str(), hmStr(PW.winEnd).c_str());
  pinMode(VOICE_GATE, OUTPUT); digitalWrite(VOICE_GATE, LOW);
  otaGraceBeginOrCheck();

  // D10 快睡路径：定时醒来看表，不在窗口就立刻回去——不开 AP、不起语音，省一整段启动
  if (PW.en && timeOk && !inWindow() && !otaGrace) gotoSleep();   // 不返回
  gateUpdate();
  if (VC.en) { voiceSer.begin(VOICE_BAUD); Serial.println("[voice] SU-03T uart @9600 RX=D7 TX=idle-D6"); }

  // 承载层：纯 AP（§10）
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, 0, 4);             // 上限 4 台（OA-5）
  WiFi.setOutputPower(10.5);       // v3 续航刀：发射功率 20.5→10.5dBm，3 米覆盖绰绰有余，均流省 ~20-30mA
  WiFi.setSleep(false);                                        // AP 下关 modem sleep，否则响应抖
  dnsServer.setTTL(30);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", WiFi.softAPIP());                   // 所有域名→192.168.4.1：喂饱 OS 探测（D7）
  Serial.printf("[net] AP '%s' ch=%d ip=%s\n", AP_SSID, AP_CHANNEL, WiFi.softAPIP().toString().c_str());

  if (MDNS.begin(HOSTNAME)) { MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.println("[net] mdns guangdeng.local (bonus only)"); }

  server.on("/", HTTP_GET, handleIndex);
  server.on("/favicon.ico", HTTP_GET, handleFavicon);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/diag", HTTP_GET, handleDiag);
  server.on("/press", HTTP_POST, handlePress);
  server.on("/params", HTTP_GET, handleGetParams);
  server.on("/setParams", HTTP_POST, handleSetParams);
  server.on("/factoryReset", HTTP_POST, handleFactoryReset);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/voice", HTTP_GET, handleVoice);
  server.on("/voiceLearn", HTTP_POST, handleVoiceLearn);
  server.on("/voiceDel", HTTP_POST, handleVoiceDel);
  server.on("/voiceEn", HTTP_POST, handleVoiceEn);
  server.on("/pwSet", HTTP_POST, handlePwSet);
  server.on("/time", HTTP_POST, handleTimeSync);
  server.on("/ota", HTTP_GET, handleOtaPage);
  server.on("/update", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleNotFound);
  server.begin();

  if (!otaGrace) servoFollowHome();                    // 观察窗内舵机保持锁定（§13.3）

  blinkLed(1);                                          // 就绪短闪
  Serial.printf("[boot] ready. freeHeap=%u\n", ESP.getFreeHeap());
}

void loop() {
  server.handleClient();                               // 单 client 串行（§5.4）
  dnsServer.processNextRequest();                      // 每轮最多 1 个 DNS 包，µs 级
  loopAction();
  loopVoice();                                         // D9：帧接收/学习绑定（全非阻塞）
  loopPower();                                         // D10：窗口检查（2s 节拍）
  otaGraceTick();
  MDNS.update();

  uint32_t now = millis();
  if (settleOffAt && now >= settleOffAt) { settleOffAt = 0; servoOff(); }
  if (rebootAt && now >= rebootAt)       { rebootAt = 0; ESP.restart(); }
}
