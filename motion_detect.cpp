#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>  // For mkdir
#include <unistd.h>

#include <algorithm>  // For std::remove_if for filename sanitization
#include <atomic>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>  // For std::put_time
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// --- Configuration ---
const int CAMERA_INDEX = 0;     // Adjust if your camera is not /dev/video0
const double CAP_WIDTH = 1280;  // Frame width for processing
const double CAP_HEIGHT = 720;  // Frame height for processing
const double CAP_FPS = 30.0;    // Desired camera FPS
const int HTTP_PORT = 8080;
const std::string RECORDINGS_DIR = "recordings";  // Directory to save videos

// Motion detection parameters
const int GAUSSIAN_BLUR_SIZE =
    15;  // Kernel size for Gaussian blur (odd number)
const double THRESHOLD_VALUE = 25;  // Threshold for detecting changes
const int MIN_NON_ZERO_COUNT =
    1000;  // Minimum non-zero pixels to trigger motion (adjust based on
           // resolution and sensitivity)
const int VIDEO_RECORD_DURATION_SECONDS =
    5;  // Duration of recorded video clips
const int MAX_RECENT_DETECTIONS =
    20;  // Max number of video clips to keep track of

// --- Global Shared Resources ---
struct MotionArtifact {
  std::string video_filename;
  std::string timestamp;
  std::string pretty_timestamp;  // For display
};

std::deque<MotionArtifact> gRecentDetections;
std::mutex gDetectionMutex;  // Protects g_recentDetections

cv::VideoCapture gCap;
std::mutex gCameraMutex;  // Protects g_cap

std::atomic<bool> gIsLiveStreamingActive{false};
std::atomic<int> gLiveStreamClientCount{
    0};  // Number of active live stream clients

// --- Utility Functions ---
// Sanitize filename to prevent directory traversal and invalid characters
std::string sanitizeFilename(const std::string& filename) {
  std::string sanitized = filename;
  // Remove path components
  size_t lastSlash = sanitized.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    sanitized = sanitized.substr(lastSlash + 1);
  }
  // Allow only alphanumeric, underscore, hyphen, dot
  sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
                                 [](char c) {
                                   return !std::isalnum(c) && c != '_' &&
                                          c != '-' && c != '.';
                                 }),
                  sanitized.end());
  return sanitized;
}

