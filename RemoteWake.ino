#include <Arduino.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <AddrList.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <server.h>
#include <base64.h>
#include <ArduinoJson.h>
#include <EEPROM_Rotate.h>

EEPROM_Rotate EEPROMr;

int SISSD_ADDR = 10;
int PASSWORD_ADDR = 100;
int REPO_ADDR = 200;
int FILE_ADDR = 300;
int GITHUB_TOKEN_ADDR = 400;
int BEFORE_IP_STRING_ADDR = 500;
int AFTER_IP_STRING_ADDR = 1500;

String ssid;
String password;
String repo;
String file;
String githubToken;
String beforeIpString;
String afterIpString;

WiFiClient client;
bool connectedWiFi;
bool bConnected;
bool firstConnected;

//发送魔术包实现
byte mac_1 = 0xf4;
byte mac_2 = 0xb5;
byte mac_3 = 0x20;
byte mac_4 = 0x30;
byte mac_5 = 0x3e;
byte mac_6 = 0x8a;

WiFiUDP Udp;

byte wake_msg[108];
//
// 构建pc唤醒魔术包
//
byte* build_wake_msg (byte mac_1=mac_1,byte mac_2=mac_2,byte mac_3=mac_3,
                     byte mac_4=mac_4,byte mac_5=mac_5,byte mac_6=mac_6) {
  for (int j = 0; j < 6; j++) {
    wake_msg[j] = 0xFF;
  }
  for (int j = 0; j < 16; j++) {
    wake_msg[6 + j * 6 + 0] = mac_1;
    wake_msg[6 + j * 6 + 1] = mac_2;
    wake_msg[6 + j * 6 + 2] = mac_3;
    wake_msg[6 + j * 6 + 3] = mac_4;
    wake_msg[6 + j * 6 + 4] = mac_5;
    wake_msg[6 + j * 6 + 5] = mac_6;
  }
  for (int j = 102; j < 108; j++) {
    wake_msg[j] = 0x00;
  }
  return wake_msg;
}

//
// 发送udp包，唤醒电脑
//
void open_pc(const char* pc_ip, int pc_port, byte* wake_msg) {
  Udp.beginPacket(pc_ip,pc_port);
  byte mypacket[108];
  for (int j=0;j<108;j++) {
    Udp.write(wake_msg[j]);
  }
  Udp.endPacket();
  delay(100);
}
//发送魔术包实现结束


const char* AP_NAME = "ESP8266";//wifi名字
 
const byte DNS_PORT = 53;//DNS端口号
IPAddress apIP(192, 168, 4, 1);//esp8266-AP-IP地址
DNSServer dnsServer;//创建dnsServer实例
ESP8266WebServer configServer(80);//创建WebServer

void handleRoot() {//访问主页回调函数
  configServer.send(200, "text/html", String("\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n\
  <title>Document</title>\r\n\
</head>\r\n\
<body>\r\n\
  <form name='input' action='/' method='POST'>\r\n\
        wifi名称: <br>\r\n\
        <textarea type='text' name='ssid'>")+ssid+String("</textarea><br>\r\n\
        wifi密码:<br>\r\n\
        <textarea type='text' name='password'>")+password+String("</textarea><br>\r\n\
        repo:<br>\r\n\
        <textarea type='text' name='repo'>")+repo+String("</textarea><br>\r\n\
        file:<br>\r\n\
        <textarea type='text' name='file'>")+file+String("</textarea><br>\r\n\
        githubToken:<br>\r\n\
        <textarea type='text' name='githubToken'>")+githubToken+String("</textarea><br>\r\n\
        beforeIpString:<br>\r\n\
        <textarea type='text' name='beforeIpString'>")+beforeIpString+String("</textarea><br>\r\n\
        afterIpString:<br>\r\n\
        <textarea type='text' name='afterIpString'>")+afterIpString+String("</textarea><br>\r\n\
        <input type='submit' value='保存'>\r\n\
    </form>\r\n\
</body>\r\n\
</html>\r\n\
"));
}
 
