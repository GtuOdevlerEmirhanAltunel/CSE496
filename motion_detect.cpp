#include <chrono>
#include <ctime>
#include <deque>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <thread>

#include "httplib.h"

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
  httplib::Server svr;

  svr.Get("/detections", [](const httplib::Request&, httplib::Response& res) {
    std::string body;
    {
      std::lock_guard<std::mutex> lock(detectionMutex);
      for (const auto& det : recentDetections) {
        body += det.timestamp + " - " + det.filename + "\n";
      }
    }
    res.set_content(body, "text/plain");
  });

  std::cout << "HTTP server on http://0.0.0.0:8080/detections\n";
  svr.listen("0.0.0.0", 8080);
}

int main() {
  std::thread camThread(motionDetectionLoop);
  std::thread webThread(startHttpServer);

  camThread.join();
  webThread.join();

  return 0;
}
