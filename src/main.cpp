/*
남박사 로봇학원 반응속도 테스터 코드
@author: 장준혁
@date: 2026-05-16
@description: 인터럽트와 millis()를 활용하여 0.5초 이내의 반응속도까지
              정밀하게 측정 가능한 논블로킹(Non-blocking) 반응속도 테스터 코드입니다.
@version: 1.0
@license: Beer License
*/

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const int LED_PIN = 3;    // Common anode LED 사용. LOW로 켜짐
const int BUTTON_PIN = 2; // 우노에서 2번 핀은 외부 인터럽트(INT0) 지원
const int BuzzerPin = 4;
const int MOTOR_PIN = 5;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// 상태 정의를 위한 열거형(Enum)
enum State
{
  READY,      // 랜덤 대기 상태
  CUE_ON,     // 신호 발생 상태
  SHOW_RESULT // 결과 출력 및 재시작 대기 상태
};

// 전역 변수 설정
State currentState = READY;
unsigned long waitStartTime = 0;
unsigned long randomWaitTime = 0;
unsigned long cueStartTime = 0;
int selectedCue = 0; // 0: LED, 1: 부저, 2: 모터
volatile unsigned long signalOnTime = 0;
volatile unsigned long buttonPressTime = 0;
volatile bool isReacted = false;

/**
 * LCD 초기화 함수
 * - LCD를 초기화하고, 초기 메시지를 출력합니다.
 * - 이 함수는 setup()에서 한 번만 호출됩니다.
 * - 초기 메시지는 "Reaction Test"와 "Get Ready..."로 설정되어 있습니다. 필요에 따라 변경 가능합니다.
 * - LCD 초기화 후 백라이트를 켜고, 커서를 초기 위치(0, 0)로 설정합니다.
 * - LCD 초기화에 실패할 경우, 시리얼 모니터에 오류 메시지를 출력하도록 개선할 수 있습니다.
 * @warning LCD 주소(0x27)와 크기(16x2)는 사용 중인 LCD에 맞게 조정해야 합니다.
 * @return void
 */
void initializeLCD()
{
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Reaction Test");
  lcd.setCursor(0, 1);
  lcd.print("Get Ready...");
}

/**
 * 신호 켜는 함수
 * - cueType에 따라 LED, 부저, 모터 중 하나를 켭니다
 * - cueType이 0이면 LED를 켜고, 1이면 부저를 켜며, 2이면 모터를 켭니다.
 * - 이 함수는 loop()에서 신호를 발생시킬 때 호출됩니다.
 * - 신호를 켤 때마다 현재 시간을 signalOnTime에 저장하여, 인터럽트 서비스 루틴에서 반응 속도를 계산할 수 있도록 합니다.
 * - 신호 켜는 방식은 간단히 digitalWrite와 tone 함수를 사용하지만, 필요에 따라 PWM 제어나 다른 방식으로 확장할 수 있습니다.
 * @warning cueType이 0, 1, 2 이외의 값이 들어올 경우 아무 신호도 켜지지 않도록 되어 있습니다. 입력값 검증을 추가할 수 있습니다.
 * @return void
 */
void turnOnCue(int cueType)
{
  if (cueType == 0)
  {
    digitalWrite(LED_PIN, LOW);
  }
  else if (cueType == 1)
  {
    tone(BuzzerPin, 1000); // 1000Hz 소리 발생
  }
  else if (cueType == 2)
  {
    digitalWrite(MOTOR_PIN, HIGH);
  }
}

/**
 * 모든 신호를 끄는 함수
 * - LED, 부저, 모터의 신호를 모두 끕니다.
 * - 이 함수는 loop()에서 신호를 종료할 때 호출됩니다.
 * @return void
 */
void turnOffAllCues()
{
  digitalWrite(LED_PIN, HIGH);
  noTone(BuzzerPin);
  digitalWrite(MOTOR_PIN, LOW);
}

