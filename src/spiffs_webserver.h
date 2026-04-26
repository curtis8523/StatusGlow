/**
 * Minimal SPIFFS file helpers and endpoints used by the embedded WebServer.
 */
extern bool isAdminRequestAuthorized();

static File gFsUploadFile;
static String gFsUploadTargetPath;

static String normalizeSpiffsPath(String path) {
	path.replace('\\', '/');
	path.trim();
	if (path.length() == 0) path = "/";
	if (!path.startsWith("/")) path = "/" + path;
	while (path.indexOf("//") >= 0) {
		path.replace("//", "/");
	}
	if (path.indexOf("..") >= 0) {
		return String();
	}
	return path;
}

bool exists(String path) {
	// Correct existence check: ensure the file handle is valid before testing directory flag
	bool yes = false;
	File file = SPIFFS.open(path, "r");
	if (file) {
		if (!file.isDirectory()) {
			yes = true;
		}
		file.close();
	}
	return yes;
}

void handleMinimalUpload() {
	server.sendHeader("Access-Control-Allow-Origin", "*");
	String keyField;
	if (server.hasArg("key")) {
		keyField = String("<input type=\"hidden\" name=\"key\" value=\"") + htmlEscape(server.arg("key")) + "\">";
	}
	String page;
	page.reserve(512 + keyField.length());
	page += F("<!DOCTYPE html><html><head><title>Upload to SPIFFS</title><meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><body><form action=\"/fs/upload\" method=\"post\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"><input type=\"text\" name=\"path\" value=\"/\">");
	page += keyField;
	page += F("<button>Upload</button></form></body></html>");
	server.send(200, "text/html", page);
}

void handleFileUpload() {
	if (!isAdminRequestAuthorized()) return;

	HTTPUpload &upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		String filename = normalizeSpiffsPath(upload.filename);
		String requestedPath = normalizeSpiffsPath(server.arg("path"));
		if (filename.length() == 0) {
			return;
		}
		if (requestedPath.length() == 0) {
			requestedPath = "/";
		}
		if (requestedPath.endsWith("/")) {
			gFsUploadTargetPath = requestedPath + filename.substring(1);
		} else if (requestedPath == "/") {
			gFsUploadTargetPath = filename;
		} else {
			gFsUploadTargetPath = requestedPath;
		}
		gFsUploadTargetPath = normalizeSpiffsPath(gFsUploadTargetPath);
		if (gFsUploadTargetPath.length() == 0 || gFsUploadTargetPath == "/") {
			return;
		}
		DBG_PRINT("handleFileUpload Name: ");
		DBG_PRINTLN(gFsUploadTargetPath);
		if (gFsUploadFile) {
			gFsUploadFile.close();
		}
		gFsUploadFile = SPIFFS.open(gFsUploadTargetPath, "w");
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (gFsUploadFile) {
			gFsUploadFile.write(upload.buf, upload.currentSize);
		}
	} else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
		if (gFsUploadFile) {
			gFsUploadFile.close();
		}
		if (upload.status == UPLOAD_FILE_ABORTED && gFsUploadTargetPath.length()) {
			SPIFFS.remove(gFsUploadTargetPath);
		}
		gFsUploadTargetPath = "";
		DBG_PRINT("handleFileUpload Size: ");
		DBG_PRINTLN(upload.totalSize);
	}
}

void handleFileDelete() {
	if (server.args() == 0) {
		sendApiError(400, "missing_path", "Provide the SPIFFS path to delete.");
		return;
	}
	String path = server.arg(0);
	DBG_PRINTLN("handleFileDelete: " + path);
	if (path == "/") {
		sendApiError(400, "invalid_path", "Refusing to delete the SPIFFS root.");
		return;
	}
	if (!exists(path)) {
		sendApiError(404, "file_not_found", "The requested SPIFFS path does not exist.");
		return;
	}
	SPIFFS.remove(path);
	sendApiOk(200);
	path = String();
}

void handleFileList() {
	if (!server.hasArg("dir")) {
		sendApiError(400, "missing_dir", "Provide a SPIFFS directory path to list.");
		return;
	}

	String path = server.arg("dir");
	DBG_PRINTLN("handleFileList: " + path);

	File root = SPIFFS.open(path);
	path = String();
	if (!root || !root.isDirectory()) {
		sendApiError(404, "dir_not_found", "The requested SPIFFS directory does not exist.");
		return;
	}
	JsonDocument doc;
	JsonArray files = doc.to<JsonArray>();
	File file = root.openNextFile();
	while (file) {
		JsonObject entry = files.add<JsonObject>();
		entry["type"] = file.isDirectory() ? "dir" : "file";
		const char* fileName = file.name();
		entry["name"] = (fileName && fileName[0] == '/') ? (fileName + 1) : fileName;
		file = root.openNextFile();
	}
	sendJsonDocument(200, doc);
}

String getContentType(String filename) {
	if (server.hasArg("download")) {
		return "application/octet-stream";
	} else if (filename.endsWith(".htm")) {
		return "text/html";
	} else if (filename.endsWith(".html")) {
		return "text/html";
	} else if (filename.endsWith(".svg")) {
		return "image/svg+xml";
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

bool handleFileRead(String path) {
	DBG_PRINTLN("handleFileRead: " + path);
	if (path.endsWith("/")) {
		path += "index.html";
	}
	String contentType = getContentType(path);
	String servedPath = path;
	String pathWithGz = path + ".gz";
	String acceptEncoding = server.header("Accept-Encoding");
	acceptEncoding.toLowerCase();
	const bool clientAcceptsGzip = acceptEncoding.indexOf("gzip") >= 0;
	const bool hasGzipVariant = exists(pathWithGz);
	const bool serveGzip = hasGzipVariant && clientAcceptsGzip;
	if (serveGzip || exists(path)) {
		if (serveGzip) {
			servedPath += ".gz";
		}
		if (contentType == "text/html") {
			server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
			server.sendHeader("Pragma", "no-cache");
			server.sendHeader("Expires", "0");
		} else {
			server.sendHeader("Cache-Control", "public, max-age=86400");
		}
		if (serveGzip) {
			server.sendHeader("Vary", "Accept-Encoding");
		}
		File file = SPIFFS.open(servedPath, "r");
		server.streamFile(file, contentType);
		file.close();
		return true;
	}
	return false;
}
