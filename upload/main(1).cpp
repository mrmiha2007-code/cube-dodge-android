#include <iostream>
#include <algorithm>
#include "raylib.h"
#if defined(PLATFORM_ANDROID)
#include <atomic>
#include <jni.h>
#endif
//#include "raylib-5.5_linux_amd64/include/raylib.h"
//#include "raylib-5.5_linux_amd64/include/raymath.h"
//cd mygame/
//возможно ll
//g++ -I../. main.cpp -L../raylib-5.5_linux_amd64/lib -l:libraylib.a
//./a.out
//cd "C:\Users\Kokoro Zakuro\code++\mygame"
//g++ main.cpp -IC:\raylib\raylib-5.5_win64_mingw-w64\include -LC:\raylib\raylib-5.5_win64_mingw-w64\lib -lraylib -lopengl32 -lgdi32 -lwinmm -o game.exe
//.\game.exe  
//Задачи: Бустеры, начальное меню, выбор жизней/сложности, сделать боссфайт вместо лвла 10,
//Идеи: сделать вокруг героя ауру после потери жизни, которая будет ломать кубы, потестить визуальную ширину лазера, Сделать мультиплаер пойнтов за пролёт или как бонус
//Советы: Сферы летают за мейн чаром
//Надо исправить: поменять скорость накопления дешей,
//Заметки: Убраться на столе

enum AndroidHapticEvent
{
    ANDROID_HAPTIC_NONE = 0,
    ANDROID_HAPTIC_LIFE_LOST = 1,
    ANDROID_HAPTIC_ULTIMATE = 2
};

enum AndroidAdaptiveTriggerPulse
{
    ANDROID_TRIGGER_PULSE_NONE = 0,
    ANDROID_TRIGGER_PULSE_ULTIMATE = 1
};

enum DirectUsbButton
{
    DIRECT_USB_DPAD_UP = 1 << 0,
    DIRECT_USB_DPAD_DOWN = 1 << 1,
    DIRECT_USB_DPAD_LEFT = 1 << 2,
    DIRECT_USB_DPAD_RIGHT = 1 << 3,
    DIRECT_USB_SQUARE = 1 << 4,
    DIRECT_USB_CROSS = 1 << 5,
    DIRECT_USB_CIRCLE = 1 << 6,
    DIRECT_USB_TRIANGLE = 1 << 7,
    DIRECT_USB_L1 = 1 << 8,
    DIRECT_USB_R1 = 1 << 9,
    DIRECT_USB_L2 = 1 << 10,
    DIRECT_USB_R2 = 1 << 11,
    DIRECT_USB_CREATE = 1 << 12,
    DIRECT_USB_OPTIONS = 1 << 13,
    DIRECT_USB_L3 = 1 << 14,
    DIRECT_USB_R3 = 1 << 15,
    DIRECT_USB_PS = 1 << 16,
    DIRECT_USB_TOUCHPAD = 1 << 17
};

struct DirectUsbGamepadFrame
{
    bool connected = false;
    unsigned int buttonsDown = 0;
    unsigned int buttonsPressed = 0;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
};

#if defined(PLATFORM_ANDROID)
//Java-часть Android опрашивает это значение и отправляет эффект в DualSense.
//Так ПК-версия не получает ни зависимостей Android, ни изменений управления.
static std::atomic<int> androidHapticEvent{ANDROID_HAPTIC_NONE};
static std::atomic<int> androidUltimateReady{0};
static std::atomic<int> androidAdaptiveTriggerPulse{ANDROID_TRIGGER_PULSE_NONE};
static std::atomic<int> androidUsbInputConnected{0};
static std::atomic<int> androidUsbButtons{0};
static std::atomic<int> androidUsbPressedButtons{0};
static std::atomic<int> androidUsbLeftX{128};
static std::atomic<int> androidUsbLeftY{128};
static std::atomic<int> androidUsbLeftTrigger{0};
static std::atomic<int> androidUsbRightTrigger{0};

void requestAndroidHaptic(AndroidHapticEvent event)
{
    androidHapticEvent.store((int)event, std::memory_order_release);
}

void setAndroidUltimateReady(bool ready)
{
    androidUltimateReady.store(ready ? 1 : 0, std::memory_order_release);
}

