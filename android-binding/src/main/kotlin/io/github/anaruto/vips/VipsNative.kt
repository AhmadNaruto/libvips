package io.github.anaruto.vips

import android.graphics.Bitmap
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Raw JNI bindings for libvips.
 * This class handles loading the native libraries in topological dependency order
 * and maps the external JNI functions directly.
 */
object VipsNative {
    private val initialized = AtomicBoolean(false)

    init {
        try {
            // Load the custom JNI bridge library.
            // On Android 6.0+ (API 23+), the dynamic linker automatically loads transitively
            // linked library dependencies (libvips, glib, etc.) in topological order.
            System.loadLibrary("vips_jni")
        } catch (e: UnsatisfiedLinkError) {
            System.err.println("VipsNative: Error loading native libvips shared libraries: ${e.message}")
            e.printStackTrace()
        }
    }

    /**
     * Initializes the libvips environment.
     */
    external fun init(): Boolean

    /**
     * Shuts down and releases the libvips threadpool and memory caches.
     */
    external fun shutdown()

    /**
     * Gets the current libvips version string.
     */
    external fun getVersion(): String

    /**
     * Configures the maximum number of worker threads libvips will use.
     */
    external fun setConcurrency(concurrency: Int)

    /**
     * Configures the maximum number of operations kept in the cache.
     */
    external fun setCacheMax(maxOperations: Int)

    /**
     * Configures the maximum memory cache size in bytes.
     */
    external fun setCacheMaxMem(maxMemBytes: Long)

    /**
     * Configures the maximum number of open files cached.
     */
    external fun setCacheMaxFiles(maxFiles: Int)

    /**
     * Retrieves image dimensions and format info from a file path.
     * Returns a JSON formatted metadata string.
     */
    external fun getImageInfo(path: String): String

    /**
     * Retrieves image dimensions and format info from an encoded in-memory buffer.
     * Returns a JSON formatted metadata string.
     */
    external fun getImageInfoFromBuffer(buffer: ByteArray): String

    /**
     * Re-encodes/Compresses an image buffer to JPEG.
     */
    external fun compressJpeg(input: ByteArray, quality: Int): ByteArray?

    /**
     * Re-encodes/Compresses an image buffer to WebP.
     */
    external fun compressWebp(input: ByteArray, quality: Int): ByteArray?

    /**
     * Re-encodes/Compresses an image buffer to PNG.
     */
    external fun compressPng(input: ByteArray, compression: Int): ByteArray?

    /**
     * Converts between image formats (jpeg, png, webp).
     */
    external fun convertFormat(input: ByteArray, format: String): ByteArray?

    /**
     * Resizes an encoded image buffer and returns the encoded result.
     */
    external fun resize(input: ByteArray, scale: Double, outputFormat: String): ByteArray?

    /**
     * Resizes source ARGB_8888 bitmap pixels directly into target ARGB_8888 bitmap pixels.
     * Zero-copy parsing using Android NDK Bitmap pixel buffers.
     */
    external fun resizeBitmap(srcBitmap: Bitmap, dstBitmap: Bitmap): Boolean

    /**
     * Thread-safe initialization helper.
     */
    fun safeInit(): Boolean {
        if (initialized.get()) return true
        synchronized(initialized) {
            if (!initialized.get()) {
                if (init()) {
                    initialized.set(true)
                }
            }
        }
        return initialized.get()
    }

    fun isInitialized(): Boolean = initialized.get()
}