void handleRootPost() {//Post回调函数
  Serial.println("handleRootPost");
  if (configServer.hasArg("ssid")) {//判断是否有账号参数
    Serial.print("got ssid:");
    ssid = configServer.arg("ssid");
    Serial.println(ssid);
  } else {//没有参数
    Serial.println("error, not found ssid");
    configServer.send(200, "text/html", "<meta charset='UTF-8'>error, not found ssid");//返回错误页面
    return;
  }
  //密码与账号同理
  if (configServer.hasArg("password")) {
    Serial.print("got password:");
    password = configServer.arg("password");
    Serial.println(password);
  } else {
    Serial.println("error, not found password");
    configServer.send(200, "text/html", "<meta charset='UTF-8'>error, not found password");
    return;
  }


  if (configServer.hasArg("repo")) {
    Serial.print("got repo:");
    repo = configServer.arg("repo");
    Serial.println(repo);
  }
  if (configServer.hasArg("file")) {
    Serial.print("got file:");
    file = configServer.arg("file");
    Serial.println(file);
  }
  if (configServer.hasArg("githubToken")) {
    Serial.print("got githubToken:");
    githubToken = configServer.arg("githubToken");
    Serial.println(githubToken);
  }
  if (configServer.hasArg("beforeIpString")) {
    Serial.print("got beforeIpString:");
    beforeIpString = configServer.arg("beforeIpString");
    Serial.println(beforeIpString);
  }
  if (configServer.hasArg("afterIpString")) {
    Serial.print("got afterIpString:");
    afterIpString = configServer.arg("afterIpString");
    Serial.println(afterIpString);
  }
  saveData();
  configServer.send(200, "text/html", "<meta charset='UTF-8'>保存成功");//返回保存成功页面
  delay(2000);
  //连接wifi
  connectNewWiFi(false);
}

// Set 'pos' parameter to specify begin position of the string in memory 
void writeString(String str, int pos){
  EEPROMr.write(pos, str.length());//EEPROM第a位，写入str字符串的长度
  //把str所有数据逐个保存在EEPROM
  for (int i = 0; i < str.length(); i++) {
    EEPROMr.write(pos + i, str[i]);
  }
  EEPROMr.write(pos + str.length(), '\0');
  EEPROMr.commit();
}

// Set 'pos' parameter to specify begin position of the string in memory
String readString(int pos){
  String data = "";
  //从EEPROM中逐个取出每一位的值，并链接
  for (int i = 0; i < 1500; i++){
    if(EEPROMr.read(pos + i)=='\0') {
      break;
    }
    data += char(EEPROMr.read(pos + i));
  }
  return data;
}

void readData() {
  ssid = readString(SISSD_ADDR);
  password = readString(PASSWORD_ADDR);
  repo = readString(REPO_ADDR);
  file = readString(FILE_ADDR);
  githubToken = readString(GITHUB_TOKEN_ADDR);
  beforeIpString = readString(BEFORE_IP_STRING_ADDR);
  afterIpString = readString(AFTER_IP_STRING_ADDR);
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(repo);
  Serial.println(file);
  Serial.println(githubToken);
  Serial.println(beforeIpString);
  Serial.println(afterIpString);
}

void saveData() {
  writeString(ssid, SISSD_ADDR);
  writeString(password, PASSWORD_ADDR);
  writeString(repo, REPO_ADDR);
  writeString(file, FILE_ADDR);
  writeString(githubToken, GITHUB_TOKEN_ADDR);
  writeString(beforeIpString, BEFORE_IP_STRING_ADDR);
  writeString(afterIpString, AFTER_IP_STRING_ADDR);
}

void initBasic(){//初始化基础
  Serial.begin(115200);
  delay(2000);
  EEPROMr.size(2);
  EEPROMr.begin(4096);
  readData();
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
  WiFi.hostname("Smart-ESP8266");//设置ESP8266设备名
}
 
void initSoftAP(){//初始化AP模式
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  if(WiFi.softAP(AP_NAME, "12345678")){
    Serial.println("ESP8266 SoftAP is right");
  }
}
 
