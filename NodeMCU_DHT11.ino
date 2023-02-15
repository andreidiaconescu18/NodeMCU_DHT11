#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RingBuf.h>

#define N 50

int timeInterval = 5000; //5 seconds
long prevTime = 0, currTime = 0;

typedef struct Data {
  String date;
  float temperature;
  float humidity;
} Data;

RingBuf<Data, N> myBuffer;

const char *ssid = "WIFI"; //Enter your WIFI ssid
const char *password = "parola"; //Enter your WIFI password

const long utcOffsetInSeconds = 7200; //GMT+2 -> 7200 seconds

DHTesp dht;

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void handleRoot() {
  String page = getPage();
  server.send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200);
  String thisBoard = ARDUINO_BOARD;
  Serial.println(thisBoard);

  dht.setup(5, DHTesp::DHT11); //GPIO5 = D1 pin of board

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

  server.on ( "/", handleRoot);

  server.begin();

  Serial.println ( "HTTP server started" );
}

void loop() {
  currTime = millis();
  if (currTime - prevTime >= timeInterval) {
    prevTime = currTime;
    readTemperature();
  }
  server.handleClient();
}

void readTemperature() {
  float temperature = dht.getTemperature();
  float humidity = dht.getHumidity();
  if (dht.getStatusString() == "OK")
  {  
    addToRingBuffer(temperature, humidity);
  }

}

void addToRingBuffer(float temperature, float humidity) {
  Data record, dummy;
  record.date = getCurrentDate();
  record.temperature = temperature;
  record.humidity = humidity;
  if (myBuffer.isFull())
    myBuffer.pop(dummy);
  myBuffer.push(record);
}

String getCurrentDate() {
  timeClient.update();

  String tm = timeClient.getFormattedTime(); //HH:MM:SS

  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  int currentYear = ptm->tm_year + 1900;
  String day = "";
  if (monthDay < 10)
    day = "0" + String(monthDay);
  else
    day = String(monthDay);

  String month = "";
  if (currentMonth < 10)
    month = "0" + String(currentMonth);
  else
    month = String(currentMonth);
  String currentDate = tm + " " + day + "." + month + "." + String(currentYear);
  return currentDate;
}

String getChartDataRow(Data record){
  String row = "['"+record.date+"',"+String(record.temperature)+","+String(record.humidity)+"]";
  return row;
}

String getPage() {
  String code = R"(
  <html>
  <head>
   <meta http-equiv="refresh" content="10">
    <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
    <script type="text/javascript">
      google.charts.load('current', {'packages':['corechart']});
      google.charts.setOnLoadCallback(drawChart);

      function drawChart() {
        var data = google.visualization.arrayToDataTable([
          ['Date', 'Temperatura', 'Umiditate'],
   )";
  String chartData ="";
 for(int i=0; i<myBuffer.size(); i++){
      Data record = myBuffer[i];
      String chartRow = getChartDataRow(record);
      chartData+=chartRow;
      if(i!=myBuffer.size()-1)
        chartData+=",";
  }
  
  
   code += chartData;
   code +=R"(
        ]);

        var options = {
          title: 'Temperatura & Umiditate',
          curveType: 'function',
          legend: { position: 'bottom' },
          chartArea: {
          height:500,
          top:100,
          },
          height:900,
          hAxis: {
          title: 'Data',
          slantedText:true,
          slantedTextAngle:90,
          }
        };

        var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));

        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <div id="curve_chart"></div>
  </body>
  </html>
  )";
  return code;
}
