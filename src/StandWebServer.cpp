#include "StandWebServer.h"
#ifdef STANDARD_WEB_SERVER

#ifdef SD_ENABLE
#include <SdFat.h>
extern SdFat sd;
#endif

File uploadFile;
String unsupportedFiles = String();

static const char TEXT_PLAIN[] PROGMEM = "text/plain";
static const char FS_INIT_ERROR[] PROGMEM = "FS INIT ERROR";
static const char FILE_NOT_FOUND[] PROGMEM = "FileNotFound";

enum UpdateType {
    FIRMWARE,
    FILESYSTEM
};

void standWebServerInit() {
    HTTP.serveStatic("/build", FileFS, "/build", "max-age=31536000");
    HTTP.serveStatic("/favicon.ico", FileFS, "/favicon.ico", "max-age=31536000");

    HTTP.on("/set", HTTP_GET, []() {
        if (HTTP.hasArg(F("routerssid")) && WiFi.getMode() == WIFI_AP) {
            jsonWriteStr(settingsFlashJson, F("routerssid"), HTTP.arg(F("routerssid")));
            syncSettingsFlashJson();
            HTTP.send(200, "text/plain", "ok");
        }

        if (HTTP.hasArg(F("routerpass")) && WiFi.getMode() == WIFI_AP) {
            jsonWriteStr(settingsFlashJson, F("routerpass"), HTTP.arg(F("routerpass")));
            syncSettingsFlashJson();
            HTTP.send(200, "text/plain", "ok");
        }
    });

    HTTP.begin();

    HTTP.on("/status", HTTP_GET, handleStatus);
    HTTP.on("/list", HTTP_GET, handleFileList);
    HTTP.on("/edit", HTTP_GET, handleGetEdit);
    HTTP.on("/edit", HTTP_PUT, handleFileCreate);
    HTTP.on("/edit", HTTP_DELETE, handleFileDelete);
    HTTP.on("/edit", HTTP_POST, replyOK, handleFileUpload);
    HTTP.on("/localota", HTTP_GET, handleLocalOTA);
    HTTP.on("/localota_handler", HTTP_GET, handleLocalOTA_Handler);

    HTTP.on("/update", HTTP_POST, []() {
        HTTP.send(200);
      }, handleUpdateOTA);
    
    HTTP.on("/update", HTTP_OPTIONS, handleCors);
    HTTP.onNotFound(handleNotFound);
}

void replyOK() {
    HTTP.send(200, FPSTR(TEXT_PLAIN), "");
}

void replyOKWithMsg(String msg) {
    HTTP.send(200, FPSTR(TEXT_PLAIN), msg);
}

void replyNotFound(String msg) {
    HTTP.send(404, FPSTR(TEXT_PLAIN), msg);
}

void replyBadRequest(String msg) {
    HTTP.send(400, FPSTR(TEXT_PLAIN), msg + "\r\n");
}

void replyServerError(String msg) {
    HTTP.send(500, FPSTR(TEXT_PLAIN), msg + "\r\n");
}

void handleStatus() {
    String json;
    json.reserve(128);

    json = "{\"type\":\"";
    json += FS_NAME;
    json += "\", \"isOk\":";

#ifdef ESP8266
    FSInfo fs_info;
    FileFS.info(fs_info);
    json += F("\"true\", \"totalBytes\":\"");
    json += fs_info.totalBytes;
    json += F("\", \"usedBytes\":\"");
    json += fs_info.usedBytes;
    json += "\"";
#endif
#ifdef ESP32
    json += F("\"true\", \"totalBytes\":\"");
    json += String(FileFS.totalBytes());
    json += F("\", \"usedBytes\":\"");
    json += String(FileFS.usedBytes());
    json += "\"";
#endif

    json += F(",\"unsupportedFiles\":\"");
    json += unsupportedFiles;
    json += "\"}";

    HTTP.send(200, "application/json", json);
}

