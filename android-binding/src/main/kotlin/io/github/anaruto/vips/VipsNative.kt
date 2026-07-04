package io.github.anaruto.vips

import android.graphics.Bitmap

/**
 * Raw JNI bindings for libvips.
 * This class handles loading the native libraries in topological dependency order
 * and maps the external JNI functions directly.
 */
object VipsNative {
    private var initialized = false

    init {
        try {
            // Load native prebuilt dependencies in order of reference
            System.loadLibrary("z")
            System.loadLibrary("intl")
            System.loadLibrary("glib-2.0")
            System.loadLibrary("gmodule-2.0")
            System.loadLibrary("gobject-2.0")
            System.loadLibrary("gthread-2.0")
            System.loadLibrary("gio-2.0")
            try {
                System.loadLibrary("girepository-2.0")
            } catch (e: UnsatisfiedLinkError) {
                // Keep going, might be built without GI bindings
            }
            System.loadLibrary("vips")
            
            // Load the custom JNI bridge library
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
     * Thread-safe synchronized initialization helper.
     */
    @Synchronized
    fun safeInit(): Boolean {
        if (initialized) return true
        initialized = init()
        return initialized
    }

    fun isInitialized() = initialized
}
