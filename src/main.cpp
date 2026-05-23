/*
남박사 로봇학원 반응속도 테스터 코드 (파이썬 CSV 연동 버전)
@author: 장준혁
@date: 2026-05-16 (Update: 2026-05-23)
@description: 인터럽트와 millis()를 활용한 논블로킹 반응속도 테스터 코드입니다.
              시리얼 출력을 파이썬 툴 파싱용 포맷(SENSE,MS)으로 변경했습니다.
*/

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const int LED_PIN = 3;    // Common anode LED 사용. LOW로 켜짐
const int BUTTON_PIN = 2; // 우노에서 2번 핀은 외부 인터럽트(INT0) 지원
const int BuzzerPin = 4;
const int MOTOR_PIN = 5;

LiquidCrystal_I2C lcd(0x27, 16, 2);

enum State
{
  READY,      // 랜덤 대기 상태
  CUE_ON,     // 신호 발생 상태
  SHOW_RESULT // 결과 출력 및 재시작 대기 상태
};

State currentState = READY;
unsigned long waitStartTime = 0;
unsigned long randomWaitTime = 0;
unsigned long cueStartTime = 0;
int selectedCue = 0; // 0: VISUAL, 1: AUDIO, 2: VIBRATE
volatile unsigned long signalOnTime = 0;
volatile unsigned long buttonPressTime = 0;
volatile bool isReacted = false;

void initializeLCD()
{
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Reaction Test");
  lcd.setCursor(0, 1);
  lcd.print("Get Ready...");
}

void turnOnCue(int cueType)
{
  if (cueType == 0)
  {
    digitalWrite(LED_PIN, LOW);
  }
  else if (cueType == 1)
  {
    tone(BuzzerPin, 1000); 
  }
  else if (cueType == 2)
  {
    digitalWrite(MOTOR_PIN, HIGH);
  }
}

void turnOffAllCues()
{
  digitalWrite(LED_PIN, HIGH);
  noTone(BuzzerPin);
  digitalWrite(MOTOR_PIN, LOW);
}

void buttonISR()
{
  if (currentState == CUE_ON && !isReacted)
  {
    buttonPressTime = millis();
    isReacted = true;
  }
  else if (currentState != CUE_ON && !isReacted)
  {
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

  randomSeed(analogRead(A0));
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, RISING);

  randomWaitTime = random(2000, 5000);
  waitStartTime = millis();
}

void loop()
{
  unsigned long currentMillis = millis();

  switch (currentState)
  {
  case READY:
    if (currentMillis - waitStartTime >= randomWaitTime)
    {
      isReacted = false;          
      selectedCue = random(0, 3); 

      turnOnCue(selectedCue);       
      cueStartTime = currentMillis; 
      signalOnTime = currentMillis; 

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! PUSH !!!");

      currentState = CUE_ON; 
    }
    break;

  case CUE_ON:
    if (isReacted)
    {
      turnOffAllCues(); 
      currentState = SHOW_RESULT;
    }
    else if (currentMillis - cueStartTime >= 500)
    {
      turnOffAllCues(); 
      if (isReacted)
      {
        currentState = SHOW_RESULT;
      }
    }
    break;

  case SHOW_RESULT:
    long reactionTime = buttonPressTime - signalOnTime;

    // 1. 패널티(조기 입력) 처리 시 시리얼 출력
    if (reactionTime < 0)
    {
      reactionTime = 999; 
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Too Early!");
      
      // 감각 종류에 따른 문자열 치환 후 CSV용 출력
      if (selectedCue == 0) Serial.print("VISUAL,");
      else if (selectedCue == 1) Serial.print("AUDIO,");
      else if (selectedCue == 2) Serial.print("VIBRATE,");
      Serial.println(reactionTime);
    }
    // 2. 정상 반응 속도 측정 성공 시 시리얼 출력
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Result:");
      lcd.setCursor(0, 1);
      lcd.print(reactionTime);
      lcd.print(" ms");

      // 감각 종류에 따른 문자열 치환 후 CSV용 출력 (조건 2, 3 매핑 완료)
      if (selectedCue == 0) Serial.print("VISUAL,");
      else if (selectedCue == 1) Serial.print("AUDIO,");
      else if (selectedCue == 2) Serial.print("VIBRATE,");
      Serial.println(reactionTime);
    }

    delay(3000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reaction Test");
    lcd.setCursor(0, 1);
    lcd.print("Get Ready...");

    Serial.println("Get Ready..."); // 파이썬 툴에서는 무시됨

    randomWaitTime = random(2000, 5000); 
    waitStartTime = millis();
    currentState = READY;
    break;
  }
}