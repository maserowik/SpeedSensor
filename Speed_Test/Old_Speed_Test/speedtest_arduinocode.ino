int analogPin1=0;
int analogPin2=1;
int reading1=0;
int reading2=0;
int readingMax=1023;
double sensorDistance=48.0;
double detectionRange = readingMax/2.0;
int senseDir = 0; //1 for pin1to2, -1 for pin2to1, 0 neutral
double startTime,endTime;
void setup() {
  Serial.begin(9600);
}

void loop() {
  reading1=analogRead(analogPin1);
  reading2=analogRead(analogPin2);
  if(reading1<=detectionRange && senseDir==0) {
    startTime=millis();
    senseDir=1;
  } else if(reading1<=detectionRange && senseDir==-1){
    endTime=millis();
    displaySpeed(startTime,endTime);
    senseDir=0;
  }

  if(reading2<=detectionRange && senseDir==0) {
    startTime=millis();
    senseDir=-1;
  } else if(reading2<=detectionRange && senseDir==1) {
    endTime=millis();
    displaySpeed(startTime,endTime);
    senseDir=0; 
  }
}

void displaySpeed(double startTime,double endTime) {
  float speed = (sensorDistance)/(endTime-startTime);
  speed = speed *1000.000;
  //Serial.println(speed);
  if(speed>=0 && (endTime-startTime>0.00001)) {
    Serial.println("!S "+String(speed/39.3701,3));
  }
  startTime=0;
  endTime=0;
  delay(5000);
}