// --- Motion Detection Loop ---
void motionDetectionLoop() {
  cv::Mat frame, gray, prevGray, diff;
  bool transientMotionDetected = false;

  // Ensure recordings directory exists
  mkdir(RECORDINGS_DIR.c_str(),
        0755);  // Read/write/execute for owner, read/execute for group/others

  std::cout << "[MotionDetector] Starting motion detection loop." << std::endl;

  while (true) {
    if (gIsLiveStreamingActive.load(std::memory_order_relaxed)) {
      if (transientMotionDetected) {
        std::cout
            << "[MotionDetector] Pausing motion detection for live stream."
            << std::endl;
        prevGray.release();  // Reset previous frame to avoid false positive
                             // after stream
        transientMotionDetected = false;
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(200));  // Sleep while live stream is active
      continue;
    }

    if (!transientMotionDetected) {
      std::cout << "[MotionDetector] Resuming motion detection." << std::endl;
      transientMotionDetected = true;
      // prevGray will be initialized on the next valid frame
    }

    cv::Mat currentFrame;
    {
      std::lock_guard<std::mutex> lock(gCameraMutex);
      if (!gCap.isOpened()) {
        std::cerr << "[MotionDetector] Error: Camera not accessible in loop."
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }
      gCap >> frame;  // Grab frame
    }

    if (frame.empty()) {
      std::cerr << "[MotionDetector] Warning: Empty frame captured."
                << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(100));  // Wait a bit before retrying
      continue;
    }

    // It's good practice to clone the frame if it's going to be used by
    // VideoWriter while the original `frame` might be overwritten by the next
    // camera read.
    currentFrame = frame.clone();

    cv::cvtColor(currentFrame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray,
                     cv::Size(GAUSSIAN_BLUR_SIZE, GAUSSIAN_BLUR_SIZE), 0);

    if (!prevGray.empty()) {
      cv::absdiff(prevGray, gray, diff);
      cv::threshold(diff, diff, THRESHOLD_VALUE, 255, cv::THRESH_BINARY);
      int nonZeroCount = cv::countNonZero(diff);

      if (nonZeroCount > MIN_NON_ZERO_COUNT) {
        auto nowChrono = std::chrono::system_clock::now();
        std::time_t nowC = std::chrono::system_clock::to_time_t(nowChrono);
        std::tm nowTm = *std::localtime(
            &nowC);  // Note: localtime is not thread-safe but okay here

        char filenameBuf[128], timestampBuf[64], prettyTimestampBuf[64];
        std::strftime(filenameBuf, sizeof(filenameBuf),
                      "motion_%Y%m%d_%H%M%S.avi", &nowTm);
        std::strftime(timestampBuf, sizeof(timestampBuf), "%Y-%m-%dT%H:%M:%SZ",
                      &nowTm);  // ISO 8601 like
        std::strftime(prettyTimestampBuf, sizeof(prettyTimestampBuf),
                      "%Y-%m-%d %H:%M:%S", &nowTm);

        std::string videoFilename = RECORDINGS_DIR + "/" + filenameBuf;
        std::cout << "[MotionDetector] Motion detected! Recording "
                  << videoFilename << std::endl;

        cv::VideoWriter videoWriter;
        int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        cv::Size frameSize(
            static_cast<int>(gCap.get(cv::CAP_PROP_FRAME_WIDTH)),
            static_cast<int>(gCap.get(cv::CAP_PROP_FRAME_HEIGHT)));

        // Use the actual FPS from the camera if available and reasonable,
        // otherwise fallback
        double actualFps = gCap.get(cv::CAP_PROP_FPS);
        if (actualFps <= 0 || actualFps > 60)
          actualFps = CAP_FPS;  // Sanity check

        videoWriter.open(videoFilename, fourcc, actualFps, frameSize, true);

        if (!videoWriter.isOpened()) {
          std::cerr << "[MotionDetector] Error: Could not open VideoWriter for "
                    << videoFilename << std::endl;
        } else {
          auto recordingStartTime = std::chrono::steady_clock::now();
          int framesWritten = 0;
          while (std::chrono::steady_clock::now() - recordingStartTime <
                 std::chrono::seconds(VIDEO_RECORD_DURATION_SECONDS)) {
            cv::Mat videoFrame;
            {  // Scope for camera lock
              std::lock_guard<std::mutex> lock(gCameraMutex);
              if (!gCap.isOpened() ||
                  gIsLiveStreamingActive.load(
                      std::memory_order_relaxed)) {  // Check live stream status
                                                     // during recording
                std::cout << "[MotionDetector] Live stream started during "
                             "recording, aborting video save."
                          << std::endl;
                break;
              }
              gCap >> videoFrame;
            }
            if (videoFrame.empty()) break;
            videoWriter.write(videoFrame.clone());  // Write a clone
            framesWritten++;
            // Small delay to roughly match desired FPS if camera is faster
            // This is a simple way; a more precise frame timing mechanism could
            // be used.
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<long long>(1000.0 / actualFps / 2.0)));
          }
          videoWriter.release();
          std::cout << "[MotionDetector] Finished recording " << videoFilename
                    << ", " << framesWritten << " frames." << std::endl;

          if (framesWritten > 0) {  // Only add if some frames were written
            std::lock_guard<std::mutex> lock(gDetectionMutex);
            gRecentDetections.push_front(
                {filenameBuf, timestampBuf,
                 prettyTimestampBuf});  // Store only base filename
            if (gRecentDetections.size() > MAX_RECENT_DETECTIONS) {
              // Optional: Delete the oldest video file from disk
              // std::string oldFile = RECORDINGS_DIR + "/" +
              // g_recentDetections.back().videoFilename;
              // remove(oldFile.c_str());
              gRecentDetections.pop_back();
            }
          } else if (videoWriter.isOpened()) {  // If it was opened but no
                                                // frames, means it was aborted
            // videoWriter.release() was called. If the file was created empty,
            // delete it.
            remove(videoFilename.c_str());
            std::cout
                << "[MotionDetector] Recording aborted, deleted empty file: "
                << videoFilename << std::endl;
          }
        }
      }
    }
    prevGray = gray.clone();
    std::this_thread::sleep_for(std::chrono::milliseconds(
        static_cast<long long>(1000.0 / CAP_FPS)));  // Control processing rate
  }
  std::cout << "[MotionDetector] Exiting motion detection loop." << std::endl;
}

