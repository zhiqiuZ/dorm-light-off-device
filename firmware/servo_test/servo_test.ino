// servo_test.ino — 信号路径最小独立测试（与 dorm_light 完全无关）
// 依次在 D2(GPIO4) → D1(GPIO5) → D5(GPIO14) 三个脚上扫一轮 45°→135°，再循环。
// 用法：舵机信号线先插在 D2 别动，盯住舵机盘/用手摸着轴：
//   - 哪个 GPIO 扫的时候它抖了 → 板上丝印与真实 GPIO 错位 / 原孔虚断
//   - 三个脚全程都没反应 → 不是软件问题（查 5V 孔是否真是 5V、或直接换舵机）
#include <Servo.h>

Servo s;
const int  PINS[]   = {4, 5, 14};
const char NAMES[][16] = {"D2 (GPIO4)", "D1 (GPIO5)", "D5 (GPIO14)"};
int i = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("=== servo pin sweep test: watch/touch the horn ===");
}

void loop() {
  Serial.printf("sweeping %s : 45 -> 135 ...\n", NAMES[i]);
  s.attach(PINS[i], 544, 2400);
  delay(60);
  s.write(45);  delay(800);
  s.write(135); delay(800);
  s.detach();
  delay(700);
  i = (i + 1) % 3;
}
