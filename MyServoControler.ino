 
// ****************************************************************
// MyServoControler
//  Ver0.00 2017.8.18
//  copyright 坂柳
//  Hardware構成
//  マイコン：wroom-02
//  IO:
//   P4,5,12,13 サーボ
//   P14 WiFiモード選択 1: WIFI_AP, 0:WIFI_STA
//   P16 SSID選択 1:SERVO_AP, 0:SERVO_AP2
// ****************************************************************

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <math.h>
#include <EEPROM.h>

//*******************************************************************************
//* 定数、変数
//*******************************************************************************
const char* ssid;
const char* ssid1 = "SERVO_AP";
const char* ssid2 = "SERVO_AP2";
const char* password = "";

//サーボ関連
#define ServoCH 4
Servo myservo[ServoCH];         //サーボ構造体
int servo_val[ServoCH];          //サーボ指令値

//サーバー
ESP8266WebServer server(80);  //webサーバー
WiFiUDP udp;                     //udpサーバー
#define UDP_PORT 10000
const IPAddress HOST_IP = IPAddress(192, 168, 0, 10);
const IPAddress SUB_MASK = IPAddress(255, 255, 255, 0);

//接続しているほかのESPのIPのリスト
#define IP_Max 20
unsigned char IP_Num = 0;
IPAddress IP_List[IP_Max];
bool IP_sync[IP_Max];

// AP or ST
WiFiMode_t WiFiMode;

// 設定
int Range[4][2];
bool Reverse[4];
#define EEPROM_NUM 9

//ポート設定
#if 0
//高機能版
 #define ServoCH1 4
 #define ServoCH2 5
 #define ServoCH3 12
 #define ServoCH4 13
 #define DipSW1 16
 #define DipSW2 14
#else
//シンプル版
 #define ServoCH1 14
 #define ServoCH2 12
 #define ServoCH3 13
 #define ServoCH4 15
 #define DipSW1 5
 #define DipSW2 4
#endif
//------------------------------
// RCW Controller対応
//------------------------------
typedef struct {
  unsigned char  btn[2];    //ボタン
  unsigned char l_hzn; //左アナログスティック 左右
  unsigned char l_ver; //左アナログスティック 上下
  unsigned char r_hzn; //右アナログスティック 左右
  unsigned char r_ver; //左アナログスティック 上下
  unsigned char acc_x; //アクセラレータ X軸
  unsigned char acc_y; //アクセラレータ Y軸
  unsigned char acc_z; //アクセラレータ Z軸
  unsigned char rot:    3; //デバイスの向き
  unsigned char r_flg:  1; //右アナログ向き
  unsigned char l_flg:  1; //左アナログ向き
  unsigned char acc_flg: 2; //アクセラレータ設定
  unsigned char dummy:  1; //ダミー
} st_udp_pkt;

//*******************************************************************************
//* プロトタイプ宣言
//*******************************************************************************
bool say_hello(IPAddress IP);
void servo_ctrl(void);