void handleLocalOTA() {
  String page = "<form action='/localota' method='POST'><label for='server'>Server Address:</label><input type='text' name='server' value='http://192.168.1.2:5500'><input type='submit' value='Update'></form>";
  HTTP.send(200, "text/html", page);
}

void handleCors() {
    HTTP.sendHeader("Access-Control-Allow-Origin", "*");
    HTTP.send(200);
}
  
void handleUpdateOTA() {
    UpdateType typeOTAfile = FIRMWARE;
    HTTPUpload& upload = HTTP.upload();
    if (upload.filename != "firmware.bin" && upload.filename != "littlefs.bin") {
        SerialPrint("E", F("OTA"), "Неверное имя файла: " + upload.filename);
        return;
    }
    if (upload.filename == "firmware.bin") {
        typeOTAfile = FIRMWARE;
    } else if (upload.filename == "littlefs.bin") {
        typeOTAfile = FILESYSTEM;
    }
    #ifdef ESP8266
    size_t size = upload.totalSize;
    int updatePartition = (typeOTAfile == FIRMWARE)? U_FLASH : U_FS;
    #else //ESP32
    size_t size = UPDATE_SIZE_UNKNOWN;
    int updatePartition = (typeOTAfile == FIRMWARE)? U_FLASH : U_SPIFFS;
    #endif
    if (upload.status == UPLOAD_FILE_START) {
      SerialPrint("i", F("OTA"), "Начало загрузки файла: " + upload.filename);
      if (!Update.begin(size, updatePartition)) {
        Update.end();
        SerialPrint("E", F("OTA"), "Ошибка: Недостаточно памяти");
        HTTP.send(500, "text/plain", "Ошибка: Недостаточно памяти");
        return;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.end();
        SerialPrint("E", F("OTA"), "Ошибка записи данных");
        HTTP.send(500, "text/plain", "Ошибка записи данных");
        return;
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        SerialPrint("i", F("OTA"), "Обновление завершено");
        HTTP.send(200, "text/plain", "Обновление успешно");
        ESP.restart();
      } else {
        Update.end();
        SerialPrint("E", F("OTA"), "Ошибка завершения обновления");
        HTTP.send(500, "text/plain", "Ошибка завершения обновления");
      }
    }
}

void handleLocalOTA_Handler() {
    String serverValue = HTTP.arg("server");
    upgrade_firmware(3, serverValue);
}

#ifdef ESP32
String getContentType(String filename) {
    if (HTTP.hasArg("download")) {
        return "application/octet-stream";
    } else if (filename.endsWith(".htm") || filename.endsWith(".html")) {
        return "text/html";
    } else if (filename.endsWith(".css")) {
        return "text/css";
    } else if (filename.endsWith(".js")) {
        return "application/javascript";
    } else if (filename.endsWith(".png")) {
        return "image/png";
    } else if (filename.endsWith(".gif")) {
        return "image/gif";
    } else if (filename.endsWith(".jpg")) {
        return "image/jpeg";
    } else if (filename.endsWith(".ico")) {
        return "image/x-icon";
    } else if (filename.endsWith(".xml")) {
        return "text/xml";
    } else if (filename.endsWith(".pdf")) {
        return "application/x-pdf";
    } else if (filename.endsWith(".zip")) {
        return "application/x-zip";
    } else if (filename.endsWith(".gz")) {
        return "application/x-gzip";
    }
    return "text/plain";
}
#endif