/**
 * 버튼이 눌렸을 때 실행될 인터럽트 서비스 루틴 (ISR)
 * - 이 함수는 버튼이 눌릴 때 자동으로 호출됩니다.
 * - 신호가 켜진 상태(CUE_ON)이고, 아직 이번 턴에 반응 기록이 없을 때만 실행됩니다.
 * @return void
 */
void buttonISR()
{
  // 신호가 켜진 상태(CUE_ON)이고, 아직 이번 턴에 반응 기록이 없을 때만 실행
  if (currentState == CUE_ON && !isReacted)
  {
    buttonPressTime = millis();
    isReacted = true;
  }
  else if (currentState != CUE_ON && !isReacted)
  {
    // 신호가 켜지기 전에 버튼이 눌렸다면, 패널티
    buttonPressTime = signalOnTime - 1;
    isReacted = true;
  }
}

void setup()
{
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  initializeLCD();

  // 1. 아날로그 0번 핀의 플로팅 전압을 이용해 랜덤 시드 초기화
  randomSeed(analogRead(A0));

  // 3. 버튼 핀에 인터럽트 설정
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, RISING);

  // 첫 번째 대기 시간 설정 (2초 ~ 5초 사이의 랜덤한 시간)
  randomWaitTime = random(2000, 5000);
  waitStartTime = millis();
}

void loop()
{
  unsigned long currentMillis = millis();

  switch (currentState)
  {

  case READY:
    // 랜덤하게 정해진 대기 시간이 지나면 신호를 발생시킴
    if (currentMillis - waitStartTime >= randomWaitTime)
    {
      isReacted = false;          // 반응 플래그 초기화
      selectedCue = random(0, 3); // 0, 1, 2 중 하나를 랜덤 선택

      turnOnCue(selectedCue);       // 신호 켜기
      cueStartTime = currentMillis; // 신호가 켜진 기점 저장
      signalOnTime = currentMillis; // ISR과 공유할 시간 저장

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! PUSH !!!");
      Serial.println("PUSH!!"); // 디버깅용 시리얼 출력

      currentState = CUE_ON; // 상태를 신호 발생 상태로 전환
    }
    break;

  case CUE_ON:
    // 사용자가 버튼을 눌렀다면 (인터럽트 발생)
    if (isReacted)
    {
      turnOffAllCues(); // 즉시 신호 종료
      currentState = SHOW_RESULT;
    }
    // 사용자가 0.5초(500ms) 동안 누르지 않았다면 자동 종료 처리
    else if (currentMillis - cueStartTime >= 500)
    {
      turnOffAllCues(); // 0.5초 경과 후 신호 종료

      // 0.5초가 지났으나 사용자가 아직 누르지 않은 경우, 계속 누르기를 기다림
      // (단, 출력은 꺼진 상태로 반응 속도만 계속 측정)
      if (isReacted)
      {
        currentState = SHOW_RESULT;
      }
    }
    break;

  case SHOW_RESULT:
    // 반응 속도 계산
    long reactionTime = buttonPressTime - signalOnTime;

    if (reactionTime < 0)
    {
      reactionTime = 999; // 패널티로 999ms 설정
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too Early!");
      Serial.println("Too Early!"); // 디버깅용 시리얼 출력
    }
    else
    {
      // LCD 및 시리얼 모니터 출력
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Result:");
      lcd.setCursor(0, 1);
      lcd.print(reactionTime);
      lcd.print(" ms");

      Serial.print("Reaction Time: ");
      Serial.print(reactionTime);
      Serial.println(" ms");
    }

    // 다음 게임을 위한 준비 대기 (결과를 볼 수 있도록 3초간 block 해도 무방한 구간)
    delay(3000);

    // 다음 테스트를 위한 세팅 및 READY 상태로 전환
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reaction Test");
    lcd.setCursor(0, 1);
    lcd.print("Get Ready...");

    Serial.println("Get Ready..."); // 디버깅용 시리얼 출력

    randomWaitTime = random(2000, 5000); // 새로운 랜덤 대기시간 설정
    waitStartTime = millis();
    currentState = READY;
    break;
  }
}