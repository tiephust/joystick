#if __has_include(<BleMouse.h>) // Kiểm tra xem thư viện có tồn tại hay không
  #include <BleMouse.h>
  #define ENABLE_VIRTUAL_MOUSE
#else
  #warning "BleMouse library not found, virtual mouse support is disabled."
#endif

#include <Arduino.h>
#include <ezButton.h>
#include <OneWire.h>
#include <MPU9250.h>
#include <Wire.h>
#include <SimpleKalmanFilter.h>
#include <RotaryEncoder.h>
#include <WiFi.h>
#include <PubSubClient.h>

//-----------------------------------------------------------------------------------------------
//      CÁC THIẾT LẬP PORTING - cho biết các module linh kiện được ghép nối với CPU ở pin nào?
//-----------------------------------------------------------------------------------------------
/** GPIO rotary encoder - Chân CLK */
#define CLK GPIO_NUM_13
/** GPIO rotary encoder - Chân DT */
#define DT GPIO_NUM_14
/** GPIO rotary encoder - Chân SW */
#define SW GPIO_NUM_23
/** GPIO cảm biến 9 trục MPU9250 - Chân SCL */
#define SCL GPIO_NUM_22
/** GPIO cảm biến 9 trục MPU9250 - Chân SDA */
#define SDA GPIO_NUM_21

//-----------------------------------------------------------------------------------------------
//      CÁC THIẾT LẬP KHÁC - cho biết các thông tin được sử dụng trong project
//-----------------------------------------------------------------------------------------------
#define ENABLE_MTQTT
/** Thiết lập thông tin MQTT host*/
#define MQTT_SERVER "broker.emqx.io"
#define MQTT_PORT 1883
/** Thiết lập MQTT topic*/
#define MQTT_JOYSTICK_PUBLISH "ESP32/DATA_RESPONSE"
/** Thiết lập Device ID */
#define DEVICE_ID "joystick/Vanh"

/** Thiết lập wifi*/
const char *ssid = "quangtiep";
const char *password = "quangtiep02";

bool buttonState = false; // Trạng thái nút bấn của rotary encoder
static int lastPos = 0;   // Giá trị gần nhất của rotary encoder
long timer = 0;
float angleX = 0, angleY = 0, angleZ = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
/** Khai báo cho rotary encoder switch */
ezButton button(SW);
/** Khai báo rotary encoder */
RotaryEncoder encoder(CLK, DT, RotaryEncoder::LatchMode::TWO03);
/** Khai báo MPU */
MPU9250 mpu9250;
/** Thiết lập thông số kalman filter cho các chỉ số khác nhau */
SimpleKalmanFilter gyroXKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter gyroYKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter gyroZKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter accelXKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter accelYKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter accelZKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter magZKalmanFilter(0.01, 0.01, 0.001);
SimpleKalmanFilter angleXKalmanFilter(0.01, 0.01, 0.1);
SimpleKalmanFilter angleYKalmanFilter(0.01, 0.01, 0.1);
SimpleKalmanFilter angleZKalmanFilter(0.01, 0.01, 0.1);

#ifdef ENABLE_VIRTUAL_MOUSE
  BleMouse bleMouse;
#endif