void requestAndroidAdaptiveTriggerPulse(AndroidAdaptiveTriggerPulse pulse)
{
    androidAdaptiveTriggerPulse.store((int)pulse, std::memory_order_release);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_cubedodge_HapticBridge_consumeHapticEvent(JNIEnv *, jclass)
{
    return androidHapticEvent.exchange(ANDROID_HAPTIC_NONE, std::memory_order_acq_rel);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_cubedodge_HapticBridge_isUltimateReady(JNIEnv *, jclass)
{
    return androidUltimateReady.load(std::memory_order_acquire) != 0 ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_cubedodge_HapticBridge_consumeAdaptiveTriggerPulse(JNIEnv *, jclass)
{
    return androidAdaptiveTriggerPulse.exchange(
        ANDROID_TRIGGER_PULSE_NONE, std::memory_order_acq_rel);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_cubedodge_DualSenseUsb_updateNativeGamepad(
    JNIEnv *, jclass, jboolean connected, jint buttons,
    jint leftX, jint leftY, jint leftTrigger, jint rightTrigger)
{
    if (connected == JNI_FALSE) {
        androidUsbInputConnected.store(0, std::memory_order_release);
        androidUsbButtons.store(0, std::memory_order_release);
        androidUsbPressedButtons.store(0, std::memory_order_release);
        return;
    }

    int previousButtons = androidUsbButtons.exchange((int)buttons, std::memory_order_acq_rel);
    androidUsbPressedButtons.fetch_or(
        ((int)buttons) & ~previousButtons, std::memory_order_acq_rel);
    androidUsbLeftX.store((int)leftX, std::memory_order_release);
    androidUsbLeftY.store((int)leftY, std::memory_order_release);
    androidUsbLeftTrigger.store((int)leftTrigger, std::memory_order_release);
    androidUsbRightTrigger.store((int)rightTrigger, std::memory_order_release);
    androidUsbInputConnected.store(1, std::memory_order_release);
}

DirectUsbGamepadFrame pollDirectUsbGamepad()
{
    DirectUsbGamepadFrame frame;
    frame.connected = androidUsbInputConnected.load(std::memory_order_acquire) != 0;
    if (!frame.connected) return frame;

    frame.buttonsDown = (unsigned int)androidUsbButtons.load(std::memory_order_acquire);
    frame.buttonsPressed = (unsigned int)androidUsbPressedButtons.exchange(
        0, std::memory_order_acq_rel);
    frame.leftX = (androidUsbLeftX.load(std::memory_order_acquire) - 127.5f)/127.5f;
    frame.leftY = (androidUsbLeftY.load(std::memory_order_acquire) - 127.5f)/127.5f;
    frame.leftTrigger = androidUsbLeftTrigger.load(std::memory_order_acquire)/255.0f;
    frame.rightTrigger = androidUsbRightTrigger.load(std::memory_order_acquire)/255.0f;
    return frame;
}
#else
void requestAndroidHaptic(AndroidHapticEvent) {}
void setAndroidUltimateReady(bool) {}
void requestAndroidAdaptiveTriggerPulse(AndroidAdaptiveTriggerPulse) {}
DirectUsbGamepadFrame pollDirectUsbGamepad() { return {}; }
#endif

struct ExplosionParticle
{
    float x;
    float y;
    float speedX;
    float speedY;
    float size;
    int life;
};

void createExplosion(ExplosionParticle particles[], int &particleIndex, int maxParticles,
                     int x, int y, int sizeX, int sizeY)
{
    //Не создаём частицы для кубов, которые сейчас находятся далеко за экраном
    if (x + sizeX < 0 || x > 800 || y + sizeY < 0 || y > 600) return;

    for (int i = 0; i < 8; i++) {
        particles[particleIndex].x = x + sizeX/2;
        particles[particleIndex].y = y + sizeY/2;
        particles[particleIndex].speedX = GetRandomValue(-45,45)/10.0f;
        particles[particleIndex].speedY = GetRandomValue(-45,45)/10.0f;
        particles[particleIndex].size = GetRandomValue(6,11);
        particles[particleIndex].life = 30;
        particleIndex = (particleIndex + 1)%maxParticles;
    }
}

void drawPixelHeart(int x, int y, int pixelSize, Color color, int outlineSize, bool roundedOutline)
{
    const char *heartShape[8] = {
        "011000110",
        "111101111",
        "111111111",
        "111111111",
        "011111110",
        "001111100",
        "000111000",
        "000010000"
    };

    if (outlineSize > 0) {
        for (int row = 0; row < 8; row++) {
            for (int column = 0; column < 9; column++) {
                if (heartShape[row][column] == '1') {
                    if (roundedOutline) {
                        Rectangle outlineRect{
                            (float)(x + column*pixelSize - outlineSize),
                            (float)(y + row*pixelSize - outlineSize),
                            (float)(pixelSize + outlineSize*2),
                            (float)(pixelSize + outlineSize*2)
                        };
                        DrawRectangleRounded(outlineRect, 0.6f, 4, BLACK);
                    }
                    else {
                        DrawRectangle(x + column*pixelSize - outlineSize,
                            y + row*pixelSize - outlineSize,
                            pixelSize + outlineSize*2, pixelSize + outlineSize*2, BLACK);
                    }
                }
            }
        }
    }

    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 9; column++) {
            if (heartShape[row][column] == '1')
                DrawRectangle(x + column*pixelSize, y + row*pixelSize,
                    pixelSize, pixelSize, color);
        }
    }
}

int main()
{
    //Создаём окно и ограничиваем фреймрейт
#if defined(PLATFORM_ANDROID)
    //На телефоне raylib создаёт полноэкранное окно с реальным разрешением дисплея.
    //Игровую логику оставляем в исходных 800x600 и масштабируем только картинку.
    SetConfigFlags(FLAG_VSYNC_HINT);
#endif
    InitWindow(800, 600, "Test");
    SetTargetFPS(60);

#if defined(PLATFORM_ANDROID)
    RenderTexture2D gameTarget = LoadRenderTexture(800, 600);
    SetTextureFilter(gameTarget.texture, TEXTURE_FILTER_POINT);
#endif
    
    //счётчики
    bool Gameover = 0;
    int Score = 0;
    int lvl = 1;
    int Best = 0;
    int speedBoost = 1;
    int DashCharges = 1;
    int DashChargesSec = 0;
    int DashEffect = 0;
    int invFrames = 0;
    int lives = 3;
    int lifeInvFrames = 0;
    const int lifeInvMax = 120;
    int hideHUD = 0;
    int LASERTransparency = 0;
    int ultimateCharge = 0;
    const int ultimateMax = 900;
    int ultimateLockFrames = 0;
    bool ultimateNeedsRelease = false;
    int side1Delay = 0;
    int side2Delay = 0;
    int side3Delay = 0;
    int side4Delay = 0;
    int ultimateFlashFrames = 0;
    float ultimateRingRadius = 0;
    float ultimateRingX = 400;
    float ultimateRingY = 300;
    int ultimateRingAlpha = 0;
    int screenShakeFrames = 0;
    bool settingsOpen = false;
    int settingsSelected = 0;
    const int settingsCount = 1;
    bool accelerationEnabled = false;
    const int maxParticles = 64;
    ExplosionParticle particles[maxParticles]{};
    int particleIndex = 0;

    //Управление с геймпада
    int gamepad = -1;
    const float gamepadDeadzone = 0.25f;

    //ратейшаны
    bool LASERrotarion = 0;
    LASERrotarion = GetRandomValue(0,1);
    bool side1rotation = 0;
    side1rotation = GetRandomValue(0,1);
    bool side2rotation = 0;
    side2rotation = GetRandomValue(0,1);
    bool side3rotation = 0;
    side3rotation = GetRandomValue(0,1);
    bool side4rotation = 0;
    side4rotation = GetRandomValue(0,1);

    //Заранее создаём коллилионные квадраты, чтобы не ругался
    Rectangle playerRect{0,0,0,0};
    Rectangle enemy1Rect{0,0,0,0};
    Rectangle enemy2Rect{0,0,0,0};
    Rectangle side1Rect{0,0,0,0};
    Rectangle side3Rect{0,0,0,0};
    Rectangle enemy3Rect{0,0,0,0};
    Rectangle LASERRect{0,0,0,0};
    Rectangle side2Rect{0,0,0,0};
    Rectangle side4Rect{0,0,0,0};
    Rectangle enemy4Rect{0,0,0,0};

    //анлокеры
    bool level1Unlocker = 0;
    bool level2Unlocker = 0;
    bool level3Unlocker = 0;
    bool level4Unlocker = 0;
    bool level5Unlocker = 0;
    bool level6Unlocker = 0;
    bool level7Unlocker = 0;
    bool level8Unlocker = 0;
    bool level9Unlocker = 0;
    bool level10Unlocker = 0;

    // Нужно ли выносить из мейна если всё и так работает?
    struct Character 
    {
        int x;
        int y;
        int size_x;
        int size_y;
        int speed;
    }; 

        //Выбираем действительно новую полосу, а не почти ту же высоту.
        //Так смена траектории заметна даже при неудачном результате рандома.
        auto nextHorizontalY = [](int currentY, int sizeY) {
            const int maxY = 600 - sizeY;
            int nextY = currentY;

            for (int attempt = 0; attempt < 8 && std::abs(nextY - currentY) < 80; attempt++)
                nextY = GetRandomValue(0, maxY);

            if (std::abs(nextY - currentY) < 80) {
                if (currentY < maxY/2) nextY = std::min(currentY + 80, maxY);
                else nextY = std::max(currentY - 80, 0);
            }
            return nextY;
        };

        Character player{375,275,50,50,5};
        Character enemy1{GetRandomValue(0,760),-50,40,40,4};
        Character enemy2{GetRandomValue(0,760),-300,40,40,4};
        Character enemy3{GetRandomValue(0,760),-500,40,40,4};
        Character enemy4{GetRandomValue(0,760),-700,40,40,4};
        Character side1{-300,GetRandomValue(0,560),40,40,4};
        if (side1rotation == 1) {
            side1.x = 1100;
            side1.speed = -side1.speed;
        }
        Character side2{-300,GetRandomValue(0,560),40,40,4};
        if (side2rotation == 1) {
            side2.x = 1100;
            side2.speed = -side2.speed;
        }
        Character side3{-300,GetRandomValue(0,560),40,40,4};
        if (side3rotation == 1) {
            side3.x = 1100;
            side3.speed = -side3.speed;
        }
        Character side4{-300,GetRandomValue(0,560),40,40,4};
        if (side4rotation == 1) {
            side4.x = 1100;
            side4.speed = -side4.speed;
        }
        Character LASER{-400, 0, 2, 600, 7};
        if (LASERrotarion == 1) {
            LASER.x = 1200;
            LASER.speed = -LASER.speed;
        }


    while (!WindowShouldClose())
    {
        //Ищем подключённый геймпад, в том числе если его подключили после запуска игры
        if (gamepad == -1 || !IsGamepadAvailable(gamepad)) {
            gamepad = -1;
            for (int i = 0; i < 4; i++) {
                if (IsGamepadAvailable(i)) {
                    gamepad = i;
                    break;
                }
            }
        }

        DirectUsbGamepadFrame directUsbGamepad = pollDirectUsbGamepad();
        auto gamepadButtonPressed = [&](int raylibButton, unsigned int directUsbButton) {
            if (directUsbGamepad.connected)
                return (directUsbGamepad.buttonsPressed & directUsbButton) != 0;
            return gamepad != -1 && IsGamepadButtonPressed(gamepad, raylibButton);
        };
        auto gamepadButtonDown = [&](int raylibButton, unsigned int directUsbButton) {
            if (directUsbGamepad.connected)
                return (directUsbGamepad.buttonsDown & directUsbButton) != 0;
            return gamepad != -1 && IsGamepadButtonDown(gamepad, raylibButton);
        };

        float leftStickX = 0.0f;
        float leftStickY = 0.0f;
        if (directUsbGamepad.connected) {
            leftStickX = directUsbGamepad.leftX;
            leftStickY = directUsbGamepad.leftY;
        } else if (gamepad != -1) {
            leftStickX = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
            leftStickY = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
        }

        bool moveRight = IsKeyDown(KEY_D) || leftStickX > gamepadDeadzone;
        bool moveLeft = IsKeyDown(KEY_A) || leftStickX < -gamepadDeadzone;
        bool moveUp = IsKeyDown(KEY_W) || leftStickY < -gamepadDeadzone;
        bool moveDown = IsKeyDown(KEY_S) || leftStickY > gamepadDeadzone;

        //Options открывает настройки поверх игры: сама игра при этом не ставится на паузу.
        if (IsKeyPressed(KEY_F10) ||
            gamepadButtonPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT, DIRECT_USB_OPTIONS)) {
            settingsOpen = !settingsOpen;
        }

        if (settingsOpen) {
            bool selectPrevious = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_LEFT_FACE_UP, DIRECT_USB_DPAD_UP) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT, DIRECT_USB_DPAD_LEFT);
            bool selectNext = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN, DIRECT_USB_DPAD_DOWN) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT, DIRECT_USB_DPAD_RIGHT);

            if (selectPrevious)
                settingsSelected = (settingsSelected + settingsCount - 1)%settingsCount;
            if (selectNext)
                settingsSelected = (settingsSelected + 1)%settingsCount;

            bool toggleSetting = IsKeyPressed(KEY_ENTER) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT, DIRECT_USB_SQUARE);
            if (toggleSetting && settingsSelected == 0)
                accelerationEnabled = !accelerationEnabled;
        }

        //выход
        if (IsKeyPressed(KEY_ESCAPE)) break;
        //Ускорение сначала нужно включить в настройках, затем удерживать L1 + R1.
        //Пробел оставлен как клавиатурный вариант для запуска и тестов через VS Code.
        bool accelerationHeld = (IsKeyDown(KEY_SPACE) ||
            (gamepadButtonDown(GAMEPAD_BUTTON_LEFT_TRIGGER_1, DIRECT_USB_L1) &&
             gamepadButtonDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_1, DIRECT_USB_R1)));
        if (accelerationEnabled && accelerationHeld) speedBoost = 2;
        //Переключатель худа: H на клавиатуре или круг на DualSense
        if (IsKeyPressed(KEY_H) ||
            gamepadButtonPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, DIRECT_USB_CIRCLE)) hideHUD = 1 - 1*hideHUD;
        //Быстрый лвл скип
        if (IsKeyPressed(KEY_ONE)) Score = 0;
        if (IsKeyPressed(KEY_TWO)) Score = 500;
        if (IsKeyPressed(KEY_THREE)) Score = 1800;
        if (IsKeyPressed(KEY_FOUR)) Score = 4000;
        if (IsKeyPressed(KEY_FIVE)) Score = 8000;
        if (IsKeyPressed(KEY_SIX)) Score = 12000;
        //Фулскрин
        if (IsKeyPressed(KEY_F11) ||
            gamepadButtonPressed(GAMEPAD_BUTTON_MIDDLE_LEFT, DIRECT_USB_CREATE)) {
            ToggleFullscreen();
        }


        if (Gameover == 0) {
            //Ульта заряжается за 15 секунд. Для активации нужно одновременно нажать L2 и R2
            if (ultimateLockFrames > 0) ultimateLockFrames -= 1;
            if (ultimateFlashFrames == 0 && ultimateCharge < ultimateMax) ultimateCharge += 1;
            if (ultimateCharge > ultimateMax) ultimateCharge = ultimateMax;

            bool ultimatePressed = directUsbGamepad.connected ?
                (directUsbGamepad.leftTrigger > 0.5f && directUsbGamepad.rightTrigger > 0.5f) :
                (gamepad != -1 &&
                 GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER) > 0.5f &&
                 GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.5f);
            if (!ultimatePressed) ultimateNeedsRelease = false;

            bool ultimateStarted = false;
            if (ultimateCharge == ultimateMax && ultimatePressed && ultimateFlashFrames == 0 &&
                ultimateLockFrames == 0 && !ultimateNeedsRelease) {
                ultimateFlashFrames = 3;
                ultimateCharge = 0;
                ultimateRingX = player.x + player.size_x/2;
                ultimateRingY = player.y + player.size_y/2;
                invFrames += 6;
                ultimateStarted = true;
                requestAndroidHaptic(ANDROID_HAPTIC_ULTIMATE);
                requestAndroidAdaptiveTriggerPulse(ANDROID_TRIGGER_PULSE_ULTIMATE);
            }

            if (!ultimateStarted && ultimateFlashFrames > 0) {
                ultimateFlashFrames -= 1;

                if (ultimateFlashFrames == 0) {
                    //Создаём осколки только у активных на этом уровне кубов
                    if (lvl >= 1) createExplosion(particles, particleIndex, maxParticles,
                        enemy1.x, enemy1.y, enemy1.size_x, enemy1.size_y);
                    if (lvl >= 2) createExplosion(particles, particleIndex, maxParticles,
                        enemy2.x, enemy2.y, enemy2.size_x, enemy2.size_y);
                    if (lvl >= 4) createExplosion(particles, particleIndex, maxParticles,
                        side1.x, side1.y, side1.size_x, side1.size_y);
                    if (lvl >= 4) createExplosion(particles, particleIndex, maxParticles,
                        side3.x, side3.y, side3.size_x, side3.size_y);
                    if (lvl >= 5) createExplosion(particles, particleIndex, maxParticles,
                        enemy3.x, enemy3.y, enemy3.size_x, enemy3.size_y);
                    if (lvl >= 7) createExplosion(particles, particleIndex, maxParticles,
                        side2.x, side2.y, side2.size_x, side2.size_y);
                    if (lvl >= 7) createExplosion(particles, particleIndex, maxParticles,
                        side4.x, side4.y, side4.size_x, side4.size_y);
                    if (lvl >= 8) createExplosion(particles, particleIndex, maxParticles,
                        enemy4.x, enemy4.y, enemy4.size_x, enemy4.size_y);

                //Разводим кубы по времени, чтобы после ульты они не возвращались одной группой
                enemy1.y = -enemy1.size_y - std::abs(enemy1.speed)*20;
                enemy1.x = GetRandomValue(0,800-enemy1.size_x);
                enemy2.y = -enemy2.size_y - std::abs(enemy2.speed)*70;
                enemy2.x = GetRandomValue(0,800-enemy2.size_x);
                enemy3.y = -enemy3.size_y - std::abs(enemy3.speed)*120;
                enemy3.x = GetRandomValue(0,800-enemy3.size_x);
                enemy4.y = -enemy4.size_y - std::abs(enemy4.speed)*170;
                enemy4.x = GetRandomValue(0,800-enemy4.size_x);

                side1.y = nextHorizontalY(side1.y, side1.size_y);
                side1.x = side1.speed > 0 ? -side1.size_x : 800;
                side2.y = nextHorizontalY(side2.y, side2.size_y);
                side2.x = side2.speed > 0 ? -side2.size_x : 800;
                side3.y = nextHorizontalY(side3.y, side3.size_y);
                side3.x = side3.speed > 0 ? -side3.size_x : 800;
                side4.y = nextHorizontalY(side4.y, side4.size_y);
                side4.x = side4.speed > 0 ? -side4.size_x : 800;

                //Оставляем безопасный разброс по времени, но каждый раз меняем
                //порядок возвращения активных горизонтальных кубов.
                int horizontalDelaySlots[4] = {
                    GetRandomValue(35,60), GetRandomValue(80,110),
                    GetRandomValue(125,155), GetRandomValue(170,205)
                };
                int activeHorizontalCount = lvl >= 7 ? 4 : (lvl >= 4 ? 2 : 0);
                for (int i = activeHorizontalCount - 1; i > 0; i--)
                    std::swap(horizontalDelaySlots[i],
                        horizontalDelaySlots[GetRandomValue(0,i)]);

                side1Delay = 0;
                side2Delay = 0;
                side3Delay = 0;
                side4Delay = 0;
                if (lvl >= 4) {
                    side1Delay = horizontalDelaySlots[0];
                    side3Delay = horizontalDelaySlots[1];
                }
                if (lvl >= 7) {
                    side2Delay = horizontalDelaySlots[2];
                    side4Delay = horizontalDelaySlots[3];
                }

                    ultimateRingRadius = 20;
                    ultimateRingAlpha = 220;
                    screenShakeFrames = 12;
                }
            }

            //заряды рывков
            if (DashCharges < 2) DashChargesSec += 1*speedBoost;
            if (DashChargesSec >= 180) {
               DashCharges += 1;
               DashChargesSec -= 180;
            }
            if ((IsKeyPressed(KEY_LEFT_SHIFT) ||
                (!settingsOpen &&
                    gamepadButtonPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT, DIRECT_USB_SQUARE))) &&
                DashCharges > 0) {
                DashEffect += 8;
                invFrames += 8;
                DashCharges -= 1;
            }
            if (DashEffect > 0 && ultimateFlashFrames == 0) {
                if (moveRight) player.x += 15;
                if (moveLeft) player.x -= 15;
                if (moveUp) player.y -= 15;
                if (moveDown) player.y += 15;
                DashEffect -= 1;
            }
            if (invFrames > 0) invFrames -= 1;
            if (lifeInvFrames > 0) lifeInvFrames -= 1;

            //лвлап
            if (Gameover == 0) {
                if(Score == 0) {
                    lvl = 1;
                }
                if(Score >= 300 && Score < 800) {
                    lvl = 2;
                }
                if(Score >= 800 && Score < 2000) {
                    lvl = 3;
                }
                if(Score >= 2000 && Score < 4000) {
                    lvl = 4;
                }
                if(Score >= 4000 && Score < 6500) {
                    lvl = 5;
                }
                if(Score >= 6500 && Score < 9500) {
                    lvl = 6;
                }
                if(Score >= 9500 && Score < 13000) {
                    lvl = 7;
                }
                if(Score >= 13000 && Score < 16500) {
                    lvl = 8;
                }
                if(Score >= 16500 && Score < 20000) {
                    lvl = 9;
                }
                if(Score >= 20000) {
                    lvl = 10;
                }
            }


            //Уровни сложности 
            //дефолт
            // enemy1.speed = 4;
            // enemy2.speed = 5;
            // enemy3.speed = 8;
            // enemy4.speed = 11;
            // side1.speed = 7;
            // side2.speed = 9;
            // side3.speed = 8;
            // side4.speed = 10;
            // LASER.speed = 7;

            if (lvl == 1 && level1Unlocker == 0) {
                enemy1.speed = 4;
                enemy2.speed = 5;
                enemy3.speed = 8;
                enemy4.speed = 13;
                side1.speed = side1.speed < 0 ? -7 : 7;
                side2.speed = side2.speed < 0 ? -9 : 9;
                side3.speed = side3.speed < 0 ? -8 : 8;
                side4.speed = side4.speed < 0 ? -10 : 10;
                LASER.speed = LASER.speed < 0 ? -7 : 7;
                level1Unlocker = 1;
            }
            if (lvl == 2 && level2Unlocker == 0) {
                enemy1.speed = 4;
                enemy2.speed = 5;
                level2Unlocker = 1;
            }
            if (lvl == 3 && level3Unlocker == 0) {
                enemy1.speed = 6;
                enemy2.speed = 7;
                level3Unlocker = 1;
            }
            if (lvl == 4 && level4Unlocker == 0) {
                enemy1.speed = 6;
                enemy2.speed = 7;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed));
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed));
                level4Unlocker = 1;
            }
            if (lvl == 5 && level5Unlocker == 0) {
                enemy1.speed = 6;
                enemy2.speed = 7;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed)+1);
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed)+1);
                enemy3.speed = 8;
                level5Unlocker = 1;
            }
            if (lvl == 6 && level6Unlocker == 0) {
                enemy1.speed = 7;
                enemy2.speed = 8;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed));
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed));
                enemy3.speed = 9;
                LASER.speed = LASER.speed/(std::abs(LASER.speed))*(std::abs(LASER.speed));
                level6Unlocker = 1;
            }
            if (lvl == 7 && level7Unlocker == 0) {
                enemy1.speed = 8;
                enemy2.speed = 9;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed));
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed));
                enemy3.speed = 10;
                LASER.speed = LASER.speed/(std::abs(LASER.speed))*(std::abs(LASER.speed));
                side2.speed = side2.speed/(std::abs(side2.speed))*(std::abs(side2.speed));
                side4.speed = side4.speed/(std::abs(side4.speed))*(std::abs(side4.speed));
                level7Unlocker = 1;
            }
            if (lvl == 8 && level8Unlocker == 0) {
                enemy1.speed = 9;
                enemy2.speed = 10;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed));
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed));
                enemy3.speed = 11;
                LASER.speed = LASER.speed/(std::abs(LASER.speed))*(std::abs(LASER.speed)+2);
                side2.speed = side2.speed/(std::abs(side2.speed))*(std::abs(side2.speed));
                side4.speed = side4.speed/(std::abs(side4.speed))*(std::abs(side4.speed));
                enemy4.speed = 11;
                level8Unlocker = 1;
            }
            if (lvl == 9 && level9Unlocker == 0) {
                enemy1.speed = 9;
                enemy2.speed = 10;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed)+1);
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed));
                enemy3.speed = 11;
                LASER.speed = LASER.speed/(std::abs(LASER.speed))*(std::abs(LASER.speed));
                side2.speed = side2.speed/(std::abs(side2.speed))*(std::abs(side2.speed)+1);
                side4.speed = side4.speed/(std::abs(side4.speed))*(std::abs(side4.speed));
                enemy4.speed = 11;
                level9Unlocker = 1;
            }
            if (lvl == 10 && level10Unlocker == 0) {
                enemy1.speed = 10;
                enemy2.speed = 11;
                side1.speed = side1.speed/(std::abs(side1.speed))*(std::abs(side1.speed));
                side3.speed = side3.speed/(std::abs(side3.speed))*(std::abs(side3.speed)+1);
                enemy3.speed = 12;
                LASER.speed = LASER.speed/(std::abs(LASER.speed))*(std::abs(LASER.speed)+1);
                side2.speed = side2.speed/(std::abs(side2.speed))*(std::abs(side2.speed));
                side4.speed = side4.speed/(std::abs(side4.speed))*(std::abs(side4.speed)+1);
                enemy4.speed = 12;
                level10Unlocker = 1;
            }
            
            

            //Движение игрока, движение врага
            if (ultimateFlashFrames == 0 && moveRight) player.x += player.speed*speedBoost;
            if (ultimateFlashFrames == 0 && moveLeft) player.x -= player.speed*speedBoost;
            if (ultimateFlashFrames == 0 && moveUp) player.y -= player.speed*speedBoost;
            if (ultimateFlashFrames == 0 && moveDown) player.y += player.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 1) enemy1.y += enemy1.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 2) enemy2.y += enemy2.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 4 && side1Delay > 0) side1Delay -= 1;
            else if (ultimateFlashFrames == 0 && lvl >= 4) side1.x += side1.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 4 && side3Delay > 0) side3Delay -= 1;
            else if (ultimateFlashFrames == 0 && lvl >= 4) side3.x += side3.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 5) enemy3.y += enemy3.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 6) LASER.x += LASER.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 7 && side2Delay > 0) side2Delay -= 1;
            else if (ultimateFlashFrames == 0 && lvl >= 7) side2.x += side2.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 7 && side4Delay > 0) side4Delay -= 1;
            else if (ultimateFlashFrames == 0 && lvl >= 7) side4.x += side4.speed*speedBoost;
            if (ultimateFlashFrames == 0 && lvl >= 8) enemy4.y += enemy4.speed*speedBoost;


            //перерождает энеми, даём + счёт
            if (enemy1.y > 600) {
                enemy1.y = GetRandomValue(-400,-240);
                enemy1.x = GetRandomValue(0,800-enemy1.size_x);
                Score += 100;
            }
            if (enemy2.y > 600) {
                enemy2.y = GetRandomValue(-400,-240);
                enemy2.x = GetRandomValue(0,800-enemy2.size_x);
                Score += 100;
            }
            if (enemy3.y > 600) {
                enemy3.y = GetRandomValue(-400,-240);
                enemy3.x = GetRandomValue(0,800-enemy3.size_x);
                Score += 100;
            }
            if (enemy4.y > 600) {
                enemy4.y = GetRandomValue(-400,-240);
                enemy4.x = GetRandomValue(0,800-enemy4.size_x);
                Score += 100;
            }
            if (side1.x > GetRandomValue(1200,2000) || side1.x < GetRandomValue(-1200,-400)) {
                side1.y = nextHorizontalY(side1.y, side1.size_y);
                side1rotation = GetRandomValue(0,1);
                if (side1rotation == 0) {
                    side1.x = -50;
                    side1.speed = std::abs(side1.speed);
                }
                if (side1rotation == 1) {
                    side1.x = 850;
                    side1.speed = (-1)*(std::abs(side1.speed));
                }
                Score += 100;
            }
            if (side2.x > GetRandomValue(1200,2000) || side2.x < GetRandomValue(-1200,-400)) {
                side2.y = nextHorizontalY(side2.y, side2.size_y);
                side2rotation = GetRandomValue(0,1);
                if (side2rotation == 0) {
                    side2.x = -50;
                    side2.speed = std::abs(side2.speed);
                }
                if (side2rotation == 1) {
                    side2.x = 850;
                    side2.speed = (-1)*(std::abs(side2.speed));
                }
                Score += 100;
            }
            if (side3.x > GetRandomValue(1200,2000) || side3.x < GetRandomValue(-1200,-400)) {
                side3.y = nextHorizontalY(side3.y, side3.size_y);
                side3rotation = GetRandomValue(0,1);
                if (side3rotation == 0) {
                    side3.x = -50;
                    side3.speed = std::abs(side3.speed);
                }
                if (side3rotation == 1) {
                    side3.x = 850;
                    side3.speed = (-1)*(std::abs(side3.speed));
                }
                Score += 100;
            }
            if (side4.x > GetRandomValue(1200,2000) || side4.x < GetRandomValue(-1200,-400)) {
                side4.y = nextHorizontalY(side4.y, side4.size_y);
                side4rotation = GetRandomValue(0,1);
                if (side4rotation == 0) {
                    side4.x = -50;
                    side4.speed = std::abs(side4.speed);
                }
                if (side4rotation == 1) {
                    side4.x = 850;
                    side4.speed = (-1)*(std::abs(side4.speed));
                }
                Score += 100;
            }
            if (LASER.x > 810 && LASER.speed > 0 || LASER.x < -10 && LASER.speed < 0 ) {
                LASERrotarion = GetRandomValue(0,1);
                if (LASERrotarion == 0) {
                    LASER.x = GetRandomValue(-800,-1600);
                    LASER.speed = std::abs(LASER.speed);
                    LASERTransparency = 0;
                }
                if (LASERrotarion == 1) {
                    LASER.x = GetRandomValue(1600,2400);
                    LASER.speed = (-1)*(std::abs(LASER.speed));
                    LASERTransparency = 0;
                }
                Score += 100;
            }
            Best = std::max(Best, Score);


            //Границы окна
            if (player.x < 0) player.x = 0;
            if (player.x > 800-player.size_x) player.x = 800-player.size_x;
            if (player.y < 0) player.y = 0;
            if (player.y > 600-player.size_y) player.y = 600-player.size_y;


            //коллизии
            if (lvl >= 1) playerRect = {(float)player.x , (float)player.y , (float)player.size_x , (float)player.size_y};
            if (lvl >= 1) enemy1Rect = {(float)enemy1.x , (float)enemy1.y , (float)enemy1.size_x , (float)enemy1.size_y};
            if (lvl >= 2) enemy2Rect = {(float)enemy2.x , (float)enemy2.y , (float)enemy2.size_x , (float)enemy2.size_y};
            if (lvl >= 4) side1Rect = {(float)side1.x , (float)side1.y , (float)side1.size_x , (float)side1.size_y};
            if (lvl >= 4) side3Rect = {(float)side3.x , (float)side3.y , (float)side3.size_x , (float)side3.size_y};
            if (lvl >= 5) enemy3Rect = {(float)enemy3.x , (float)enemy3.y , (float)enemy3.size_x , (float)enemy3.size_y};
            if (lvl >= 6) LASERRect = {(float)LASER.x , (float)LASER.y , (float)LASER.size_x , (float)LASER.size_y};
            if (lvl >= 7) side2Rect = {(float)side2.x , (float)side2.y , (float)side2.size_x , (float)side2.size_y};
            if (lvl >= 7) side4Rect = {(float)side4.x , (float)side4.y , (float)side4.size_x , (float)side4.size_y};
            if (lvl >= 8) enemy4Rect = {(float)enemy4.x , (float)enemy4.y , (float)enemy4.size_x , (float)enemy4.size_y};
            bool playerHit = false;
            if (CheckCollisionRecs(playerRect , enemy1Rect) && lvl >= 1)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , enemy2Rect) && lvl >= 2)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , enemy3Rect) && lvl >= 5)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , enemy4Rect) && lvl >= 8)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , side1Rect) && lvl >= 4)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , side3Rect) && lvl >= 4)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , side2Rect) && lvl >= 7)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , side4Rect) && lvl >= 7)
            {
                if (DashEffect > 0) {
                    Score+=20;
                    DashChargesSec+=6;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }
            if (CheckCollisionRecs(playerRect , LASERRect) && lvl >= 6)
            {
                if (DashEffect > 0) {
                    Score+=50;
                    DashChargesSec+=30;
                };
                if (DashEffect == 0 && invFrames == 0 && lifeInvFrames == 0) playerHit = true;
            }

            if (playerHit) {
                requestAndroidHaptic(ANDROID_HAPTIC_LIFE_LOST);
                lives -= 1;

                if (lives <= 0) Gameover = 1;
                else {
                    //После потери жизни автоматически очищаем экран и даём время отойти
                    lifeInvFrames = lifeInvMax;
                    ultimateLockFrames = 30;
                    ultimateNeedsRelease = true;
                    ultimateFlashFrames = 3;
                    ultimateRingX = player.x + player.size_x/2;
                    ultimateRingY = player.y + player.size_y/2;
                }
            }
        }

        //Java считывает текущее состояние, поэтому эффект восстановится и после
        //повторного подключения USB-геймпада. Во время блокировки сопротивления нет.
        setAndroidUltimateReady(Gameover == 0 && ultimateCharge == ultimateMax &&
            ultimateLockFrames == 0 && ultimateFlashFrames == 0);


        //Обновляем осколки и волну от ульты
        for (int i = 0; i < maxParticles; i++) {
            if (particles[i].life > 0) {
                particles[i].x += particles[i].speedX;
                particles[i].y += particles[i].speedY;
                particles[i].speedY += 0.08f;
                particles[i].size *= 0.94f;
                particles[i].life -= 1;
            }
        }

        if (ultimateRingAlpha > 0) {
            ultimateRingRadius += 18;
            ultimateRingAlpha -= 14;
            if (ultimateRingAlpha < 0) ultimateRingAlpha = 0;
        }

        int shakeX = 0;
        int shakeY = 0;
        if (screenShakeFrames > 0) {
            shakeX = GetRandomValue(-5,5);
            shakeY = GetRandomValue(-5,5);
            screenShakeFrames -= 1;
        }

