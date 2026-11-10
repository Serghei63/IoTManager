#define LODEPNG_NO_COMPILE_PNG
#define LODEPNG_NO_COMPILE_DECODER
#define LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_NO_COMPILE_DISK
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS

#include "Panel_img.h"
#include "classes/IoTItem.h"
#include "SPI.h"
#include <SdFat.h>

// Отключаем конфликт с FS.h
#define FS_NO_GLOBALS

// Используем SdFat
SdFat SD;

std::vector<lv_obj_t*> ui_Image(0, nullptr);
int img = 0;

// ======================================================
// SD PINS
// ======================================================
#define SD_SCK   48
#define SD_MISO  41
#define SD_MOSI  47
#define SD_CS    42

SPIClass spiSD(FSPI);

// === СТАТИЧЕСКИЕ ФЛАГИ ДЛЯ ОДНОКРАТНОЙ ИНИЦИАЛИЗАЦИИ ===
static bool spi_initialized = false;
static bool sd_initialized = false;

// ======================================================
// PRINT SD FILES (для отладки) — ИСПРАВЛЕНО!
// ======================================================
void printDirectory(FsFile& dir, int numTabs) {
    FsFile entry;
    while (entry.openNext(&dir, O_RDONLY)) {
        for (uint8_t i = 0; i < numTabs; i++) Serial.print("  ");
       // Serial.print(entry.name());

        if (entry.isDir()) {
            Serial.println("/");
            printDirectory(entry, numTabs + 1); // Рекурсия с ссылкой
        } else {
            Serial.printf("  SIZE: %llu\n", entry.fileSize()); // %llu для uint64_t
        }
        entry.close();
    }
}

void listSDFiles() {
    Serial.println("\n====================================");
    Serial.println("[SD] FILE LIST");
    Serial.println("====================================");

    FsFile root;
    if (!root.open("/", O_RDONLY)) {
        Serial.println("[SD] ERROR: ROOT OPEN FAILED");
        return;
    }

    printDirectory(root, 0);
    root.close();
    Serial.println("====================================\n");
}

// ======================================================
// LVGL FS DRIVER — ИСПРАВЛЕНО!
// ======================================================
static void* sd_open_cb(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
    LV_UNUSED(drv);
    String fixedPath = path;
    if (!fixedPath.startsWith("/")) fixedPath = "/" + fixedPath;

    uint8_t flags = O_RDONLY;
    if (mode == LV_FS_MODE_WR) flags = O_WRONLY | O_CREAT | O_TRUNC;

    FsFile* f = new FsFile();
    if (!f->open(fixedPath.c_str(), flags)) {
        Serial.printf("[FS OPEN] FAILED: %s\n", fixedPath.c_str());
        delete f;
        return nullptr;
    }
    //Serial.printf("[FS OPEN] OK: %s\n", fixedPath.c_str());
    return f;
}

