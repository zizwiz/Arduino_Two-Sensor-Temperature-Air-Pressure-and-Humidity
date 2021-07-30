#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_BME280.h>
#include <DS3231.h>
#include <SPI.h>
#include <SD.h>

#define DHTPIN1 3 //define the pins we will use to get the temperature data from.
#define DHTTYPE DHT22
DHT dht(DHTPIN1, DHTTYPE);

DS3231  rtc(SDA, SCL);
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 20 chars and 4 line display
Adafruit_BME280 bme;

Time t;
File DataFile;

char SerialInput = '0';

String output = "";
String Filename = "datalog.csv";
String MyTime2 = "";
String MyDST = "";

float humidity[2];
float temperature[2];
float pressure[1];
float PreviousValue = 0;

int graph[20];

// We need this so Arduino will actually count a minute.
unsigned long counter = -1;
unsigned long count = 0;

unsigned long seconds = 1000L; //Notice the L
unsigned long minute = seconds * 60;
int iterations = 59L;

//Define Custom Characters
byte up[] = {B11111, B11111, B00000, B00000, B00000, B00000, B00000, B00000};
byte even[] = {B00000, B00000, B00000, B11111, B11111, B00000, B00000, B00000};
byte down[] = {B00000, B00000, B00000, B00000, B00000, B00000, B11111, B11111};

void setup()
{
  lcd.init();  // initialise the class
  lcd.backlight(); //switch on the backlight

  Serial.begin(9600);  //open the RS232 port at 9600 baud
  dht.begin();
  rtc.begin();

  if (!bme.begin(0x76)) {
    Serial.println(F("BME280 initialisation failed"));
    while (1);
  }

  if (!SD.begin(10)) {
    Serial.println(F("SD initialisation failed"));  // Chip select pin for us is pin 10. Chamge this if you use a different pin.
    while (1);
  }

  Serial.println(F("All Initialised OK."));

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  DataFile = SD.open(Filename, FILE_WRITE);

  // if the file is available, write to it:
  if (DataFile) {
    DataFile.println(F("Date,Time,ITemp,IHumidity,OTemp,OHumidity,Pressure"));
    DataFile.close();
    // print to the serial port too:
    Serial.println(F("Date,Time,ITemp,IHumidity,OTemp,OHumidity,Pressure"));
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }


  //Write the non-changing data
  lcd.setCursor(0, 0);
  lcd.print(F("I"));
  lcd.setCursor(0, 1);
  lcd.print(F("O"));
  lcd.setCursor(0, 2);
  lcd.print(F("O"));

  // create custom chars
  lcd.createChar(1, up);
  lcd.createChar(2, even);
  lcd.createChar(3, down);

  memset(graph, 0, sizeof(graph)); //fill with 0
}

void loop()
{

  output = "";
  //Read the inside sensor data
  temperature[0] = dht.readTemperature(); // Gets the values of the temperature
  humidity[0] = dht.readHumidity(); // Gets the values of the humidity

  //   Read the outside sensor data
  temperature[1] = bme.readTemperature();
  humidity[1] = bme.readHumidity();
  pressure[0] = bme.readPressure() / 100.0F;

  //Read RTC and check UTC
  MyTime2 = rtc.getTimeStr(); //winter time UTC time

  if (!AreWeInUTC())
  {
    //If Summer time we add an hour UTC+1
    MyDST  = MyTime2.substring(2);
    MyTime2 = ((MyTime2.substring(0, 2)).toInt()) + 1;
    if (MyTime2.toInt() < 10)
    { // add leading zero
      MyTime2 = "0" + (String)MyTime2 + MyDST;
    }
    else
    {
      MyTime2 = (String)MyTime2 + MyDST;
    }
  }

  //Line 0 = inside
  lcd.setCursor(1, 0);
  lcd.print(F(" "));
  lcd.print(temperature[0]);
  lcd.print((char)223);
  lcd.print(F("C "));
  lcd.setCursor(13, 0);
  lcd.print(humidity[0]);
  lcd.print(F("%"));

  //line 2 and 3 = outside
  lcd.setCursor(1, 1);
  lcd.print(F(" "));
  lcd.print(temperature[1]);
  lcd.print((char)223);
  lcd.print(F("C "));
  lcd.setCursor(13, 1);
  lcd.print(humidity[1]);
  lcd.print(F("%"));

  lcd.setCursor(1, 2);
  lcd.print(F(" "));
  lcd.print(pressure[0]);
  lcd.print(F("mb  "));
  lcd.setCursor(11, 2);

  counter ++;

  if ((counter > iterations) || (counter == 0)) // draw graph every hour
  {
    //Make Data string
    output = (String)rtc.getDateStr() + "," + MyTime2 + "," + (String)temperature[0] + "," ;
    output += (String)humidity[0] + "," + (String)temperature[1] + ",";
    output += (String)humidity[1] + "," + (String)pressure[0];

    DataFile = SD.open(Filename, FILE_WRITE);

    // if the file is available, write to it
    if (DataFile)
    {
      DataFile.println(output);
      // print to the serial port too:
      Serial.println(output);
      DataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println(F("error opening datalog.csv"));
    }

    DrawGraph();
    counter = 0;
  }

  delay(minute); // delay = 1 minute as unsigned Long


  //dump file to serial monitor.
  if (Serial)
  {
    SerialInput = Serial.read();
    if (SerialInput == '1') //write file to monitor
    {
      File dataFile = SD.open(Filename);

      // if the file is available, read it:
      if (dataFile) {
        while (dataFile.available()) {
          Serial.write(dataFile.read());
        }
        dataFile.close();
      }
      // if the file isn't open, pop up an error:
      else {
        Serial.println(F("Error opening datalog.csv"));
      }
    }
    else if (SerialInput == '2') //delete file
    {
      SD.remove(Filename);
    }
    SerialInput = '0';
  }
}


void DrawGraph()
{
  int line = 0;
  int value = 0;
  int block_size = 0;
  int pressureNow;

  pressureNow = pressure[0];

  if (PreviousValue > 0)
  {
    if ( pressureNow > PreviousValue) {
      block_size = 1;
    }
    else if (pressureNow == PreviousValue) {
      block_size = 2;
    }
    else if (pressureNow < PreviousValue) {
      block_size = 3;
    }
  }
  PreviousValue = pressureNow;

  for (int i = 1; i < 20; i++)
  {
    lcd.setCursor(line, 3);
    line = line + 1;
    value = graph[i];

    if (value == 0)
  {
    lcd.write((char)32); //space
    }
    else
    {
      lcd.write(byte(value));
    }

    graph[i - 1] = graph[i];
  }

  graph[19] = block_size;
  if (block_size == 0)
  {
    lcd.write((char)32);//space
  }
  else
  {
    lcd.write(byte(block_size));    
  }

}

bool AreWeInUTC()
{
  String MyTime = "";

  //use this for UK summertime = false = UTC+1, wintertime = true = UTC
  t = rtc.getTime();
  MyTime = (String)rtc.getDateStr();

  bool DST = 1;
  byte yy = t.year % 100;  //get year of century

  byte mm = (MyTime.substring(3, 5)).toInt();
  byte dd = (MyTime.substring(0, 2)).toInt();

  byte x1 = 31 - (yy + yy / 4 - 2) % 7; //last Sunday March
  byte x2 = 31 - (yy + yy / 4 + 2) % 7; // last Sunday October

  if ((mm > 3 && mm < 10) || (mm == 3 && dd >= x1) || (mm == 10 && dd < x2))
  {
    DST = 0;
  }

  return DST;
}
