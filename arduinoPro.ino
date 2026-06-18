#include <LiquidCrystal.h>
#include <Servo.h>
#include <WiFiS3.h>


LiquidCrystal lcd(3,4,5,6,7,8);

#define BUZZER_PIN 2
#define TRIG_PIN 10
#define ECHO_PIN 9
#define SERVO_X_PIN 11
#define SERVO_Y_PIN 12
#define RELAY_PIN 13

#define LDR_TOP A2
#define LDR_BOTTOM A3
#define LDR_LEFT A4
#define LDR_RIGHT A5

Servo servoX;
Servo servoY;

char ssid[] = "WIFI_SSID"; //와이파이 이름 입력
char pass[] = "WIFI_PASS"; // 와이파이 비밀번호 입력

WiFiServer server(80);

enum State { WAIT_USER, STUDYING, BREAK_TIME };
State currentState = WAIT_USER;

unsigned long studyDuration = 30000UL;   // 30초
unsigned long breakDuration = 10000UL;   // 10초

unsigned long studyStartTime = 0;
unsigned long breakStartTime = 0;
unsigned long pausedTimeTotal = 0;
unsigned long pauseStartTime = 0;

unsigned long lastDistanceCheck = 0;
unsigned long lastTracking = 0;
unsigned long userLeaveTime = 0;

bool userPresent = false;
bool timerPaused = false;

const unsigned long leaveLimit = 10000UL;

int servoXPos = 90;
int servoYPos = 90;

long getDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if(duration == 0) return 999;

  return duration * 0.034 / 2;
}

void playAlarm()
{
  for(int i=0;i<3;i++)
  {
    tone(BUZZER_PIN,1000);
    delay(300);
    noTone(BUZZER_PIN);
    delay(300);
  }
}

void ledOn(){ digitalWrite(RELAY_PIN,HIGH); }
void ledOff(){ digitalWrite(RELAY_PIN,LOW); }

void resetStudy()
{
  ledOff();
  servoX.write(90);
  servoY.write(90);

  servoXPos = 90;
  servoYPos = 90;

  pausedTimeTotal = 0;
  timerPaused = false;
  userPresent = false;

  lcd.clear();
  lcd.print("Waiting User");

  currentState = WAIT_USER;
}

void tracking()
{
  int top = analogRead(LDR_TOP);
  int bottom = analogRead(LDR_BOTTOM);
  int left = analogRead(LDR_LEFT);
  int right = analogRead(LDR_RIGHT);

  int avg = (top+bottom+left+right)/4;

  int dx = (left-avg) - (right-avg);
  int dy = (top-avg) - (bottom-avg);

  if(dx > 100) servoXPos--;
  else if(dx < -100) servoXPos++;

  if(dy > 100) servoYPos++;
  else if(dy < -100) servoYPos--;

  servoXPos = constrain(servoXPos,0,180);
  servoYPos = constrain(servoYPos,0,180);

  servoX.write(servoXPos);
  servoY.write(servoYPos);
}

void setup()
{
  WiFi.begin(ssid, pass);
  Serial.begin(115200);

  while(WiFi.status() != WL_CONNECTED)
{
    Serial.println("Connecting...");
    delay(1000);
}

  server.begin();
  Serial.println(WiFi.localIP());

  Serial.begin(115200);
  delay(3000);

  Serial.println("Start");

  int status = WiFi.begin(ssid, pass);

  Serial.print("Begin = ");
  Serial.println(status);

  delay(10000);

  Serial.print("Status = ");
  Serial.println(WiFi.status());

  Serial.print("SSID = ");
  Serial.println(WiFi.SSID());

  Serial.print("RSSI = ");
  Serial.println(WiFi.RSSI());

  Serial.print("IP = ");
  Serial.println(WiFi.localIP());

  // 웹 서버 시작
  server.begin();
  Serial.println("Web Server Started");

  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);
  pinMode(RELAY_PIN,OUTPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  servoX.attach(SERVO_X_PIN);
  servoY.attach(SERVO_Y_PIN);

  lcd.begin(16,2);
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
}

void loop()
{
  WiFiClient client = server.available();

if (client)
{
  WiFiClient client = server.available();
    while(client.connected() && !client.available())
{
    delay(1);
}

        String request = client.readStringUntil('\n');

        Serial.println(request);

        int dataIndex =
            request.indexOf("data=");

        if(dataIndex != -1)
        {
            int start = dataIndex + 5;
            int end =
                request.indexOf(' ', start);

            String receivedData =
                request.substring(start,end);

            studyDuration =
                receivedData.toInt() * 1000UL;
        }

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain");
        client.println();
        client.println("OK");

        client.stop();
    }


  unsigned long now = millis();

  switch(currentState)
  {
    case WAIT_USER:

      if(now - lastDistanceCheck >= 500)
      {
        lastDistanceCheck = now;

        if(getDistance() < 80)
        {
          lcd.clear();
          lcd.print("User Detected");

          ledOn();

          studyStartTime = millis();
          pausedTimeTotal = 0;
          timerPaused = false;
          userPresent = true;

          currentState = STUDYING;
        }
      }
      break;

    case STUDYING:

      if(now - lastDistanceCheck >= 1000)
      {
        lastDistanceCheck = now;

        long distance = getDistance();

        if(distance >= 80)
        {
          if(userPresent)
          {
            userPresent = false;
            userLeaveTime = millis();

            timerPaused = true;
            pauseStartTime = millis();

            lcd.clear();
            lcd.print("User Away");
          }

          if(millis() - userLeaveTime > leaveLimit)
          {
            resetStudy();
            break;
          }
        }
        else
        {
          if(!userPresent)
          {
            userPresent = true;

            if(timerPaused)
            {
              pausedTimeTotal += millis() - pauseStartTime;
              timerPaused = false;
            }

            lcd.clear();
          }
        }
      }

      if(!timerPaused)
      {
        if(now - lastTracking >= 100)
        {
          lastTracking = now;
          tracking();
        }

        unsigned long effectiveElapsed =
          now - studyStartTime - pausedTimeTotal;

        unsigned long remainStudy = 0;

        if(effectiveElapsed < studyDuration)
          remainStudy =
            (studyDuration - effectiveElapsed) / 1000;

        lcd.setCursor(0,0);
        lcd.print("Studying      ");

        lcd.setCursor(0,1);
        int m = remainStudy / 60;
        int s = remainStudy % 60;

        if(m < 10) lcd.print("0");
        lcd.print(m);
        lcd.print(":");
        if(s < 10) lcd.print("0");
        lcd.print(s);
        lcd.print("   ");

        if(effectiveElapsed >= studyDuration)
        {
          playAlarm();
          ledOff();

          breakStartTime = millis();

          lcd.clear();
          lcd.print("Break Time");

          currentState = BREAK_TIME;
        }
      }
      break;

    case BREAK_TIME:

      lcd.setCursor(0,0);
      lcd.print("Take Break    ");

      unsigned long remainBreak = 0;

      if(now - breakStartTime < breakDuration)
        remainBreak =
          (breakDuration - (now - breakStartTime))/1000;

      lcd.setCursor(0,1);

      int bm = remainBreak / 60;
      int bs = remainBreak % 60;

      if(bm < 10) lcd.print("0");
      lcd.print(bm);
      lcd.print(":");
      if(bs < 10) lcd.print("0");
      lcd.print(bs);
      lcd.print("   ");

      if(now - breakStartTime >= breakDuration)
      {
        lcd.clear();
        lcd.print("Break End");
        delay(1000);

        currentState = WAIT_USER;
      }
      break;
  }
}