static lv_fs_res_t sd_close_cb(lv_fs_drv_t* drv, void* file_p) {
    LV_UNUSED(drv);
    FsFile* f = static_cast<FsFile*>(file_p);
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_read_cb(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
    LV_UNUSED(drv);
    FsFile* f = static_cast<FsFile*>(file_p);
    *br = f->read(buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t sd_seek_cb(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
    LV_UNUSED(drv);
    FsFile* f = static_cast<FsFile*>(file_p);

    uint64_t new_pos;
    switch (whence) {
        case LV_FS_SEEK_SET:
            new_pos = pos;
            break;
        case LV_FS_SEEK_CUR:
            new_pos = f->curPosition() + pos;
            break;
        case LV_FS_SEEK_END:
            new_pos = f->fileSize() + pos;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    if (f->seek(new_pos)) {
        return LV_FS_RES_OK;
    }
    return LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t sd_tell_cb(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
    LV_UNUSED(drv);
    FsFile* f = static_cast<FsFile*>(file_p);
    *pos_p = f->curPosition();
    return LV_FS_RES_OK;
}

void lv_fs_sdcard_init() {
    static bool initialized = false;
    if (initialized) return;

    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter = 'S';
    drv.open_cb  = sd_open_cb;
    drv.close_cb = sd_close_cb;
    drv.read_cb  = sd_read_cb;
    drv.seek_cb  = sd_seek_cb;
    drv.tell_cb  = sd_tell_cb;
    lv_fs_drv_register(&drv);
    initialized = true;
    Serial.println("[LVGL] SdFat FS registered");
}

// ======================================================
// PANEL IMG
// ======================================================
Panel_img::Panel_img(String parameters) : IoTItem(parameters) {
    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    // Создание объекта изображения
    lv_obj_t* parent = ui_Screen1;
    if (name == "TabPage" && tab2 >= nomer) {
        parent = ui_TabPage[nomer - 1];
    } else if (name == "Button" && isf1 >= nomer) {
        parent = ui_Button[nomer - 1];
    } else if (name == "Container" && cont >= nomer) {
        parent = ui_Container[nomer - 1];
    }
    ui_Image.push_back(lv_image_create(parent));

    // ВАЖНО: Сохраняем индекс текущего изображения
    currentImageIndex = img;

    Serial.printf("[PANEL IMG] ================================\n");
    Serial.printf("[PANEL IMG] %s %d, imageIndex: %d\n", name.c_str(), nomer, currentImageIndex);
    Serial.printf("[PANEL IMG] ================================\n");

    // Установка позиции
    lv_obj_set_x(ui_Image[img], _x);
    lv_obj_set_y(ui_Image[img], _y);
    lv_obj_clear_flag(ui_Image[img], LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_Image[img]);

    // === ИНИЦИАЛИЗАЦИЯ SPI (ТОЛЬКО ПРИ ПЕРВОМ ВЫЗОВЕ) ===
    if (!spi_initialized) {
        spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
        spi_initialized = true;
        Serial.println("[SPI] Initialized");
    }

    // === ИНИЦИАЛИЗАЦИЯ SD (ТОЛЬКО ПРИ ПЕРВОМ ВЫЗОВЕ) ===
    if (!sd_initialized) {
        SdSpiConfig spiConfig(SD_CS, DEDICATED_SPI, 40000000, &spiSD);
        if (!SD.begin(spiConfig)) {
            Serial.println("[SdFat] INIT FAILED");
            // Не устанавливаем флаг sd_initialized
        } else {
            Serial.println("[SdFat] Card initialized");

            uint64_t cardSizeMB = SD.card()->sectorCount() * 512ULL / (1024 * 1024);
            Serial.printf("[SdFat] Card Size: %llu MB\n", cardSizeMB);

            listSDFiles();
            lv_fs_sdcard_init();

            sd_initialized = true;
        }
    }

    // Загрузка изображения
    String file = "";
    jsonRead(parameters, "name_img", file);

    if (file != "") {
        if (!file.startsWith("/")) file = "/" + file;

        Serial.println("\n====================================");
        Serial.println("[IMAGE LOAD]");
        Serial.println("====================================");
        Serial.printf("[SD] Requested: %s\n", file.c_str());

        if (file.endsWith(".bmp") || file.endsWith(".BMP")) {
            if (SD.exists(file)) {
                String lvgl_path = "S:" + file;
                Serial.printf("[LVGL] Loading: %s\n", lvgl_path.c_str());
                lv_image_set_src(ui_Image[img], lvgl_path.c_str());
                lv_obj_invalidate(ui_Image[img]);
                Serial.println("[LVGL] BMP image loaded");
            } else {
                Serial.println("[SdFat] File not found");
            }
        } else {
            Serial.println("[IMAGE] Only .bmp files allowed!");
        }
        Serial.println("====================================\n");
    }

    img++;
}

Panel_img::~Panel_img() {}

void Panel_img::setValue(const IoTValue& Value, bool genEvent) {
    value = Value;
    regEvent((String)(int)value.valD, F("Panel_img"), false, genEvent);
}

String Panel_img::getValue() {
    return (String)(int)value.valD;
}

// Execute method for handling commands
IoTValue Panel_img::execute(String command, std::vector<IoTValue> &param){
    Serial.printf("[PANEL IMG EXECUTE] Command: %s, ImageIndex: %d\n", command.c_str(), currentImageIndex);
    
    // Проверка валидности индекса
    if (currentImageIndex < 0 || currentImageIndex >= (int)ui_Image.size()) {
        Serial.printf("[PANEL IMG ERROR] Invalid image index: %d, vector size: %d\n", 
                      currentImageIndex, ui_Image.size());
        return {};
    }
    
    if (ui_Image[currentImageIndex] == nullptr) {
        Serial.println("[PANEL IMG ERROR] ui_Image[currentImageIndex] is NULL");
        return {};
    }
    
    if (command == "hide"){
        Serial.println("[PANEL IMG] Hiding image");
        lv_obj_add_flag(ui_Image[currentImageIndex], LV_OBJ_FLAG_HIDDEN);
    }
    else if (command == "show"){
        Serial.println("[PANEL IMG] Showing image");
        lv_obj_clear_flag(ui_Image[currentImageIndex], LV_OBJ_FLAG_HIDDEN);
    }
    else if (command == "pos"){
        if (param.size() >= 2) {
            _x = param[0].valD;
            _y = param[1].valD;
            Serial.printf("[PANEL IMG] Moving to pos: %d, %d\n", _x, _y);
            lv_obj_set_pos(ui_Image[currentImageIndex], _x, _y);
        }
    }
    else if (command == "x"){
        if (param.size() >= 1) {
            _x = param[0].valD;
            Serial.printf("[PANEL IMG] Moving to x: %d\n", _x);
            lv_obj_set_x(ui_Image[currentImageIndex], _x);
        }
    }
    else if (command == "y"){
        if (param.size() >= 1) {
            _y = param[0].valD;
            Serial.printf("[PANEL IMG] Moving to y: %d\n", _y);
            lv_obj_set_y(ui_Image[currentImageIndex], _y);
        }
    }
    else {
        Serial.printf("[PANEL IMG] Unknown command: %s\n", command.c_str());
    }

    return {};
}