//*******************************************************************************
//* HomePage
//*******************************************************************************
// -----------------------------------
// Top Page
//------------------------------------
void handleMyPage() {
  int i;
  char buff[10];
  String ch_str;

  Serial.println("MyPage");

  String content =
    "<!DOCTYPE html>\
 <html>\
  <head>\
   <meta charset='UTF-8'>\
   <meta name='viewport' content='width=device-width, user-scalable=no'>\
   <title>Servo Control</title>\
   <style type='text/css'>\
    input{width:100%;height:20pt;}\
   </style>\
   <script type='text/javascript'>\
    function formsubmit(){\
     document.inputform.submit();\
     }\
  </script>\
  </head>\
  <body>\
  <H1>\
  Servo Controler\
  </H1>\
  <p>[<a href='/Propo'>Propo mode</a>] [<a href='/List'>IP List</a>] [<a href='/Setting'>Setting</a>]</p>\
  <form name='inputform' action='/Ctrl' method='POST' target='iframe'>\
   Servo0:<br><input type='range' name='SERVO0' value='" + String(servo_val[0]) + "' min='0' max='180' step='1' onChange='formsubmit()'><br>\
   Servo1:<br><input type='range' name='SERVO1' value='" + String(servo_val[1]) + "' min='0' max='180' step='1' onChange='formsubmit()'><br>\
   Servo2:<br><input type='range' name='SERVO2' value='" + String(servo_val[2]) + "' min='0' max='180' step='1' onChange='formsubmit()'><br>\
   Servo3:<br><input type='range' name='SERVO3' value='" + String(servo_val[3]) + "' min='0' max='180' step='1' onChange='formsubmit()'><br>\
          <br><input type='submit' name='SUBMIT' value='Submit'>\
  </form>\
  <iframe name='iframe' style='display:none'></iframe>\
  </body>\
</html>";
  server.send(200, "text/html", content);
}
// -----------------------------------
// Propo Mode
//------------------------------------
void handlePropo() {
  Serial.println("PropoMode");
  String content =
    "<!DOCTYPE html><html><head>\
 <title>Propo</title>\
 <meta name='viewport' content='width=device-width, user-scalable=no'>\
 <meta name='apple - mobile - web - app - capable' content='yes'>\
 <meta name='apple - mobile - web - app - status - bar - style' content='black'>\
 <style type='text/css'>\
  body { margin: 0px; overflow: hidden; }\
  canvas { border: none;}\
 </style>\
<script type='text/javascript'>\
var canvas, ctx;\
var w = 0,h = 0;\
var timer,timer2;\
\
var lx,ly,rx,ry,sw;\
var s_array = [90,90,90,90];\
\
var updateStarted = false;\
var touches = [];\
\
function update() {\
 if (updateStarted) return;\
  updateStarted = true;\
\
  var nw = window.innerWidth;\
  var nh = window.innerHeight;\
\
  if ((w != nw) || (h != nh)) {\
    w = nw;\
    h = nh;\
\
    canvas.style.width = w+'px';\
    canvas.style.height = h+'px';\
    canvas.width = w;\
    canvas.height = h;\
  }\
\
  if(h>w){\
    lx= w/2;rx=w/2;\
    if(h/4>w/2){\
      ly=w/2;ry=h-w/2; sw=w/2*0.7;\
    }else{\
      ly=h/4;ry=h-h/4; sw=h/4*0.7;\
    }\
  }else{\
    ly= h/2;ry=h/2;\
    if(w/4>h/2){\
      lx=h/2;rx=w-h/2; sw=h/2*0.7;\
    }else{\
      lx=w/4;rx=w-w/4; sw=w/4*0.7;\
    }\
  }\
\
  var i, len = touches.length;\
  for (i=0; i<len; i++) {\
    var touch = touches[i];\
    var px = touch.pageX;\
    var py = touch.pageY;\
\
    if(Math.abs(px-lx)<sw && Math.abs(py-ly)<sw){\
      s_array[0]= PosToServo(px,lx,sw*2);\
      s_array[1]= PosToServo(py,ly,sw*2);\
    }else if(Math.abs(px-rx)<sw && Math.abs(py-ry)<sw){\
      s_array[2]= PosToServo(px,rx,sw*2);\
      s_array[3]= PosToServo(py,ry,sw*2);\
    }\
    \
    if(px<sw*0.2 && py<sw*0.2){\
      window.location.href = '/'; \
    }\
  }\
  \
  make_graphic();\
\
  updateStarted = false;\
}\
\
function make_graphic(){\
  ctx.clearRect(0, 0, w, h);\
  ctx.lineWidth = 2.0;\
  ctx.strokeStyle = 'rgba(0, 0, 0, 1)';\
\
  ctx.beginPath();\
  ctx.arc(lx,ly,sw,0,2*Math.PI,true);\
  ctx.rect(lx-sw,ly-sw,sw*2,sw*2);\
  ctx.stroke();\
\
  ctx.beginPath();\
  ctx.arc(rx,ry,sw,0,2*Math.PI, true);\
  ctx.rect(rx-sw,ry-sw,sw*2,sw*2);\
  ctx.stroke();\
  \
  ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';\
  ctx.beginPath();\
  ctx.arc(ServoToPos(s_array[0],lx,sw*2), ServoToPos(s_array[1],ly,sw*2), sw*0.2, 0, 2*Math.PI, true);\
  ctx.fill();\
  ctx.stroke();\
  \
  ctx.beginPath();\
  ctx.arc(ServoToPos(s_array[2],rx,sw*2), ServoToPos(s_array[3],ry,sw*2), sw*0.2, 0, 2*Math.PI, true);\
  ctx.fill();\
  ctx.stroke();\
  \
  ctx.beginPath();\
  ctx.rect(0,0,sw*0.2,sw*0.2);\
  ctx.stroke();\
  ctx.beginPath();\
  ctx.moveTo(0,sw*0.1);\
  ctx.lineTo(sw*0.2,0);\
  ctx.lineTo(sw*0.2,sw*0.2);\
  ctx.lineTo(0,sw*0.1);\
  ctx.fillStyle = 'rgba(0, 100, 0, 0.5)';\
  ctx.fill();\
  ctx.stroke();\
}\
\
function ol() {\
  canvas = document.getElementById('canvas');\
  ctx = canvas.getContext('2d');\
  timer = setInterval(update, 15);\
  timre2 = setInterval(submitServo,32);\
\
  canvas.addEventListener('touchmove', function(event) {\
  event.preventDefault();\
  touches = event.touches;});\
}\
\
function PosToServo( pos, center,width ){\
  var res;\
  res = ((pos - center)/width+0.5)*180;\
  res = Math.max(res,0);\
  res = Math.min(res,180);\
  return Math.round(res);\
}\
\
function ServoToPos( servo, center,width ){\
  var res;\
  res = Math.max(servo,0);\
  res = Math.min(servo,180);\
  res = center + (servo -90)*width/180;\
  return Math.round(res);\
}\
\
\
function submitServo(){\
  for(var i=0;i<s_array.length;i++){\
    document.inputform.elements['SERVO'+i].value = s_array[i];\
  }\
  document.inputform.submit();\
  console.log(s_array);\
}\
</script>\
\
</head>\
\
<body onload='ol()'>\
<canvas id='canvas' width='300' height='300' style='top:0px; left:0px; width: 300px; height: 300px;'></canvas>\
  <form name='inputform' action='/Ctrl' method='POST' target='iframe'>\
   <input type='hidden' name='SERVO0' >\
   <input type='hidden' name='SERVO1' >\
   <input type='hidden' name='SERVO2' >\
   <input type='hidden' name='SERVO3' >\
  </form>\
<iframe name='iframe' style='display:none'></iframe>\
</body>\
\
</html>";
  server.send(200, "text/html", content);
}
// -----------------------------------
// IP List
//------------------------------------
void handleList() {
  Serial.println("IP List");
  String content = "\
<!DOCTYPE html><html><head>\
 <meta charset='UTF-8'><meta name='viewport' content='width=device-width, user-scalable=no'>\
 <title>IP List</title>\
<style type='text/css'>\
input[type=checkbox]{height:20pt;width:20pt}\
input[type=submit]{height:20pt;width:100%}\
</style></head>\
<body>\
<H1>IP List</H1>\
<p>[<a href='/'>main</a>] [<a href='/Setting'>Setting</a>]</p>\
<form name='inputform' action='/SetSync' method='POST' target='iframe'>\
";
  for (int i = 0; i < IP_Num; i++) {
    content += "<input type='checkbox' name='IP" + String(i) + "' value='" + IP_List[i].toString() + "'";
    if (IP_sync[i]) content += " checked='checked'";
    content += ">";
    content += "<a href='http://";
    content += IP_List[i].toString();
    content += "'>";
    content += IP_List[i].toString();
    content += "</a><br>";
  }
  content += "\
<br><input type='submit' name='SUBMIT' value='Submit'>\
</form>\
<p>接続しているコントローラーの一覧です。チェックを付けたコントローラーは連動します。</p>\
<iframe name='iframe' style='display:none'></iframe>\
</body>\
</html>\
";
  server.send(200, "text/html", content);
}

