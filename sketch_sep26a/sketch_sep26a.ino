#include<NewPing.h>
#include<Servo.h>
//our L298N control pins
const int LeftMotorForward = 7;
const int LeftMotorBackward = 6;
const int RightMotorForward = 4;
const int RightMotorBackward = 5;

//sensor pins
#define trig_pin A1 //analog input 1
#define echo_pin A2 //analog input 2
Servo myservo;
int distance=-1;
NewPing sonar(trig_pin, echo_pin, 100); 
void setup() {
pinMode(LeftMotorForward,OUTPUT);
pinMode(LeftMotorBackward,OUTPUT);
pinMode(RightMotorForward,OUTPUT);
pinMode(RightMotorBackward,OUTPUT);
pinMode(trig_pin,OUTPUT);
pinMode(echo_pin,INPUT);
myservo.attach(10);
myservo.write(90);
int max;
myservo.write(100);
distance =readPing();
delay(200);
myservo.write(90);
}
void loop() {
  int dleft=0,dright=0;
 if(distance>25){

  moveforward();
    distance=readPing();
 }
if(distance<10){

  stop();
  movebackward();
  stop();
  delay(200);
 float dright= lookright();
  float dleft=lookleft();
  if(dright>dleft){
moveright();
delay(200);
   distance=readPing();
  }
  else if(dright<dleft){
    moveleft();
    delay(200);
       distance=readPing();
  }

}

}
float readPing(){
  delay(70);
  float cm = sonar.ping_cm();
  if (cm==0){
    cm=250;
  }
  return cm;
}
void movebackward(){
   digitalWrite(LeftMotorForward,LOW);

digitalWrite(LeftMotorBackward,HIGH);
digitalWrite(RightMotorForward,LOW);
digitalWrite(RightMotorBackward,HIGH);
}
void moveforward(){
  digitalWrite(LeftMotorForward,HIGH);

digitalWrite(LeftMotorBackward,LOW);
digitalWrite(RightMotorForward,HIGH);
digitalWrite(RightMotorBackward,LOW);
}
void stop(){
   digitalWrite(LeftMotorForward,LOW);

digitalWrite(LeftMotorBackward,LOW);
digitalWrite(RightMotorForward,LOW);
digitalWrite(RightMotorBackward,LOW);
}
void moveright(){
     digitalWrite(LeftMotorForward,HIGH);

digitalWrite(LeftMotorBackward,LOW);
digitalWrite(RightMotorForward,LOW);
digitalWrite(RightMotorBackward,HIGH);
}
void moveleft(){
     digitalWrite(LeftMotorForward,LOW);

digitalWrite(LeftMotorBackward,HIGH);
digitalWrite(RightMotorForward,HIGH);
digitalWrite(RightMotorBackward,LOW);
}
float lookright(){
  float dright=-1;
  for(int i=90;i>=0;i--){
  myservo.write(i);
float max=readPing();

if(dright<max){
  dright=max;
}

}
delay(200);
myservo.write(90);
return dright;
}
float lookleft(){
  float dleft=-1;
  for(int i=0;i<=180;i++){
  myservo.write(i);
float max=readPing();

if(dleft<max){
  dleft=max;
}
}
delay(200);
myservo.write(90);
return dleft;
}