void initConfigServer(void){//初始化WebServer
  //configServer.on("/",handleRoot);
  //上面那行必须以下面这种格式去写否则无法强制门户
  configServer.on("/", HTTP_GET, handleRoot);//设置主页回调函数
  configServer.onNotFound(handleRoot);//设置无法响应的http请求的回调函数
  configServer.on("/", HTTP_POST, handleRootPost);//设置Post请求回调函数
  configServer.begin();//启动WebServer
  Serial.println("WebServer started!");
}
 
void initDNS(void){//初始化DNS服务器
  if(dnsServer.start(DNS_PORT, "*", apIP)){//判断将所有地址映射到esp8266的ip上是否成功
    Serial.println("start dnsserver success.");
  }
  else Serial.println("start dnsserver failed.");
}

void connectNewWiFi(bool first){
  WiFi.mode(WIFI_STA);//切换为STA模式
  WiFi.setAutoConnect(true);
  if(first) {
    WiFi.begin();//连接上一次连接成功的wifi
  } else {
    WiFi.persistent(true);
    WiFi.begin(ssid, password);
  }
  Serial.println("");
  Serial.print("Connect to wifi");
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    count++;
    if(count > 15){//如果5秒内没有连上，就开启Web配网 可适当调整这个时间
      initSoftAP();
      initConfigServer();
      initDNS();
      break;//跳出 防止无限初始化
    }
    Serial.print(".");
  }
  Serial.println("");
  if(WiFi.status() == WL_CONNECTED){//如果连接上 就输出IP信息 防止未连接上break后会误输出
    connectedWiFi = true;
    Serial.println("WIFI Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());//打印esp8266的IP地址
    configServer.stop();
    initServer();
  }
}
ESP8266WebServer esp8266_server(80);

void updateFile(String content, String sha) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;   
  http.begin(client, "https://api.github.com/repos/"+repo+"/contents/"+file);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("Authorization", "Bearer "+githubToken);
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");       
  int httpResponseCode = http.PUT("{\"message\":\"message\",\"content\":\""+base64::encode(content)+"\",\"sha\":\""+sha+"\"}");   
  if (httpResponseCode>0) {
    String response = http.getString();   
    Serial.println(httpResponseCode);
    Serial.println(response);          
   } else {
    Serial.print("Error on sending PUT Request: ");
    Serial.println(httpResponseCode);
   }
   http.end();
}

