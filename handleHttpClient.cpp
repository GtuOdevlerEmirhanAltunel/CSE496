#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "defines.hpp"
#include "utils.hpp"

void handleHttpClient(int clientFd) {
  char buffer[2048] = {0};  // Increased buffer size
  ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);

  if (bytesRead <= 0) {
    perror("[HttpServer] read error or client disconnected prematurely");
    close(clientFd);
    return;
  }
  buffer[bytesRead] = '\0';  // Null-terminate the received data

  std::istringstream req(buffer);
  std::string method, path, version;
  req >> method >> path >> version;

  std::cout << "[HttpServer] Request: " << method << " " << path << std::endl;

  std::ostringstream response;

  if (method == "GET") {
    if (path == "/") {
      response
          << "HTTP/1.1 200 OK\r\n"
          << "Content-Type: text/html; charset=utf-8\r\n"
          << "Connection: close\r\n\r\n"
          << "<!DOCTYPE html><html><head><title>RPi Camera</title>"
          << "<style>"
          << "body { font-family: Arial, sans-serif; margin: 20px; "
             "background-color: #f4f4f4; color: #333; }"
          << "h1 { color: #0056b3; }"
          << "a { color: #007bff; text-decoration: none; }"
          << "a:hover { text-decoration: underline; }"
          << ".container { background-color: #fff; padding: 20px; "
             "border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }"
          << "#detectionsList { list-style-type: none; padding: 0; }"
          << "#detectionsList li { background-color: #e9ecef; margin-bottom: "
             "8px; padding: 10px; border-radius: 4px; }"
          << "</style>"
          << "</head><body><div class='container'>"
          << "<h1>Camera Control Panel</h1>"
          << "<p><a href='/live'>View Live Stream</a> (stops motion "
             "detection)</p>"
          << "<h2>Recent Motion Detections (Videos)</h2>"
          << "<ul id='detectionsList'></ul>"
          << "<script>"
          << "function fetchDetections() {"
          << "  fetch('/detections')"
          << "    .then(response => response.json())"
          << "    .then(data => {"
          << "      const list = document.getElementById('detectionsList');"
          << "      list.innerHTML = '';"  // Clear old list
          << "      if (data.length === 0) { list.innerHTML = '<li>No "
             "detections yet.</li>'; }"
          << "      data.forEach(det => {"
          << "        const item = document.createElement('li');"
          << "        item.innerHTML = `${det.prettyTimestamp} - <a "
             "href='/videos/${det.videoFilename}' "
             "target='_blank'>${det.videoFilename}</a>`;"
          << "        list.appendChild(item);"
          << "      });"
          << "    }).catch(err => { console.error('Error fetching "
             "detections:', err); const list = "
             "document.getElementById('detectionsList'); list.innerHTML = "
             "'<li>Error loading detections.</li>'; });"
          << "}"
          << "fetchDetections(); setInterval(fetchDetections, 15000); // "
             "Refresh every 15 seconds"
          << "</script>"
          << "</div></body></html>";
      send(clientFd, response.str().c_str(), response.str().length(), 0);

    } else if (path == "/live") {
      gLiveStreamClientCount.fetch_add(1, std::memory_order_relaxed);
      gIsLiveStreamingActive.store(true, std::memory_order_relaxed);
      std::cout << "[HttpServer] Live stream client connected. Active clients: "
                << gLiveStreamClientCount.load() << std::endl;

      std::string boundary = "--FRAME_BOUNDARY";
      response << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: multipart/x-mixed-replace; boundary="
               << boundary << "\r\n"
               << "Connection: close\r\n"  // Keep-alive can be complex with raw
                                           // sockets and MJPEG
               << "Cache-Control: no-cache, no-store, must-revalidate\r\n"
               << "Pragma: no-cache\r\n"
               << "Expires: 0\r\n\r\n";

      if (send(clientFd, response.str().c_str(), response.str().length(),
               MSG_NOSIGNAL) < 0) {
        perror("[HttpServer] Error sending live stream headers");
      } else {
        cv::Mat liveFrame;
        std::vector<uchar> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY,
                                   90};  // JPEG quality

        while (gIsLiveStreamingActive.load(
            std::memory_order_relaxed)) {  // Check global flag too
          {
            std::lock_guard<std::mutex> lock(gCameraMutex);
            if (!gCap.isOpened()) {
              std::cerr << "[HttpServer] Live stream: Camera not available."
                        << std::endl;
              break;
            }
            gCap >> liveFrame;
          }

          if (liveFrame.empty()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(30));  // Wait if frame is empty
            continue;
          }

          cv::imencode(".jpg", liveFrame, buf, params);

          std::ostringstream frameHeader;
          frameHeader << boundary << "\r\n"
                      << "Content-Type: image/jpeg\r\n"
                      << "Content-Length: " << buf.size() << "\r\n\r\n";

          // Send frame header
          if (send(clientFd, frameHeader.str().c_str(),
                   frameHeader.str().length(), MSG_NOSIGNAL) < 0) {
            // perror("[HttpServer] Live stream: send frame header failed");
            std::cout
                << "[HttpServer] Live stream client disconnected (header send)."
                << std::endl;
            break;
          }
          // Send frame data
          if (send(clientFd, buf.data(), buf.size(), MSG_NOSIGNAL) < 0) {
            // perror("[HttpServer] Live stream: send frame data failed");
            std::cout
                << "[HttpServer] Live stream client disconnected (data send)."
                << std::endl;
            break;
          }
          std::this_thread::sleep_for(
              std::chrono::milliseconds(static_cast<long long>(
                  1000.0 / CAP_FPS)));  // Stream at camera FPS
        }
      }

      if (gLiveStreamClientCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
        gIsLiveStreamingActive.store(
            false, std::memory_order_relaxed);  // Last client disconnected
        std::cout << "[HttpServer] Last live stream client disconnected. "
                     "Resuming motion detection."
                  << std::endl;
      } else {
        std::cout
            << "[HttpServer] Live stream client disconnected. Active clients: "
            << gLiveStreamClientCount.load() << std::endl;
      }

    } else if (path == "/detections") {
      std::ostringstream jsonBody;
      jsonBody << "[";
      {
        std::lock_guard<std::mutex> lock(gDetectionMutex);
        for (size_t i = 0; i < gRecentDetections.size(); ++i) {
          const auto& det = gRecentDetections[i];
          jsonBody << "{\"timestamp\":\"" << det.timestamp << "\","
                   << "\"prettyTimestamp\":\"" << det.pretty_timestamp << "\","
                   << "\"videoFilename\":\"" << det.video_filename << "\"}";
          if (i < gRecentDetections.size() - 1) jsonBody << ",";
        }
      }
      jsonBody << "]";

      response << "HTTP/1.1 200 OK\r\n"
               << "Content-Type: application/json; charset=utf-8\r\n"
               << "Content-Length: " << jsonBody.str().length() << "\r\n"
               << "Connection: close\r\n\r\n"
               << jsonBody.str();
      send(clientFd, response.str().c_str(), response.str().length(), 0);

    } else if (path.rfind("/videos/", 0) ==
               0) {  // Check if path starts with /videos/
      std::string requestedFileBase = path.substr(8);  // Length of "/videos/"
      std::string safeFilename = sanitizeFilename(requestedFileBase);
      std::string fullFilepath = RECORDINGS_DIR + "/" + safeFilename;

      bool foundInDetections = false;
      {
        std::lock_guard<std::mutex> lock(gDetectionMutex);
        for (const auto& det : gRecentDetections) {
          if (det.video_filename == safeFilename) {
            foundInDetections = true;
            break;
          }
        }
      }

      if (!foundInDetections) {
        std::cerr
            << "[HttpServer] Video file not in recent detections or unsafe: "
            << requestedFileBase << std::endl;
        response << "HTTP/1.1 404 Not Found\r\nContent-Type: "
                    "text/plain\r\nConnection: close\r\n\r\nVideo not found or "
                    "access denied.";
        send(clientFd, response.str().c_str(), response.str().length(), 0);
      } else {
        std::ifstream file(fullFilepath, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
          std::streamsize size = file.tellg();
          file.seekg(0, std::ios::beg);
          std::vector<char> videoBuffer(size);
          if (file.read(videoBuffer.data(), size)) {
            response
                << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: video/avi\r\n"  // Assuming MJPEG is in AVI
                << "Content-Length: " << size << "\r\n"
                << "Connection: close\r\n\r\n";
            send(clientFd, response.str().c_str(), response.str().length(), 0);
            send(clientFd, videoBuffer.data(), size, 0);
          } else {
            std::cerr << "[HttpServer] Error reading video file: "
                      << fullFilepath << std::endl;
            response << "HTTP/1.1 500 Internal Server Error\r\nContent-Type: "
                        "text/plain\r\nConnection: close\r\n\r\nError reading "
                        "video file.";
            send(clientFd, response.str().c_str(), response.str().length(), 0);
          }
          file.close();
        } else {
          std::cerr << "[HttpServer] Video file not found on disk: "
                    << fullFilepath << std::endl;
          response << "HTTP/1.1 404 Not Found\r\nContent-Type: "
                      "text/plain\r\nConnection: close\r\n\r\nVideo file not "
                      "found on disk.";
          send(clientFd, response.str().c_str(), response.str().length(), 0);
        }
      }
    } else {
      response << "HTTP/1.1 404 Not Found\r\nContent-Type: "
                  "text/plain\r\nConnection: close\r\n\r\nEndpoint not found.";
      send(clientFd, response.str().c_str(), response.str().length(), 0);
    }
  } else {
    response << "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: "
                "text/plain\r\nConnection: close\r\n\r\nMethod not allowed.";
    send(clientFd, response.str().c_str(), response.str().length(), 0);
  }

  close(clientFd);
}