// --- HTTP Server ---
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

void startHttpServer() {
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd == -1) {
    perror("[HttpServer] socket creation failed");
    return;
  }

  // Allow address reuse
  int opt = 1;
  if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("[HttpServer] setsockopt SO_REUSEADDR failed");
    close(serverFd);
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(HTTP_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("[HttpServer] bind failed");
    close(serverFd);
    return;
  }

  if (listen(serverFd, 10) < 0) {  // Increased backlog queue
    perror("[HttpServer] listen failed");
    close(serverFd);
    return;
  }

  std::cout << "[HttpServer] Serving on http://0.0.0.0:" << HTTP_PORT
            << std::endl;

  while (true) {
    sockaddr_in clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientAddrLen);

    if (clientFd < 0) {
      perror("[HttpServer] accept failed");
      continue;  // Continue to accept other connections
    }

    // For simplicity, handling each client sequentially.
    // For concurrent handling, you'd typically spawn a new thread here:
    // std::thread(handleHttpClient, clientFd).detach();
    // However, for RPi Zero, sequential might be more stable without careful
    // thread management. The /live endpoint itself is blocking for that
    // connection.
    handleHttpClient(
        clientFd);  // This will block for /live until client disconnects
  }
  close(serverFd);  // Should be unreachable in this loop
}

// --- Main Function ---
int main() {
  std::cout << "Application starting..." << std::endl;
  std::cout << "Attempting to open camera index: " << CAMERA_INDEX << std::endl;
  std::cout << "Requested Resolution: " << CAP_WIDTH << "x" << CAP_HEIGHT
            << " @ " << CAP_FPS << "FPS" << std::endl;

  {  // Scope for camera lock during initialization
    std::lock_guard<std::mutex> lock(gCameraMutex);
    gCap.open(CAMERA_INDEX);
    if (!gCap.isOpened()) {
      // Try GStreamer pipeline as a fallback for Raspberry Pi
      std::string gstPipeline =
          "v4l2src device=/dev/video" + std::to_string(CAMERA_INDEX) +
          " ! video/x-raw,width=" +
          std::to_string(static_cast<int>(CAP_WIDTH)) +
          ",height=" + std::to_string(static_cast<int>(CAP_HEIGHT)) +
          ",framerate=" + std::to_string(static_cast<int>(CAP_FPS)) + "/1" +
          " ! videoconvert ! appsink";
      std::cout << "Camera index " << CAMERA_INDEX
                << " failed. Trying GStreamer: " << gstPipeline << std::endl;
      gCap.open(gstPipeline, cv::CAP_GSTREAMER);
    }

    if (!gCap.isOpened()) {
      std::cerr << "Error: Could not open camera using index " << CAMERA_INDEX
                << " or GStreamer. Exiting." << std::endl;
      return -1;
    }

    // Set camera parameters (some might not be settable if using GStreamer
    // pipeline string directly)
    gCap.set(cv::CAP_PROP_FRAME_WIDTH, CAP_WIDTH);
    gCap.set(cv::CAP_PROP_FRAME_HEIGHT, CAP_HEIGHT);
    // gCap.set(cv::CAP_PROP_FPS, CAP_FPS);
  }

  std::cout << "Camera opened successfully." << std::endl;
  std::cout << "Actual Resolution: " << gCap.get(cv::CAP_PROP_FRAME_WIDTH)
            << "x" << gCap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
  std::cout << "Actual FPS: " << gCap.get(cv::CAP_PROP_FPS) << std::endl;

  // Ensure recordings directory exists before starting threads that might use
  // it
  if (mkdir(RECORDINGS_DIR.c_str(), 0755) != 0 && errno != EEXIST) {
    perror("Failed to create recordings directory");
    // Decide if this is a fatal error
  } else {
    std::cout << "Recordings will be saved to ./" << RECORDINGS_DIR
              << std::endl;
  }

  std::thread camThread(motionDetectionLoop);
  std::thread webThread(startHttpServer);

  std::cout << "Main: Motion detection and HTTP server threads started."
            << std::endl;

  camThread.join();
  webThread.join();

  {  // Scope for camera lock during release
    std::lock_guard<std::mutex> lock(gCameraMutex);
    if (gCap.isOpened()) {
      gCap.release();
    }
  }
  std::cout << "Application terminated." << std::endl;
  return 0;
}
