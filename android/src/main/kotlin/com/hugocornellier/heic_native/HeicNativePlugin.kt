package com.hugocornellier.heic_native

import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import android.os.Handler
import android.os.Looper
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class HeicNativePlugin : FlutterPlugin, MethodCallHandler {
    private lateinit var channel: MethodChannel
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val mainHandler: Handler = Handler(Looper.getMainLooper())

    companion object {
        init {
            System.loadLibrary("heic_native_android")
        }
    }

    private external fun nativeConvert(inputPath: String, outputPath: String, compressionLevel: Int, preserveMetadata: Boolean): Boolean
    private external fun nativeConvertToBytes(inputPath: String, compressionLevel: Int, preserveMetadata: Boolean): ByteArray

    override fun onAttachedToEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel = MethodChannel(binding.binaryMessenger, "heic_native")
        channel.setMethodCallHandler(this)
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
        executor.shutdown()
    }

    override fun onMethodCall(call: MethodCall, result: Result) {
        when (call.method) {
            "convert" -> handleConvert(call, result)
            "convertToBytes" -> handleConvertToBytes(call, result)
            else -> result.notImplemented()
        }
    }

    private fun handleConvert(call: MethodCall, result: Result) {
        val inputPath = call.argument<String>("inputPath")
        val outputPath = call.argument<String>("outputPath")
        if (inputPath == null || outputPath == null) {
            result.error("invalid_arguments", "inputPath and outputPath are required", null)
            return
        }

        val preserveMetadata = call.argument<Boolean>("preserveMetadata") ?: true
        val compressionLevel = (call.argument<Int>("compressionLevel") ?: 6).coerceIn(0, 9)

        executor.execute {
            try {
                nativeConvert(inputPath, outputPath, compressionLevel, preserveMetadata)
                mainHandler.post { result.success(true) }
            } catch (e: Exception) {
                val msg = e.message ?: "Unknown error"
                val colonIdx = msg.indexOf(": ")
                if (colonIdx != -1) {
                    val code = msg.substring(0, colonIdx)
                    val message = msg.substring(colonIdx + 2)
                    mainHandler.post { result.error(code, message, null) }
                } else {
                    mainHandler.post { result.error("conversion_failed", msg, null) }
                }
            }
        }
    }

    private fun handleConvertToBytes(call: MethodCall, result: Result) {
        val inputPath = call.argument<String>("inputPath")
        if (inputPath == null) {
            result.error("invalid_arguments", "inputPath is required", null)
            return
        }

        val preserveMetadata = call.argument<Boolean>("preserveMetadata") ?: true
        val compressionLevel = (call.argument<Int>("compressionLevel") ?: 6).coerceIn(0, 9)

        executor.execute {
            try {
                val bytes = nativeConvertToBytes(inputPath, compressionLevel, preserveMetadata)
                mainHandler.post { result.success(bytes) }
            } catch (e: Exception) {
                val msg = e.message ?: "Unknown error"
                val colonIdx = msg.indexOf(": ")
                if (colonIdx != -1) {
                    val code = msg.substring(0, colonIdx)
                    val message = msg.substring(colonIdx + 2)
                    mainHandler.post { result.error(code, message, null) }
                } else {
                    mainHandler.post { result.error("conversion_failed", msg, null) }
                }
            }
        }
    }
}