String getSha() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;   
  http.begin(client, "https://api.github.com/repos/"+repo+"/contents/"+file);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("Authorization", "Bearer "+githubToken);
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");       
  int httpResponseCode = http.GET();   
  if (httpResponseCode>0) {
    String response = http.getString();   
    Serial.println(httpResponseCode);
    Serial.println(response);
    http.end();
    StaticJsonDocument<200> jsonBuffer; //声明一个JsonDocument对象，长度200
    // 反序列化JSON
    DeserializationError error = deserializeJson(jsonBuffer, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return "";
    }
    String str = jsonBuffer["sha"];
    Serial.println(str);
    return str;
  } else {
    Serial.print("Error on sending GET Request: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return "";
}

bool checkNeedUpdate(String content) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;   
  http.begin(client, "https://api.github.com/repos/"+repo+"/contents/"+file);
  Serial.println("https://api.github.com/repos/"+repo+"/contents/"+file);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("Authorization", "Bearer "+githubToken);
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");       
  int httpResponseCode = http.GET();   
  if (httpResponseCode>0) {
    String response = http.getString();   
    Serial.println(httpResponseCode);
    Serial.println(response);
    http.end();
    StaticJsonDocument<200> jsonBuffer; //声明一个JsonDocument对象，长度200
    // 反序列化JSON
    DeserializationError error = deserializeJson(jsonBuffer, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return true;
    }
    String str = jsonBuffer["content"];
    Serial.println(content);
    str.replace("\n", "");
    Serial.println(str);
    Serial.println(base64::encode(content));
    Serial.println(str.compareTo(base64::encode(content)));
    return str.compareTo(base64::encode(content))!=0;
  } else {
    Serial.print("Error on sending GET Request: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return true;
}

int lastMillis = -100000;

void updateIp() {
  if(WiFi.status() != WL_CONNECTED) {
    return;
  }
  if(millis()-lastMillis<120000) {
    return;
  }
  lastMillis = millis();
  Serial.println("Update IP");
  Serial.println(ESP.getFullVersion());
  for (auto a : addrList) {
    Serial.println(a.isV6());
    Serial.println(a.toString().startsWith("2"));
    Serial.println(a.toString().c_str());
    if(a.isV6() && a.toString().startsWith("2")) {
      if(checkNeedUpdate(beforeIpString+a.toString()+afterIpString)) {
        Serial.println("NEED UPDATE");
        updateFile(beforeIpString+a.toString()+afterIpString, getSha());
        break;
      }
      Serial.println("DO NOT NEED UPDATE");
      break;
    }
  }
}

void handleWake() {
  Serial.println("handleWake");
  if (!esp8266_server.hasArg("ip") || !esp8266_server.hasArg("port") 
  || !esp8266_server.hasArg("mac_1") || !esp8266_server.hasArg("mac_2") || !esp8266_server.hasArg("mac_3") 
  || !esp8266_server.hasArg("mac_4") || !esp8266_server.hasArg("mac_5") || !esp8266_server.hasArg("mac_6")) {//判断是否有账号参数
    Serial.println("error, not found args");
    esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>error, not found args");//返回错误页面
    return;
  }
  Serial.println(esp8266_server.arg("ip").c_str());
  Serial.println(esp8266_server.arg("port").toInt());
  mac_1 = atoi(esp8266_server.arg("mac_1").c_str());
  mac_2 = atoi(esp8266_server.arg("mac_2").c_str());
  mac_3 = atoi(esp8266_server.arg("mac_3").c_str());
  mac_4 = atoi(esp8266_server.arg("mac_4").c_str());
  mac_5 = atoi(esp8266_server.arg("mac_5").c_str());
  mac_6 = atoi(esp8266_server.arg("mac_6").c_str());
  Serial.println("Received MAC:");
  Serial.println(mac_1);
  Serial.println(mac_2);
  Serial.println(mac_3);
  Serial.println(mac_4);
  Serial.println(mac_5);
  Serial.println(mac_6);
  open_pc(esp8266_server.arg("ip").c_str(), esp8266_server.arg("port").toInt(), build_wake_msg(mac_1, mac_2, mac_3, mac_4, mac_5, mac_6));
  esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>成功");
}

void handlePing() {
  Serial.println("handlePing");
  if (!esp8266_server.hasArg("ip")) {
    Serial.println("error, not found args");
    esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>error, not found args");//返回错误页面
    return;
  }
  if (Ping.ping(esp8266_server.arg("ip").c_str(), esp8266_server.hasArg("times")?esp8266_server.arg("times").toInt():1)) {
    esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>true");
    return;
  }
  esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>false");
}

bool toggled;

void handleStatus() {
  Serial.println("handleStatus");
  esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>"+String(toggled));
}

void handleToggle() {
  Serial.println("handleToggle");
  toggled = !toggled;
  digitalWrite(0, toggled?LOW:HIGH);
  esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>"+String(toggled));
}

void handleReset() {
  Serial.println("handleReset");
  connectedWiFi = false;
  esp8266_server.send(200, "text/html", "<meta charset='UTF-8'>ok");
  WiFi.disconnect();
  Serial.println("WIFI Disconnected!");
  esp8266_server.stop();
  initSoftAP();
  initConfigServer();
  initDNS();
}

void initServer() {
  firstConnected = true;
  esp8266_server.on("/wake", HTTP_GET, handleWake);
  esp8266_server.on("/ping", HTTP_GET, handlePing);
  esp8266_server.on("/status", HTTP_GET, handleStatus);
  esp8266_server.on("/toggle", HTTP_GET, handleToggle);
  esp8266_server.on("/reset", HTTP_GET, handleReset);
  esp8266_server.begin();
}

void setup() {
  initBasic();
  Serial.println("Startup");
  connectNewWiFi(true);
}

void loop() {
  configServer.handleClient();
  dnsServer.processNextRequest();
  esp8266_server.handleClient();
  updateIp();
  if(WiFi.status() != WL_CONNECTED && connectedWiFi && firstConnected){
    connectedWiFi = false;
    Serial.println("WIFI Disconnected!");
    esp8266_server.stop();
    connectNewWiFi(false);
  }
}