// -----------------------------------
// Setting
//------------------------------------
void handleSetting() {
  Serial.println("Setting");
  int i, j;
  bool flg = false;
  // 設定反映
  if (server.hasArg("SUBMIT")) {
    flg = true;
    for (i = 0; i < ServoCH; i++) {
      Reverse[i] = false;
    }
  }
  for (i = 0; i < ServoCH; i++) {
    if (server.hasArg("Rng_mn" + String(i))) Range[i][0] = atoi(server.arg("Rng_mn" + String(i)).c_str());
    if (server.hasArg("Rng_mx" + String(i))) Range[i][1] = atoi(server.arg("Rng_mx" + String(i)).c_str());
    if (server.hasArg("Rvs"   + String(i))) Reverse[i]  = true;
  }

  String content = "\
<!DOCTYPE html><html><head>\
 <meta charset='UTF-8'><meta name='viewport' content='width=device-width, user-scalable=no'>\
 <title>Setting</title>\
<style type='text/css'>\
    input[type=range]{height:20pt;width:40%}\
    input[type=submit]{height:20pt;width:100%}\
    input[type=checkbox]{height:20pt;width:20pt;}\
</style>\
<script type='text/javascript'>\
function update(){\
";
  for (i = 0; i < ServoCH; i++) {
    content += "document.getElementById('S" + String(i) + "').innerHTML='('+document.inputform.Rng_mn" + String(i) + ".value+'-'+document.inputform.Rng_mx" + String(i) + ".value + ')';";
  }
  content += "\
}\
</script></head>\
<body onload='update()'>\
<H1>Setting</H1>\
<p>[<a href='/'>main</a>] [<a href='/Propo'>Propo mode</a>] [<a href='/List'>IP List</a>]</p>\
<form name='inputform' action='/Setting' method='POST'>\
<H2>Range</H2>\
";
  for (i = 0; i < ServoCH; i++) {
    content += "Servo" + String(i) + ":<div id='S" + String(i) + "'>(0-180)</div><br>";
    content += "0<input type='range' name='Rng_mn" + String(i) + "' value='" + String(Range[i][0]) + "'   min='0'  max='90'  step='1' onChange='update()'>90";
    content += "<input type='range' name='Rng_mx" + String(i) + "' value='" + String(Range[i][1]) + "' min='90' max='180' step='1' onChange='update()'>180<br>";
  }
  content += "<H2>Reverse</H2>";
  for (i = 0; i < ServoCH; i++) {
    content += "Servo" + String(i) + ":<input type='checkbox' name='Rvs" + String(i) + "' value='1'";
    if (Reverse[i]) content += " checked='checked'";
    content += "><br>";
  }
  content += "\
<br><input type='submit' name='SUBMIT' value='Submit'>\
</form>\
</body>\
</html>\
";
  server.send(200, "text/html", content);
  //EEPROM書き込み
  if (flg) {
    unsigned char data[EEPROM_NUM];
    data[8] = 0;
    for (i = 0; i < ServoCH; i++) {
      for (j = 0; j < 2; j++) {
        data[i * 2 + j] = Range[i][j];
      }
      if (Reverse[i])data[8] += (1 << i);
    }
    for (i = 0; i < EEPROM_NUM; i++) {
      EEPROM.write(i, data[i]);
    }
    EEPROM.commit();
  }
}
// -----------------------------------
// Not Found
//------------------------------------
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
// -----------------------------------
// Servo Control
//------------------------------------
void handleCtrl() {
  Serial.println("CtrlPage");
  int i;
  char buff[10];
  String ch_str, servo_str;
  String OPT_str = "";
  int t_servo_val[ServoCH];
  for (i = 0; i < ServoCH; i++) {
    itoa(i, buff, 10);
    ch_str = buff;
    ch_str = "SERVO" + ch_str;

    if (server.hasArg(ch_str)) {
      servo_str = server.arg(ch_str);
      if (OPT_str.length() > 0) OPT_str += "&";
      OPT_str += ch_str + "=" + servo_str;
      //    Serial.println(ch_str + ":" + servo_str);
      servo_val[i] = atoi(servo_str.c_str());
      if (servo_val[i] > 180) servo_val[i] = 180;
      if (servo_val[i] <   0) servo_val[i] = 0;
      //        myservo[i].write(servo_val[i]);              // tell servo to go to position in variable 'pos'
    }
  }
  //同期
  /*  st_udp_pkt pkt;
    pkt.l_ver = ((unsigned long)servo_val[0] *255)/180;
    pkt.l_hzn = ((unsigned long)servo_val[1] *255)/180;
    pkt.r_ver = ((unsigned long)servo_val[2] *255)/180;
    pkt.r_hzn = ((unsigned long)servo_val[3] *255)/180;
    pkt.l_flg = 1;
    pkt.r_flg = 1;
    for(i=0;i<IP_Num;i++){
      if(IP_sync[i]){
        if(udp.beginPacket(IP_List[i],UDP_PORT)){
          udp.write((char*)&pkt,sizeof(pkt));
          udp.endPacket();
        }
      }
    }
  */
  servo_ctrl();
  String content = "<!DOCTYPE html><html><head></head><body></body></html>";
  server.send(200, "text/html", content);
}
// -----------------------------------
// Set Synchronization
//------------------------------------
void handleSetSync() {
  int i, j;
  String ch_str, IP_str;

  Serial.println("SetSyncPage");

  IPAddress t_IPs[IP_Max];

  for (i = 0; i < IP_Num; i++) {
    IP_sync[i] = false;
  }

  for (i = 0; i < IP_Max; i++) {
    ch_str = "IP" + String(i);
    if (server.hasArg(ch_str)) {
      IP_str = server.arg(ch_str);
      Serial.println("Synchronize:" + IP_str + "(" + ch_str + ")");
      for (j = 0; j < IP_Num; j++) {
        if (IP_List[j].toString() == IP_str) {
          IP_sync[j] = true;
          break;
        }
      }
    }
  }

  String content = "<!DOCTYPE html><html><head></head><body></body></html>";
  server.send(200, "text/html", content);
}