#if defined(ENABLE_MTQTT)
void connect_to_broker()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void publish(
    float x,
    float y,
    float z,
    float ax,
    float ay,
    float az,
    float gx,
    float gy,
    float gz,
    float magx, 
    float magy,
    float magz,
    int r,
    bool sw)
{
  std::string str_x = std::to_string(x);
  std::string str_y = std::to_string(y);
  std::string str_z = std::to_string(z);
  std::string str_ax = std::to_string(ax);
  std::string str_ay = std::to_string(ay);
  std::string str_az = std::to_string(az);
  std::string str_gx = std::to_string(gx);
  std::string str_gy = std::to_string(gy);
  std::string str_gz = std::to_string(gz);
  std::string str_mgx = std::to_string(magx);
  std::string str_mgy = std::to_string(magy);
  std::string str_mgz = std::to_string(magz);
  std::string str_r = std::to_string(r);
  std::string str =
      "{\"x\":" + str_x + ",\"y\":" + str_y + ",\"z\":" + str_z +
      ",\"ax\":" + str_ax + ",\"ay\":" + str_ay + ",\"az\":" + str_az +
      ",\"gx\":" + str_gx + ",\"gy\":" + str_gy + ",\"gz\":" + str_gz +
      ",\"magx\":" + str_mgx + ",\"magy\":" + str_mgy + ",\"magz\":" + str_mgz +
      ",\"rotaryIndex\":" + str_r + ",\"deviceId\":" + "\"" + DEVICE_ID + "\"" +
      ",\"rotaryButton\":" + (sw ? "true}" : "false}");
  char *temp = new char[str.length() + 1];
  strcpy(temp, str.c_str());
  client.publish(MQTT_JOYSTICK_PUBLISH, temp);
  Serial.println(temp);
}
#endif

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  initWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  connect_to_broker();
  Serial.println("Start transfer");

#ifdef ENABLE_VIRTUAL_MOUSE
  Serial.println("Starting BLE work!");
  bleMouse.begin();
#endif

  /** Khai I2C cho GPIO 21 GPIO 22 */
  Wire.begin(SDA, SCL);
  delay(1000);

  if (!mpu9250.setup(0x68))
  { 
    while (1)
    {
      Serial.println("MPU connection failed. Please check your connection.");
      delay(3000);
    }
  }

  mpu9250.verbose(true);
  delay(2000);
  mpu9250.calibrateAccelGyro();
  delay(2000);
  mpu9250.calibrateMag();
  mpu9250.verbose(false);

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
}

void loop()
{
  mpu9250.update();
  encoder.tick();
  button.loop();

  if (button.isPressed())
  {
    buttonState = true;
#ifdef ENABLE_VIRTUAL_MOUSE
    bleMouse.click();
#endif
    Serial.println(buttonState);
  }

  if (button.isReleased())
  {
    buttonState = false;
    Serial.println(buttonState);
  }

#ifdef ENABLE_VIRTUAL_MOUSE
  if (bleMouse.isConnected())
  {
    float x = mpu9250.getRoll();
    float z = mpu9250.getYaw();
    int moveVertical = (int)((x - angleX) * 3);
    angleX = x;
    int moveHorizon = (int)((z - angleZ) * 3);
    angleZ = z;
    Serial.print("Move vertical : ");
    Serial.println(moveVertical);
    Serial.print("Move horizon : ");
    Serial.println(moveHorizon);
    bleMouse.move(-moveHorizon, -moveVertical);
    delay(100);

    int newPos = encoder.getPosition();
    if (lastPos != newPos)
    {
      bleMouse.move(0, 0, newPos - lastPos);
      lastPos = newPos;
      delay(100);
    }
  }
#endif

  if (millis() - timer > 1000)
  {
    float ax = accelXKalmanFilter.updateEstimate(mpu9250.getAccX());
    float ay = accelYKalmanFilter.updateEstimate(mpu9250.getAccY());
    float az = accelZKalmanFilter.updateEstimate(mpu9250.getAccZ());
    float gx = gyroXKalmanFilter.updateEstimate(mpu9250.getGyroX());
    float gy = gyroYKalmanFilter.updateEstimate(mpu9250.getGyroY());
    float gz = gyroZKalmanFilter.updateEstimate(mpu9250.getGyroZ());
    float magx = mpu9250.getMagX();
    float magy = mpu9250.getMagY();
    float magz = magZKalmanFilter.updateEstimate(mpu9250.getMagZ());
    int r = encoder.getPosition();
    timer = millis();

    publish(mpu9250.getRoll(), mpu9250.getPitch(), mpu9250.getYaw(), ax, ay, az, gx, gy, gz, magx, magy, magz, r, buttonState);
  }
}
