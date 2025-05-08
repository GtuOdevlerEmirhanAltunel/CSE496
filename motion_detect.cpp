#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <deque>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <thread>

struct MotionDetection {
  std::string filename;
  std::string timestamp;
};

std::deque<MotionDetection> recentDetections;
std::mutex detectionMutex;

void motionDetectionLoop() {
  cv::VideoCapture cap(0);
  if (!cap.isOpened()) {
    std::cerr << "Error: Could not open camera\n";
    return;
  }

  cv::Mat frame, gray, prevGray, diff;

  while (true) {
    cap >> frame;
    if (frame.empty()) break;

    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(21, 21), 0);

    if (!prevGray.empty()) {
      cv::absdiff(prevGray, gray, diff);
      cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);
      int nonZeroCount = cv::countNonZero(diff);

      if (nonZeroCount > 5000) {
        auto now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        char filename[64], timestamp[64];
        std::strftime(filename, sizeof(filename), "motion_%Y%m%d_%H%M%S.jpg",
                      std::localtime(&now));
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now));

        cv::imwrite(filename, frame);

        std::lock_guard<std::mutex> lock(detectionMutex);
        recentDetections.push_front({filename, timestamp});
        if (recentDetections.size() > 10) recentDetections.pop_back();

        std::cout << "Motion: " << filename << "\n";
      }
    }

    prevGray = gray.clone();
    cv::waitKey(33);
  }
}

void startHttpServer() {
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd == -1) {
    perror("socket");
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return;
  }

  if (listen(serverFd, 5) < 0) {
    perror("listen");
    return;
  }

  std::cout << "Serving on http://0.0.0.0:8080\n";

  while (true) {
    int clientFd = accept(serverFd, nullptr, nullptr);
    if (clientFd < 0) {
      perror("accept");
      continue;
    }

    char buffer[1024] = {0};
    read(clientFd, buffer, sizeof(buffer));

    std::ostringstream body;
    {
      std::lock_guard<std::mutex> lock(detectionMutex);
      for (const auto& det : recentDetections)
        body << det.timestamp << " - " << det.filename << "\n";
    }

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/plain\r\n"
             << "Content-Length: " << body.str().size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body.str();

    send(clientFd, response.str().c_str(), response.str().size(), 0);
    close(clientFd);
  }
}

int main() {
  std::thread camThread(motionDetectionLoop);
  std::thread webThread(startHttpServer);

  camThread.join();
  webThread.join();

  return 0;
}