bool handleFileRead(String path) {
    if (path.endsWith("/")) {
        path += "index.html";
    }

#ifdef SD_ENABLE
    String sdPath = path;
    if (sdPath.startsWith("/")) {
        sdPath.remove(0, 1);
    }

    if (sdPath.startsWith("lgsd/")) {
        if (sd.exists(sdPath.c_str())) {
            FsFile file = sd.open(sdPath.c_str(), O_RDONLY);
            if (file) {
                HTTP.sendHeader("Content-Type", "text/plain");
                HTTP.sendHeader("Content-Length", String(file.fileSize()));
                HTTP.send(200);
                
                uint8_t buffer[512];
                while (file.available()) {
                    int bytesRead = file.read(buffer, sizeof(buffer));
                    HTTP.client().write(buffer, bytesRead);
                }
                file.close();
                return true;
            }
        }
        return false;
    }
#endif

    String contentType;
    if (HTTP.hasArg("download")) {
        contentType = F("application/octet-stream");
    } else {
#ifdef ESP32
        contentType = getContentType(path);
#endif
#ifdef ESP8266
        contentType = mime::getContentType(path);
#endif
    }

    if (!FileFS.exists(path)) {
        path = path + ".gz";
    }
    if (FileFS.exists(path)) {
        File file = FileFS.open(path, "r");
        if (HTTP.streamFile(file, contentType) != file.size()) {
            // Sent less data than expected
        }
        file.close();
        return true;
    }

    return false;
}

String lastExistingParent(String path) {
#ifndef LIBRETINY      
    while (!path.isEmpty() && !FileFS.exists(path)) 
#else
    while (path.length() != 0 && !FileFS.exists(path)) 
#endif      
    {
        if (path.lastIndexOf('/') > 0) {
            path = path.substring(0, path.lastIndexOf('/'));
        } else {
            path = String();
        }
    }
    return path;
}

void handleFileUpload() {
    if (HTTP.uri() != "/edit") return;
    HTTPUpload &upload = HTTP.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        uploadFile = FileFS.open(filename, "w");
        if (!uploadFile) {
            return replyServerError(F("CREATE FAILED"));
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
            if (bytesWritten != upload.currentSize) {
                return replyServerError(F("WRITE FAILED"));
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
        }
    }
}

#if defined ESP8266 
void deleteRecursive(String path) {
    File file = FileFS.open(path, "r");
    bool isDir = file.isDirectory();
    file.close();

    if (!isDir) {
        FileFS.remove(path);
        return;
    }
    Dir dir = FileFS.openDir(path);
    while (dir.next()) {
        deleteRecursive(path + '/' + dir.fileName());
    }
    FileFS.rmdir(path);
}
#endif

#if defined ESP32 || defined LIBRETINY
struct treename {
    uint8_t type;
    char *name;
};

void deleteRecursive(String path) {
    fs::File dir = FileFS.open(path.c_str());

    if (!dir.isDirectory()) {
        dir.close();
        FileFS.remove(path);
        return;
    }

    fs::File entry;
    while (entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
#if defined ESP32
            deleteRecursive(entry.path());
#elif defined LIBRETINY
            deleteRecursive(entry.fullName());
#endif
        } else {
            String tmpname = path + "/" + strdup(entry.name());
            entry.close();
            FileFS.remove(tmpname);
        }
    }
    dir.close();
    FileFS.rmdir(path);
}
#endif

void handleFileDelete() {
    String path = HTTP.arg(0);
#ifndef LIBRETINY
    if (path.isEmpty() || path == "/") return replyBadRequest("BAD PATH");
#else
    if (path.length() == 0 || path == "/") return replyBadRequest("BAD PATH");
#endif
    if (!FileFS.exists(path)) {
        return replyNotFound(FPSTR(FILE_NOT_FOUND));
    }
    deleteRecursive(path);
    replyOKWithMsg(lastExistingParent(path));
}