#if defined(PLATFORM_ANDROID)
        BeginTextureMode(gameTarget);
#else
        BeginDrawing();
#endif
        //клир
        if (Gameover == 0) ClearBackground(RAYWHITE);
        if (Gameover == 1) ClearBackground(BLACK);

        //Предупреждение о лазере
        Color lightred{230,41,55,(unsigned char)LASERTransparency};
        if (hideHUD == 0) {
            if (Gameover == 0) {
                if (LASER.x > 800 && LASER.speed < 0 && lvl >= 6) {
                    DrawRectangle(790,0,10,600,lightred);
                    if (LASER.x < 1300) LASERTransparency += 0.5*std::abs(LASER.speed);
                }
                if (LASER.x < 0 && LASER.speed > 0 && lvl >= 6) {
                    DrawRectangle(0,0,10,600,lightred);
                    if (LASER.x > -500) LASERTransparency += 0.5*std::abs(LASER.speed); 
                }
            }
        }
        

        //рисует мейн чара, врагов (мейн в конце,чтобы поверх был во время деша)
        Color enemyColor = ultimateFlashFrames > 0 ? WHITE : RED;
        DrawRectangle(enemy1.x + shakeX, enemy1.y + shakeY, enemy1.size_x, enemy1.size_y, enemyColor);
        if (lvl >= 2) DrawRectangle(enemy2.x + shakeX, enemy2.y + shakeY, enemy2.size_x, enemy2.size_y, enemyColor);
        if (lvl >= 5) DrawRectangle(enemy3.x + shakeX, enemy3.y + shakeY, enemy3.size_x, enemy3.size_y, enemyColor);
        if (lvl >= 8) DrawRectangle(enemy4.x + shakeX, enemy4.y + shakeY, enemy4.size_x, enemy4.size_y, enemyColor);
        if (lvl >= 4) DrawRectangle(side1.x + shakeX, side1.y + shakeY, side1.size_x, side1.size_y, enemyColor);
        if (lvl >= 4) DrawRectangle(side3.x + shakeX, side3.y + shakeY, side3.size_x, side3.size_y, enemyColor);
        if (lvl >= 7) DrawRectangle(side2.x + shakeX, side2.y + shakeY, side2.size_x, side2.size_y, enemyColor);
        if (lvl >= 7) DrawRectangle(side4.x + shakeX, side4.y + shakeY, side4.size_x, side4.size_y, enemyColor);
        if (lvl >= 6) DrawRectangle(LASER.x + shakeX, LASER.y + shakeY, LASER.size_x, LASER.size_y, RED);

        if (ultimateRingAlpha > 0) {
            Color ringColor{230,41,55,(unsigned char)ultimateRingAlpha};
            DrawCircleLines((int)ultimateRingX + shakeX, (int)ultimateRingY + shakeY,
                ultimateRingRadius, ringColor);
            DrawCircleLines((int)ultimateRingX + shakeX, (int)ultimateRingY + shakeY,
                ultimateRingRadius + 2, ringColor);
        }

        for (int i = 0; i < maxParticles; i++) {
            if (particles[i].life > 0) {
                unsigned char particleAlpha = (unsigned char)(255*particles[i].life/30);
                Color particleColor{230,41,55,particleAlpha};
                DrawRectangle((int)particles[i].x + shakeX, (int)particles[i].y + shakeY,
                    (int)particles[i].size, (int)particles[i].size, particleColor);
            }
        }

        //Во время неуязвимости игрок мигает
        if (lifeInvFrames == 0 || (lifeInvFrames/6)%2 == 0)
            DrawRectangle(player.x + shakeX, player.y + shakeY, player.size_x, player.size_y, GREEN);


        if (hideHUD == 0) {
            //рисуем лвл и счёт, почему именно %i? Как я понял %i говорит, что нужно заполнить %i последующим int Score
            //Лвл
            Color CustomBlack{0,0,0,120};
            const char *levelText = TextFormat("Level: %i", lvl);
            if (Gameover == 0) DrawText(levelText, 670, 20, 30, BLACK);
            if (Gameover == 1) DrawText(levelText, 670, 20, 30, VIOLET);

            int heartsWidth = 108;
            int heartsX = 670 + (MeasureText(levelText, 30) - heartsWidth)/2;
            if (lives >= 1) drawPixelHeart(heartsX + 3, 58, 3, RED, 2, true);
            if (lives >= 2) drawPixelHeart(heartsX + 41, 58, 3, RED, 2, true);
            if (lives >= 3) drawPixelHeart(heartsX + 79, 58, 3, RED, 2, true);
            //Деши
            if (Gameover == 0) DrawRectangle(535, 580, (int)(1.4*180), 10, LIGHTGRAY);
            if (Gameover == 0) DrawText(TextFormat("Dash Charges: %i", DashCharges), 535, 545, 30, BLACK);
            if (Gameover == 0) DrawRectangle(535, 580, (int)(1.4*DashChargesSec), 10, BLUE);

            //Ульта
            if (Gameover == 0) DrawRectangle(20, 580, (int)(0.28*ultimateMax), 10, LIGHTGRAY);
            if (Gameover == 0 && ultimateCharge < ultimateMax) DrawText("Ultimate", 20, 545, 30, BLACK);
            if (Gameover == 0 && ultimateCharge == ultimateMax) DrawText("L2 + R2", 20, 545, 30, RED);
            if (Gameover == 0) DrawRectangle(20, 580, (int)(0.28*ultimateCharge), 10, RED);
            
            //Счёт
            bool showBestScore = IsKeyDown(KEY_TAB) ||
                gamepadButtonDown(GAMEPAD_BUTTON_RIGHT_FACE_DOWN, DIRECT_USB_CROSS);

            if (showBestScore) {
                if (Gameover == 0) DrawText(TextFormat("The best score: %i", Best), 20, 20, 30, BLACK);
                if (Gameover == 1) DrawText(TextFormat("The best score: %i", Best), 20, 20, 30, VIOLET);
            }
            if (!showBestScore) {
                if (Gameover == 0) DrawText(TextFormat("Score: %i", Score), 20, 20, 30, BLACK);
                if (Gameover == 1) DrawText(TextFormat("Score: %i", Score), 20, 20, 30, VIOLET);
            }
        }
        
        //Сброс спидбуста в конце фрейма
        speedBoost = 1;

        //Рестарт и надпись при лузе
        if (Gameover == 1) {
            DrawText("Wasted",105,50,164,VIOLET);
            DrawText("Press R",145,350,120,VIOLET);
            DrawText("to restart",80,460,120,VIOLET);
            if (IsKeyPressed(KEY_R) ||
                gamepadButtonPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP, DIRECT_USB_TRIANGLE)) {
                player.x = 375;
                player.y = 275;
                enemy1.y = -50;
                enemy1.x = GetRandomValue(0,800-enemy1.size_x);
                enemy2.y = GetRandomValue(-400,-140);
                enemy2.x = GetRandomValue(0,800-enemy2.size_x);
                enemy3.y = GetRandomValue(-700,-340);
                enemy3.x = GetRandomValue(0,800-enemy3.size_x);
                enemy4.y = GetRandomValue(-800,-540);
                enemy4.x = GetRandomValue(0,800-enemy4.size_x);
                side1.y = nextHorizontalY(side1.y, side1.size_y);
                side1.x = -300;
                side3.y = nextHorizontalY(side3.y, side3.size_y);
                side3.x = -300;
                side2.y = nextHorizontalY(side2.y, side2.size_y);
                side2.x = -300;
                side4.y = nextHorizontalY(side4.y, side4.size_y);
                side4.x = -300;
                LASER.x = -400;
                LASER.speed = 7;
                lvl = 1;
                Gameover = 0;
                Score = 0;
                speedBoost = 1;
                DashCharges = 1;
                DashChargesSec = 0;
                DashEffect = 0;
                invFrames = 0;
                lives = 3;
                lifeInvFrames = 0;
                ultimateCharge = 0;
                ultimateLockFrames = 0;
                ultimateNeedsRelease = false;
                ultimateFlashFrames = 0;
                ultimateRingAlpha = 0;
                screenShakeFrames = 0;
                particleIndex = 0;
                for (int i = 0; i < maxParticles; i++) particles[i].life = 0;
                side1Delay = 0;
                side2Delay = 0;
                side3Delay = 0;
                side4Delay = 0;
                hideHUD = 0;
                settingsOpen = false;
                LASERrotarion = 0;
                level1Unlocker = 0;
                level2Unlocker = 0;
                level3Unlocker = 0;
                level4Unlocker = 0;
                level5Unlocker = 0;
                level6Unlocker = 0;
                level7Unlocker = 0;
                level8Unlocker = 0;
                level9Unlocker = 0;
                level10Unlocker = 0;
                LASERrotarion = GetRandomValue(0,1);               
                side1rotation = GetRandomValue(0,1);               
                side2rotation = GetRandomValue(0,1);                
                side3rotation = GetRandomValue(0,1);                
                side4rotation = GetRandomValue(0,1);
                side1.speed = std::abs(side1.speed);
                side2.speed = std::abs(side2.speed);
                side3.speed = std::abs(side3.speed);
                side4.speed = std::abs(side4.speed);
                if (side1rotation == 1) {
                    side1.x = 1100;
                    side1.speed = -std::abs(side1.speed);
                }
                if (side2rotation == 1) {
                    side2.x = 1100;
                    side2.speed = -std::abs(side2.speed);
                }
                if (side3rotation == 1) {
                    side3.x = 1100;
                    side3.speed = -std::abs(side3.speed);
                }
                if (side4rotation == 1) {
                    side4.x = 1100;
                    side4.speed = -std::abs(side4.speed);
                }
                if (LASERrotarion == 1) {
                    LASER.x = 1200;
                    LASER.speed = -std::abs(LASER.speed);
                }
                LASERTransparency = 0;
                Rectangle playerRect = {0,0,0,0};
                Rectangle enemy1Rect = {0,0,0,0};
                Rectangle enemy2Rect = {0,0,0,0};
                Rectangle side1Rect = {0,0,0,0};
                Rectangle side3Rect = {0,0,0,0};
                Rectangle enemy3Rect = {0,0,0,0};
                Rectangle LASERRect = {0,0,0,0};
                Rectangle side2Rect = {0,0,0,0};
                Rectangle side4Rect = {0,0,0,0};
                Rectangle enemy4Rect = {0,0,0,0};
            } 
        }

        //Оверлей рисуется последним, поэтому остаётся поверх продолжающейся игры.
        if (settingsOpen) {
            Color panelColor{12,12,16,225};
            Color rowColor{38,38,46,240};
            Color selectedColor{65,65,78,245};

            DrawRectangle(145, 145, 510, 310, panelColor);
            DrawRectangle(165, 165, 470, 270, BLACK);
            DrawRectangle(168, 168, 464, 264, panelColor);
            DrawText("SETTINGS", 278, 188, 36, WHITE);

            DrawRectangle(195, 258, 410, 72,
                settingsSelected == 0 ? selectedColor : rowColor);
            DrawText(">", 213, 277, 30, RED);
            DrawText("Acceleration", 250, 277, 30, WHITE);
            DrawText(accelerationEnabled ? "ON" : "OFF", 535, 277, 30,
                accelerationEnabled ? GREEN : LIGHTGRAY);

            DrawText("D-PAD: SELECT", 205, 360, 20, LIGHTGRAY);
            DrawText("SQUARE: TOGGLE", 430, 360, 20, LIGHTGRAY);
            DrawText("OPTIONS: CLOSE", 315, 398, 20, LIGHTGRAY);
        }
#if defined(PLATFORM_ANDROID)
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        const float screenWidth = (float)GetScreenWidth();
        const float screenHeight = (float)GetScreenHeight();
        const float scaleX = screenWidth/800.0f;
        const float scaleY = screenHeight/600.0f;
        const float scale = std::min(scaleX, scaleY);
        const float outputWidth = 800.0f*scale;
        const float outputHeight = 600.0f*scale;

        Rectangle source{0.0f, 0.0f, 800.0f, -600.0f};
        Rectangle destination{
            (screenWidth - outputWidth)/2.0f,
            (screenHeight - outputHeight)/2.0f,
            outputWidth,
            outputHeight
        };
        DrawTexturePro(gameTarget.texture, source, destination, {0.0f, 0.0f}, 0.0f, WHITE);
        EndDrawing();
#else
        EndDrawing();
#endif
    }
#if defined(PLATFORM_ANDROID)
    UnloadRenderTexture(gameTarget);
#endif
    CloseWindow();
}

