#include "file_manager.h"

AsyncWebServer server(80);

void refreshMappings() {
    mappingCount = 0;
    maxRank = 0;
    File mappingFile = SD.open(mapping_file, FILE_READ);

    #if DEBUG
        Serial.println("Refreshing mappings");
    #endif

    if (mappingFile) {
        char line[AUDIOFILENAME_MAX + PHONENUMBER_MAX + 6];
        while (mappingFile.available() && mappingCount < MAPPING_MAX) {
            size_t len = mappingFile.readBytesUntil('\n', line, sizeof(line) - 1);
            line[len] = '\0';

            char *colonIndex = strchr(line, ',');
            if (colonIndex) {
                *colonIndex = '\0';
                char *remaining = colonIndex + 1;
                int number = atoi(remaining);
                if (number == -1) {
                    strcpy(notAvailableFileName, line);
                } else if (number == -2) {
                    strcpy(noNumberFileName, line);
                }
                char *rankIndex = strchr(remaining, ',');
                if (rankIndex) {
                    *rankIndex = '\0';
                    int rank = atoi(rankIndex + 1);
                    if (rank > maxRank) {
                        maxRank = rank;
                    }

                    strcpy(mappings[mappingCount].fileName, line);
                    mappings[mappingCount].phoneNumber = number;
                    mappings[mappingCount].rank = rank;

                    #if DEBUG
                        Serial.print(" - ");
                        Serial.print(mappings[mappingCount].fileName);
                        Serial.print(",");
                        Serial.print(mappings[mappingCount].phoneNumber);
                        Serial.print(",");
                        Serial.println(mappings[mappingCount].rank);
                    #endif

                    mappingCount++;
                }
            }
        }
        #if DEBUG
            Serial.printf("Total mappings: %d, Max rank: %d\n", mappingCount, maxRank);
        #endif
        mappingFile.close();
    } else {
        #if DEBUG
            Serial.println("Failed to open mapping file");
        #endif
    }
}

#if USE_WEBSERVER
void setup_web_server() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        #if DEBUG
            Serial.println("HTTP /");
        #endif

        File indexFile = SD.open("/index.html", FILE_READ);
        if (!indexFile) {
            #if DEBUG
                Serial.println("index.html file not available");
            #endif
            String fallbackHtml = "<html><body><h1>Default Page</h1><p>The index.html file is missing.</p></body></html>";
            request->send(200, "text/html", fallbackHtml);
            return;
        }


        String fileList = "<table border='1'><tr><th>File Name</th><th>Phone Number</th><th>Solution</th><th>Actions</th></tr>";
        File root = SD.open("/");
        File file = root.openNextFile();

        while (file) {
            if (String(file.name()).endsWith(".wav")) {
                fileList += "<tr>";
                fileList += "<td>" + String(file.name()) + "</td>";
                String phoneNumber = "";
                int rank = 0;
                for(int i = 0; i < mappingCount; i++) {
                    if (!strcmp(mappings[i].fileName, file.name())) {
                        phoneNumber = String(mappings[i].phoneNumber);
                        rank = mappings[i].rank;
                        break;
                    }
                }
               
                fileList += "<td><input type='text' id='" + String(file.name()) + "_number' value='" + phoneNumber + "'></td>";
                fileList += "<td><input type='number' id='" + String(file.name()) + "_rank' value='" + (rank >= 0 ? String(rank) : "") + "'></td>";
                fileList += "<td><button class='btn-danger' onclick=\"deleteFile('" + String(file.name()) + "')\">Delete</button></td>";
                fileList += "</tr>";
            }
            file = root.openNextFile();
        }
        fileList += "</table>";

        String htmlContent = "";
        while (indexFile.available()) {
            htmlContent += indexFile.readStringUntil('\n');
        }
        indexFile.close();

        htmlContent.replace("%FILELIST%", fileList);

        request->send(200, "text/html", htmlContent);
    });

    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
        #if DEBUG
            Serial.println("HTTP /upload");
        #endif
        request->send(200, "text/plain", "File uploaded successfully");
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            if (!filename.endsWith(".wav")) {
                #if DEBUG
                    Serial.printf("Invalid file type: %s\n", filename.c_str());
                #endif
                request->send(400, "text/plain", "Only .wav files are allowed");
                return;
            }
            Serial.printf("UploadStart: %s\n", filename.c_str());
            request->_tempFile = SD.open("/" + filename, FILE_WRITE);
        }
        if (request->_tempFile) {
            request->_tempFile.write(data, len);
        }
        if (final) {
            #if DEBUG
                Serial.printf("UploadEnd: %s, %u bytes\n", filename.c_str(), index + len);
            #endif
            if (request->_tempFile) {
                request->_tempFile.close();
            }
        }
    });

    server.on("/save_all", HTTP_POST, [](AsyncWebServerRequest *request) {
        #if DEBUG
            Serial.println("HTTP /save_all");
        #endif
        if (request->hasParam("mappings", true)) {
            String mappingsData = request->getParam("mappings", true)->value();

            #if DEBUG
                Serial.println(mappingsData);
            #endif

            File mappingFile = SD.open(mapping_file, FILE_WRITE);
            if (mappingFile) {
                mappingFile.print(mappingsData);
                mappingFile.close();
                refreshMappings(); // Refresh mappings after saving
                request->send(200, "text/plain", "All mappings saved successfully");
            } else {
                request->send(500, "text/plain", "Failed to save mappings");
            }
        } else {
            request->send(400, "text/plain", "Invalid parameters");
        }
    });

    server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
        #if DEBUG
            Serial.println("HTTP /delete");
        #endif
        if (request->hasParam("file", true)) {
            String file = request->getParam("file", true)->value();
            #if DEBUG
                Serial.printf("Deleting file: %s\n", file.c_str());
            #endif
            if (SD.remove("/" + file)) {
                request->send(200, "text/plain", "File deleted successfully");
            } else {
                request->send(500, "text/plain", "Failed to delete file");
            }
        } else {
            #if DEBUG
                Serial.println("Missing 'file' parameter in request");
            #endif
            request->send(400, "text/plain", "Invalid parameters");
        }
    });

    server.begin();
    Serial.println("Web server started");
}
#endif // USE_WEBSERVER