#ifndef FLUTTER_PLUGIN_HEIC_NATIVE_PLUGIN_H_
#define FLUTTER_PLUGIN_HEIC_NATIVE_PLUGIN_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

namespace heic_native {

class HeicNativePlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  HeicNativePlugin();

  virtual ~HeicNativePlugin();

  // Disallow copy and assign.
  HeicNativePlugin(const HeicNativePlugin&) = delete;
  HeicNativePlugin& operator=(const HeicNativePlugin&) = delete;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

 private:
  bool ConvertHeicToPng(const std::string &input_path,
                        const std::string &output_path,
                        int compression_level, bool preserve_metadata,
                        std::string &error_code, std::string &error_message);
  std::vector<uint8_t> ConvertHeicToPngBytes(const std::string &input_path,
                                             int compression_level,
                                             bool preserve_metadata,
                                             std::string &error_code,
                                             std::string &error_message);

  // Worker thread infrastructure
  std::thread worker_thread_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::queue<std::function<void()>> work_queue_;
  std::atomic<bool> shutting_down_{false};

  // Main-thread result posting
  HWND flutter_window_{nullptr};
  static constexpr UINT WM_HEIC_NATIVE_RESULT = WM_APP + 1;
  std::mutex result_mutex_;
  std::queue<std::function<void()>> result_queue_;

  void WorkerLoop();
  void PostResultToMainThread(std::function<void()> callback);
  static LRESULT CALLBACK ResultSubclassProc(HWND, UINT, WPARAM, LPARAM,
                                              UINT_PTR, DWORD_PTR);
};

}  // namespace heic_native

#endif  // FLUTTER_PLUGIN_HEIC_NATIVE_PLUGIN_H_