// -----------------------------------
// Register
//------------------------------------
void handleHello() {
  if (WiFiMode == WIFI_AP) {
    int flg = 1;
    IPAddress t_IP = server.client().remoteIP();
    Serial.println("Welcome," + t_IP.toString());
    for (int i = 0; i < IP_Num; i++)
    {
      if (t_IP == IP_List[i]) {
        flg = 0;
        break;
      }
    }
    if (flg) {
      if (IP_Num >= IP_Max) {
        Serial.println("IP Full. Cannot Register:" + t_IP.toString());
      } else {
        IP_List[IP_Num] = t_IP;
        IP_Num++;
        Serial.println("Register:" + t_IP.toString());
      }
    }
  }
  server.send(200, "text/plain", "Hello");
}
// *****************************************************************************************
// * その他の処理
// *****************************************************************************************
bool say_hello(IPAddress IP) {
  bool flg = false;

  HTTPClient http;
  http.begin("http://" + IP.toString() + "/Hello");

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.print("[HTTP] GET... code: ");
    Serial.println(httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
    flg = true;
  } else {
    Serial.print("[HTTP] GET... failed, error:");
    Serial.println(http.errorToString(httpCode).c_str());
    flg = false;
  }
  http.end();
  return (flg);
}

//------------------------------
// Control関数
//------------------------------
void servo_ctrl(void) {
  //Range変換
  int i, val[ServoCH];
  for (i = 0; i < ServoCH; i++) {
    val[i] = servo_val[i];
    //Range変換
    if (Reverse[i]) val[i] = 180 - val[i];
    val[i] = val[i] * (Range[i][1] - Range[i][0]) / 180 + Range[i][0];
    myservo[i].write(val[i]);
  }

  //同期
  st_udp_pkt pkt;
  pkt.l_ver = ((unsigned long)servo_val[0] * 255) / 180;
  pkt.l_hzn = ((unsigned long)servo_val[1] * 255) / 180;
  pkt.r_ver = ((unsigned long)servo_val[2] * 255) / 180;
  pkt.r_hzn = ((unsigned long)servo_val[3] * 255) / 180;
  pkt.l_flg = 1;
  pkt.r_flg = 1;
  for (i = 0; i < IP_Num; i++) {
    if (IP_sync[i]) {
      if (udp.beginPacket(IP_List[i], UDP_PORT)) {
        udp.write((char*)&pkt, sizeof(pkt));
        udp.endPacket();
      }
    }
  }

}