void handleFileCreate() {
    String path = HTTP.arg("path");
#ifndef LIBRETINY
    if (path.isEmpty()) return replyBadRequest(F("PATH ARG MISSING"));
#else
    if (path.length() == 0) return replyBadRequest(F("PATH ARG MISSING"));
#endif

#ifdef USE_SPIFFS
    if (checkForUnsupportedPath(path).length() > 0) {
        return replyServerError(F("INVALID FILENAME"));
    }
#endif

    if (path == "/") return replyBadRequest("BAD PATH");
    if (FileFS.exists(path)) return replyBadRequest(F("PATH FILE EXISTS"));

    String src = HTTP.arg("src");
#ifndef LIBRETINY
    if (src.isEmpty()) {
#else
    if (src.length() == 0) {
#endif    
        if (path.endsWith("/")) {
            path.remove(path.length() - 1);
            if (!FileFS.mkdir(path)) {
                return replyServerError(F("MKDIR FAILED"));
            }
        } else {
            File file = FileFS.open(path, "w");
            if (file) {
#ifdef ESP8266
                file.write((const char *)0);
#endif
#ifdef ESP32
                file.write(0);
#endif
#ifdef LIBRETINY
                file.write((uint8_t)0);
#endif
                file.close();
            } else {
                return replyServerError(F("CREATE FAILED"));
            }
        }
        if (path.lastIndexOf('/') > -1) {
            path = path.substring(0, path.lastIndexOf('/'));
        }
        replyOKWithMsg(path);
    } else {
        if (src == "/") return replyBadRequest("BAD SRC");
        if (!FileFS.exists(src)) return replyBadRequest(F("SRC FILE NOT FOUND"));

        if (path.endsWith("/")) path.remove(path.length() - 1);
        if (src.endsWith("/")) src.remove(src.length() - 1);
        if (!FileFS.rename(src, path)) {
            return replyServerError(F("RENAME FAILED"));
        }
        replyOKWithMsg(lastExistingParent(src));
    }
}

#ifdef ESP8266
void handleFileList() {
    if (!HTTP.hasArg("dir")) return replyBadRequest(F("DIR ARG MISSING"));

    String path = HTTP.arg("dir");
    if (path != "/" && !FileFS.exists(path)) return replyBadRequest("BAD PATH");

    Dir dir = FileFS.openDir(path);
    path.clear();

    if (!HTTP.chunkedResponseModeStart(200, "text/json")) {
        HTTP.send(505, F("text/html"), F("HTTP1.1 required"));
        return;
    }

    String output;
    output.reserve(64);
    while (dir.next()) {
#ifdef USE_SPIFFS
        String error = checkForUnsupportedPath(dir.fileName());
        if (error.length() > 0) continue;
#endif
        if (output.length()) {
            HTTP.sendContent(output);
            output = ',';
        } else {
            output = '[';
        }

        output += "{\"type\":\"";
        if (dir.isDirectory()) {
            output += "dir";
        } else {
            output += F("file\",\"size\":\"");
            output += dir.fileSize();
        }

        output += F("\",\"name\":\"");
        if (dir.fileName()[0] == '/') {
            output += &(dir.fileName()[1]);
        } else {
            output += dir.fileName();
        }
        output += "\"}";
    }

    output += "]";
    HTTP.sendContent(output);
    HTTP.chunkedResponseFinalize();
}
#endif

#if defined ESP32 
void handleFileList() {
    if (!HTTP.hasArg("dir")) {
        HTTP.send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = HTTP.arg("dir");
    
    if (path == "/") {
        File root = FileFS.open(path);
        String output = "[";
        if (root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (output != "[") output += ',';
                output += "{\"type\":\"" + String(file.isDirectory() ? "dir" : "file") + 
                          "\",\"size\":\"" + String(file.size()) + 
                          "\",\"name\":\"" + String(file.name()) + "\"}";
                file = root.openNextFile();
            }
        }
#ifdef SD_ENABLE
        if (sd.exists("/")) {
            if (output != "[") output += ',';
            output += "{\"type\":\"dir\",\"size\":\"0\",\"name\":\"lgsd\"}";
        }
#endif
        output += "]";
        HTTP.send(200, "text/json", output);
        return;
    }

#ifdef SD_ENABLE
    if (path.startsWith("/lgsd")) {
        FsFile dir = sd.open(path.c_str());
        String output = "[";
        if (dir && dir.isDirectory()) {
            FsFile entry = dir.openNextFile();
            while (entry) {
                char nameBuf[64];
                entry.getName(nameBuf, sizeof(nameBuf));
                String itemName = String(nameBuf);
                if (itemName != "" && itemName != "." && itemName != "..") {
                    if (output != "[") output += ',';
                    
                    String relativePath = path;
                    if (!relativePath.endsWith("/")) relativePath += "/";
                    relativePath += itemName;
                    
                    String displayName = relativePath;
                    if (displayName.startsWith("/")) displayName.remove(0, 1);

                    output += "{\"type\":\"" + String(entry.isDirectory() ? "dir" : "file") + 
                              "\",\"size\":\"" + String(entry.size()) + 
                              "\",\"name\":\"" + displayName + "\"}";
                }
                entry = dir.openNextFile();
            }
            dir.close();
        }
        output += "]";
        HTTP.send(200, "text/json", output);
        return;
    }
#endif

    if (path != "/" && !FileFS.exists(path)) {
        return replyBadRequest("BAD PATH");
    }
    File root = FileFS.open(path);
    String output = "[";
    if (root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (output != "[") output += ',';
            output += "{\"type\":\"" + String(file.isDirectory() ? "dir" : "file") + 
                      "\",\"size\":\"" + String(file.size()) + 
                      "\",\"name\":\"" + String(file.name()) + "\"}";
            file = root.openNextFile();
        }
    }
    output += "]";
    HTTP.send(200, "text/json", output);
}
#endif

