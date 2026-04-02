#include "heic_native_core.h"

#include <jni.h>
#include <string>
#include <vector>

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_hugocornellier_heic_1native_HeicNativePlugin_nativeConvert(
    JNIEnv* env,
    jobject /*thiz*/,
    jstring input_path_j,
    jstring output_path_j,
    jint compression_level,
    jboolean preserve_metadata) {
  const char* input_path = env->GetStringUTFChars(input_path_j, nullptr);
  const char* output_path = env->GetStringUTFChars(output_path_j, nullptr);

  std::string error_code;
  std::string error_message;
  bool ok = heic_native_convert(input_path, output_path,
                             static_cast<int>(compression_level),
                             static_cast<bool>(preserve_metadata),
                             error_code, error_message);

  env->ReleaseStringUTFChars(input_path_j, input_path);
  env->ReleaseStringUTFChars(output_path_j, output_path);

  if (!ok) {
    std::string msg = error_code + ": " + error_message;
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), msg.c_str());
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

JNIEXPORT jbyteArray JNICALL
Java_com_hugocornellier_heic_1native_HeicNativePlugin_nativeConvertToBytes(
    JNIEnv* env,
    jobject /*thiz*/,
    jstring input_path_j,
    jint compression_level,
    jboolean preserve_metadata) {
  const char* input_path = env->GetStringUTFChars(input_path_j, nullptr);

  std::string error_code;
  std::string error_message;
  std::vector<uint8_t> out_bytes;
  bool ok = heic_native_convert_to_bytes(input_path,
                                      static_cast<int>(compression_level),
                                      static_cast<bool>(preserve_metadata),
                                      out_bytes, error_code, error_message);

  env->ReleaseStringUTFChars(input_path_j, input_path);

  if (!ok) {
    std::string msg = error_code + ": " + error_message;
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), msg.c_str());
    return nullptr;
  }

  jbyteArray result = env->NewByteArray(static_cast<jsize>(out_bytes.size()));
  if (result) {
    env->SetByteArrayRegion(result, 0,
                            static_cast<jsize>(out_bytes.size()),
                            reinterpret_cast<const jbyte*>(out_bytes.data()));
  }
  return result;
}

}  // extern "C"
