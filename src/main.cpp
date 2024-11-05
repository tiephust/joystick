#include <Arduino.h> // Bao gồm thư viện Arduino cơ bản
#include <ezButton.h> // Bao gồm thư viện để xử lý nút bấm
#include <MPU9250.h> // Bao gồm thư viện cho cảm biến MPU9250
#include <Wire.h> // Bao gồm thư viện cho giao thức I2C
#include <RotaryEncoder.h> // Bao gồm thư viện cho rotary encoder
#include <LiquidCrystal_I2C.h> // Bao gồm thư viện cho LCD I2C

// Định nghĩa chiều dài, rộng cho màn LCD
#define SCREEN_WIDTH 16
#define SCREEN_HEIGHT 2

#define LCD_RESET 4

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

bool buttonState = false; // Trạng thái nút bấm của rotary encoder
static int lastPos = 0;   // Giá trị gần nhất của rotary encoder
long timer = 0; // Biến để theo dõi thời gian

float angleX = 0, angleY = 0, angleZ = 0, gyroX = 0, gyroY = 0, gyroZ = 0, tempMPU = 0; // Biến để lưu góc, gia tốc X, Y, Z và nhiệt độ

/** Khai báo cho rotary encoder switch */
ezButton button(SW); // Khai báo nút bấm
/** Khai báo rotary encoder */
RotaryEncoder encoder(CLK, DT, RotaryEncoder::LatchMode::TWO03); // Khai báo rotary encoder với chân CLK và DT

/** Khai báo LCD I2C */
LiquidCrystal_I2C lcd(0x27, SCREEN_WIDTH, SCREEN_HEIGHT); // Địa chỉ I2C cho LCD

MPU9250 mpu; // Khởi tạo đối tượng cho cảm biến MPU9250

void setup() {
    Serial.begin(115200); // Khởi động Serial với tốc độ 115200
    Wire.begin(SDA, SCL);
    lcd.begin(SCREEN_WIDTH, SCREEN_HEIGHT);
    lcd.backlight();
    lcd.print("Joystick");
    delay(1000);

    // Khởi tạo cảm biến MPU9250
    if (!mpu.setup(0x68)) {
        Serial.println("Failed to find MPU9250 chip");
        while (1) {
            delay(10);
        }
    }

    Serial.println("MPU9250 Found!");
    delay(1000);

    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(SW, INPUT_PULLUP);
}

void loop() {
    // Đọc dữ liệu cảm biến MPU9250
    mpu.update();

    angleX = mpu.getAccX();
    angleY = mpu.getAccY();
    angleZ = mpu.getAccZ();
    gyroX = mpu.getGyroX();
    gyroY = mpu.getGyroY();
    gyroZ = mpu.getGyroZ();
    tempMPU = mpu.getTemperature();

    // Cập nhật và đọc giá trị từ rotary encoder
    encoder.tick();
    button.loop(); // Cập nhật trạng thái nút bấm

    int newPos = encoder.getPosition();
    bool shouldUpdateLCD = false;

    // Kiểm tra nếu giá trị rotary encoder đã thay đổi
    if (newPos != lastPos) {
        lastPos = newPos;
        shouldUpdateLCD = true; // Đánh dấu rằng có thay đổi
    }

    // Kiểm tra trạng thái nút bấm
    if (button.isPressed()) {
        buttonState = true; // Đặt trạng thái nút bấm thành true
        shouldUpdateLCD = true; // Đánh dấu rằng có thay đổi
    }

    if (button.isReleased()) {
        buttonState = false; // Đặt trạng thái nút bấm thành false
        shouldUpdateLCD = true; // Đánh dấu rằng có thay đổi
    }

    // Nếu có bất kỳ thay đổi nào, hiển thị thông số
    if (shouldUpdateLCD) {
        lcd.clear();
        lcd.setCursor(0, 0);

        // Hiển thị thông số rotary encoder
        lcd.print("Encoder: ");
        lcd.print(newPos);
        delay(3000);
        lcd.clear();

        // Hiển thị Acceleration
        lcd.setCursor(0, 0);
        lcd.print("A   :");
        lcd.print("X: ");
        lcd.print(angleX);
        lcd.setCursor(0, 1);
        lcd.print(" Y: ");
        lcd.print(angleY);
        lcd.print(" Z: ");
        lcd.print(angleZ);
        delay(3000); // Giữ màn hình trong 3 giây

        // Hiển thị Gyro 
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.println("G:");
        lcd.print("X: ");
        lcd.print(gyroX);
        lcd.setCursor(0, 1);
        lcd.print("Y: ");
        lcd.print(gyroY);
        lcd.print(" Z: ");
        lcd.print(gyroZ);
        delay(3000); // Giữ màn hình trong 3 giây

        // Hiển thị Temperature
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp:");
        lcd.print(tempMPU);
        delay(5000); // Giữ màn hình trong 5 giây
    }
}