#if defined LIBRETINY
void handleFileList() {
    if (!HTTP.hasArg("dir")) {
        HTTP.send(500, "text/plain", "BAD ARGS");
        return;
    }
    String path = HTTP.arg("dir");
    if (path != "/" && !FileFS.exists(path)) {
        return replyBadRequest("BAD PATH");
    }
    FileFS.open(path.c_str());
    lfs_dir_t dir;
    struct lfs_info info;
    int err = lfs_dir_open(FileFS.getFS(), &dir, path.c_str());
    if (err) {
        HTTP.send(500, "text/plain", "FAIL OPEN DIR");
        return;
    }
    String output = "[";
    while (true) {
        int res = lfs_dir_read(FileFS.getFS(), &dir, &info);
        if (res < 0) {
            lfs_dir_close(FileFS.getFS(), &dir);
            return;
        }
        
        if (!res) break;
        
        if (output != "[") output += ',';

        output += "{\"type\":\"";
        if (info.type == LFS_TYPE_DIR) {
            output += "dir";
        } else {
            output += F("file\",\"size\":\"");
            output += info.size;
        }

        output += "\",\"name\":\"";
        output += String(info.name);
        output += "\"}";
    }

    err = lfs_dir_close(FileFS.getFS(), &dir);
    if (err) {
        HTTP.send(500, "text/plain", "FAIL CLOSE DIR");
        return;
    }

    output += "]";
    HTTP.send(200, "text/json", output);
}
#endif

void handleNotFound() {
#ifdef ESP8266
    String uri = ESP8266WebServer::urlDecode(HTTP.uri());
#endif
#ifdef ESP32
    String uri = WebServer::urlDecode(HTTP.uri());
#endif
#ifdef LIBRETINY
    String uri = WebServer::urlDecode(HTTP.uri());
#endif
    if (handleFileRead(uri)) {
        return;
    }

    String message;
    message.reserve(100);
    message = F("Error: File not found\n\nURI: ");
    message += uri;
    message += F("\nMethod: ");
    message += (HTTP.method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += HTTP.args();
    message += '\n';
    for (uint8_t i = 0; i < HTTP.args(); i++) {
        message += F(" NAME:");
        message += HTTP.argName(i);
        message += F("\n VALUE:");
        message += HTTP.arg(i);
        message += '\n';
    }
    message += "path=";
    message += HTTP.arg("path");
    message += '\n';

    return replyNotFound(message);
}

void handleGetEdit() {
    if (handleFileRead(F("/edit.htm"))) {
        return;
    }

#ifdef INCLUDE_FALLBACK_INDEX_HTM
    server.sendHeader(F("Content-Encoding"), "gzip");
    server.send(200, "text/html", index_htm_gz, index_htm_gz_len);
#else
    replyNotFound(FPSTR(FILE_NOT_FOUND));
#endif
}

#endif