// *****************************************************************************************
// * 初期化
// *****************************************************************************************
//------------------------------
// RCW Controller対応
//------------------------------
void setup_udp(void) {
  udp.begin(UDP_PORT);
}
//------------------------------
// IO初期化
//------------------------------
void setup_io(void) {
  //init servo
  myservo[0].attach(ServoCH1);   // attaches the servo on GIO4 to the servo object
  myservo[1].attach(ServoCH2);   // attaches the servo on GIO5 to the servo object
  myservo[2].attach(ServoCH3);  // attaches the servo on GI12 to the servo object
  myservo[3].attach(ServoCH4);  // attaches the servo on GI13 to the servo object
  //init port
  pinMode(DipSW1, INPUT);
  pinMode(DipSW2, INPUT);
}
// ------------------------------------
void setup_com(void) {      //通信初期化
  // シリアル通信
  Serial.begin(115200);

  // WiFi
  if (WiFiMode == WIFI_AP) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(HOST_IP, HOST_IP, SUB_MASK);
    WiFi.softAP(ssid, password);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}
// ------------------------------------
void setup_ram(void) {
  //サーボ初期値
  int i;
  for (i = 0; i < ServoCH; i++)
  {
    servo_val[i] = 90;
  }

  //WiFiモード選択
  int t_WiFiMode = digitalRead(DipSW2);
  if (t_WiFiMode) WiFiMode = WIFI_AP;
  else           WiFiMode = WIFI_STA;

  //AP選択
  int AP_sw = digitalRead(DipSW1);
  if (AP_sw)  ssid = ssid1;
  else        ssid = ssid2;

}
// ------------------------------------
void register_ip(void) {
  if (WiFiMode == WIFI_AP) return;

  // サーバーにIPを登録
  Serial.println("Register IP");
  say_hello(HOST_IP);

}
// ------------------------------------
void setup_http(void) {
  server.on("/", handleMyPage);
  server.on("/Propo", handlePropo);
  server.on("/List", handleList);
  server.on("/Setting", handleSetting);
  server.on("/Hello", handleHello);
  server.on("/Ctrl", handleCtrl);
  server.on("/SetSync", handleSetSync);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

// ------------------------------------
void setup_eeprom(void) {
  //EEPROMのマップ
  // 0～7:Range[4][2]
  // 8:Reverse
  EEPROM.begin(EEPROM_NUM);
  unsigned char data[EEPROM_NUM];
  int i, j;
  for (i = 0; i < EEPROM_NUM; i++) {
    data[i] = EEPROM.read(i);
  }
  for (i = 0; i < ServoCH; i++) {
    for (j = 0; j < 2; j++) {
      Range[i][j] = data[i * 2 + j];
    }
    if (Range[i][0] > 90) Range[i][0] = 0;
    if (Range[i][1] < 90 || Range[i][1] > 180) Range[i][1] = 180;
    Reverse[i] = (data[8] & (1 << i)) >> i;
  }
}
// ------------------------------------
void setup(void) {

  setup_io();
  setup_ram();
  setup_eeprom();
  setup_com();
  register_ip();
  setup_http();
  setup_udp();
}
// *****************************************************************************************
// * Loop処理
// *****************************************************************************************
//------------------------------
// 登録IPをチェック
//------------------------------
void check_IP(void) {
  // 消えたESPを特定して削除する
  IPAddress t_IPs[IP_Max];
  unsigned char t_IPnum = 0;
  int i;
  for (i = 0; i < IP_Num; i++) {
    if (say_hello(IP_List[i])) {
      t_IPs[t_IPnum] = IP_List[i];
      t_IPnum++;
    } else {
      Serial.println(IP_List[i].toString() + " is Lost");
    }
  }
  for (i = 0; i < IP_Num; i++) {
    if (i < t_IPnum) IP_List[i] = t_IPs[i];
    else            IP_List[i] = IPAddress(0, 0, 0, 0);
  }
  IP_Num = t_IPnum;
}

//------------------------------
// Station数をチェック
//------------------------------
int station_num = 0;
void check_softap(void) {
  if (WiFiMode == WIFI_AP) {
    int t_st_num = WiFi.softAPgetStationNum();
    if (t_st_num != station_num) {
      Serial.print("StationNum:");
      Serial.println(t_st_num);
      Serial.println(WiFi.softAPIP());
      check_IP();
    }
    station_num = t_st_num;
  }
}

//------------------------------
// RCW Controller対応
//------------------------------
void udp_loop(void) {
  int rlen;
  rlen = udp.parsePacket();
  if (rlen < 10) return;
  st_udp_pkt pkt;
  udp.read((char*)(&pkt), 10);
  Serial.print("BTN:");  Serial.print(pkt.btn[0]);
  Serial.print(" ");  Serial.print(pkt.btn[1]);
  Serial.print(" L1:");  Serial.print(pkt.l_hzn);
  Serial.print(" L2:");  Serial.print(pkt.l_ver);
  Serial.print(" R1:");  Serial.print(pkt.r_hzn);
  Serial.print(" R2:");  Serial.print(pkt.r_ver);
  Serial.print(" accx:"); Serial.print(pkt.acc_x);
  Serial.print(" accy:"); Serial.print(pkt.acc_y);
  Serial.print(" accz:"); Serial.print(pkt.acc_z);
  Serial.print(" rot:"); Serial.print(pkt.rot);
  Serial.print(" r_flg:"); Serial.print(pkt.r_flg);
  Serial.print(" l_flg:"); Serial.print(pkt.l_flg);
  Serial.print(" acc_flg:"); Serial.print(pkt.acc_flg);
  Serial.print(" dummy:"); Serial.print(pkt.dummy);
  Serial.print("\n");

  if (pkt.l_flg)
  {
    servo_val[0] = (unsigned char)(((unsigned long)pkt.l_ver * 180) >> 8);
    servo_val[1] = (unsigned char)(((unsigned long)pkt.l_hzn * 180) >> 8);
  } else {
    Serial.print("debug:" + String(pkt.btn[1]) + ":" + String(pkt.btn[1] & 0x03));
    switch (pkt.btn[1] & 0x03) {
      case 0x01: servo_val[0] = 180; break; //UP
      case 0x02: servo_val[0] = 0;   break; //DOWN
      //    case 0x03: //START
      default:   servo_val[0] = 90;  break;
    }
    switch (pkt.btn[1] & 0x0C) {
      case 0x04: servo_val[1] = 180; break;//RIGHT
      case 0x08: servo_val[1] = 0;   break;//LEFT
      //    case 0x0C: //START
      default:   servo_val[1] = 90;  break;
    }

  }
  if (pkt.r_flg)
  {
    servo_val[2] = (unsigned char)(((unsigned long)pkt.r_ver * 180) >> 8);
    servo_val[3] = (unsigned char)(((unsigned long)pkt.r_hzn * 180) >> 8);
  } else {
    switch (pkt.btn[1] & 0x30) {
      case 0x10: servo_val[2] = 180;  break; //Y
      case 0x20: servo_val[2] = 0;    break; //A
      default:   servo_val[2] = 90;   break;
    }
    if     (pkt.btn[1] & 0x40) servo_val[3] = 180; //B
    else if (pkt.btn[0] & 0x01) servo_val[3] = 0; //X
    else                     servo_val[3] = 90;
  }
  servo_ctrl();
}

void loop(void) {
  server.handleClient();
  check_softap();
  udp_loop();
}
