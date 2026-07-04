package io.github.anaruto.vips

import android.graphics.Bitmap

/**
 * Developer-facing high-level Kotlin API for libvips on Android 12+.
 *
 * libvips is an extremely fast, multithreaded, and memory-efficient image processing library.
 * This class wraps raw JNI calls with input validations to prevent native segmentation faults.
 */
object Vips {
    private val SUPPORTED_FORMATS = setOf("jpeg", "jpg", "png", "webp")

    /**
     * Initializes the libvips runtime environment.
     * Must be called before performing any image operations.
     * @return True if initialized successfully, false otherwise.
     */
    fun init(): Boolean = VipsNative.safeInit()

    /**
     * Returns whether the libvips runtime has been successfully initialized.
     */
    fun isInitialized(): Boolean = VipsNative.isInitialized()

    /**
     * Retrieves the libvips version string.
     */
    fun getVersion(): String {
        checkInitialized()
        return VipsNative.getVersion()
    }

    /**
     * Retrieves image information (width, height, format, channels, estimated raw size)
     * as a JSON-formatted metadata string from a file path.
     */
    fun getImageInfo(path: String): String {
        checkInitialized()
        require(path.isNotBlank()) { "File path cannot be blank" }
        return VipsNative.getImageInfo(path)
    }

    /**
     * Retrieves image information as a JSON-formatted metadata string from an encoded byte array.
     */
    fun getImageInfo(buffer: ByteArray): String {
        checkInitialized()
        require(buffer.isNotEmpty()) { "Input buffer cannot be empty" }
        return VipsNative.getImageInfoFromBuffer(buffer)
    }

    /**
     * Compresses/re-encodes an image buffer to JPEG.
     * @param input Encoded source image bytes.
     * @param quality Compression quality (1 to 100, default is 85).
     * @return Encoded JPEG byte array, or null if compression failed.
     */
    fun compressJpeg(input: ByteArray, quality: Int = 85): ByteArray? {
        checkInitialized()
        require(input.isNotEmpty()) { "Input image data cannot be empty" }
        require(quality in 1..100) { "Quality must be between 1 and 100" }
        return VipsNative.compressJpeg(input, quality)
    }

    /**
     * Compresses/re-encodes an image buffer to WebP.
     * @param input Encoded source image bytes.
     * @param quality Compression quality (1 to 100, default is 85).
     * @return Encoded WebP byte array, or null if compression failed.
     */
    fun compressWebp(input: ByteArray, quality: Int = 85): ByteArray? {
        checkInitialized()
        require(input.isNotEmpty()) { "Input image data cannot be empty" }
        require(quality in 1..100) { "Quality must be between 1 and 100" }
        return VipsNative.compressWebp(input, quality)
    }

    /**
     * Compresses/re-encodes an image buffer to PNG.
     * @param input Encoded source image bytes.
     * @param compression Zlib compression level (0 to 9, default is 6).
     * @return Encoded PNG byte array, or null if compression failed.
     */
    fun compressPng(input: ByteArray, compression: Int = 6): ByteArray? {
        checkInitialized()
        require(input.isNotEmpty()) { "Input image data cannot be empty" }
        require(compression in 0..9) { "Compression level must be between 0 and 9" }
        return VipsNative.compressPng(input, compression)
    }

    /**
     * Converts an encoded image buffer into a different format.
     * @param input Encoded source image bytes.
     * @param format Destination format ("jpeg", "png", "webp").
     * @return Transcoded image bytes, or null if transcoding failed.
     */
    fun convertFormat(input: ByteArray, format: String): ByteArray? {
        checkInitialized()
        require(input.isNotEmpty()) { "Input image data cannot be empty" }
        require(format.isNotBlank()) { "Target format cannot be blank" }
        val normalized = format.trim().lowercase()
        require(normalized in SUPPORTED_FORMATS) {
            "Unsupported format: $format. Supported: jpeg, png, webp"
        }
        return VipsNative.convertFormat(input, normalized)
    }

    /**
     * Resizes an encoded image buffer using high-quality Lanczos-3 filter.
     * @param input Encoded source image bytes.
     * @param scale Scaling factor (e.g. 0.5 to half the dimensions, 2.0 to double them).
     * @param outputFormat Format of the returned resized buffer ("jpeg", "png", "webp").
     * @return Encoded resized image bytes, or null if resize failed.
     */
    fun resize(input: ByteArray, scale: Double, outputFormat: String = "jpeg"): ByteArray? {
        checkInitialized()
        require(input.isNotEmpty()) { "Input image data cannot be empty" }
        require(scale > 0.0) { "Scale factor must be positive" }
        val normalized = outputFormat.trim().lowercase()
        require(normalized in SUPPORTED_FORMATS) {
            "Unsupported output format: $outputFormat. Supported: jpeg, png, webp"
        }
        return VipsNative.resize(input, scale, normalized)
    }

    /**
     * Resizes an Android Bitmap directly into a destination Bitmap.
     * Performs a high-performance zero-copy resizing using the NDK.
     * Supports asymmetric scaling if the target Bitmap aspect ratio differs.
     *
     * @param src Source bitmap (must be ARGB_8888, non-recycled).
     * @param dst Target destination bitmap (must be ARGB_8888, non-recycled).
     * @return True if successful, false otherwise.
     */
    fun resizeBitmap(src: Bitmap, dst: Bitmap): Boolean {
        checkInitialized()
        require(!src.isRecycled) { "Source bitmap is recycled" }
        require(!dst.isRecycled) { "Destination bitmap is recycled" }
        require(src.config == Bitmap.Config.ARGB_8888) { "Source bitmap must be Config.ARGB_8888" }
        require(dst.config == Bitmap.Config.ARGB_8888) { "Destination bitmap must be Config.ARGB_8888" }
        require(src.width > 0 && src.height > 0) { "Source bitmap must have dimensions greater than 0" }
        require(dst.width > 0 && dst.height > 0) { "Destination bitmap must have dimensions greater than 0" }
        
        return VipsNative.resizeBitmap(src, dst)
    }

    /**
     * Resizes a Bitmap to a target width, maintaining aspect ratio.
     * Automatically instantiates and returns a new Config.ARGB_8888 Bitmap of correct dimensions.
     *
     * @param src Source bitmap (must be ARGB_8888, non-recycled).
     * @param targetWidth Target width in pixels.
     * @return A newly allocated resized ARGB_8888 Bitmap.
     */
    fun resizeBitmap(src: Bitmap, targetWidth: Int): Bitmap {
        checkInitialized()
        require(targetWidth > 0) { "Target width must be greater than 0" }
        require(!src.isRecycled) { "Source bitmap is recycled" }
        require(src.config == Bitmap.Config.ARGB_8888) { "Source bitmap must be Config.ARGB_8888" }
        
        val scale = targetWidth.toDouble() / src.width.toDouble()
        val targetHeight = (src.height * scale).toInt().coerceAtLeast(1)
        
        val dst = Bitmap.createBitmap(targetWidth, targetHeight, Bitmap.Config.ARGB_8888)
        val success = VipsNative.resizeBitmap(src, dst)
        if (!success) {
            dst.recycle()
            throw RuntimeException("libvips failed to resize Bitmap")
        }
        return dst
    }

    /**
     * Sets the maximum number of worker threads libvips will use for parallel operations.
     * By default, it is set to the number of available CPU cores.
     */
    fun setConcurrency(concurrency: Int) {
        checkInitialized()
        require(concurrency >= 1) { "Concurrency must be at least 1" }
        VipsNative.setConcurrency(concurrency)
    }

    /**
     * Sets the maximum number of operations that libvips can keep in its cache.
     */
    fun setCacheMax(maxOperations: Int) {
        checkInitialized()
        require(maxOperations >= 0) { "Cache max operations must be non-negative" }
        VipsNative.setCacheMax(maxOperations)
    }

    /**
     * Sets the maximum amount of memory (in bytes) that libvips can use for its cache.
     * Setting this to 0 disables the cache memory limits.
     */
    fun setCacheMaxMem(maxMemBytes: Long) {
        checkInitialized()
        require(maxMemBytes >= 0L) { "Cache max memory must be non-negative" }
        VipsNative.setCacheMaxMem(maxMemBytes)
    }

    /**
     * Sets the maximum number of open files that libvips can cache.
     */
    fun setCacheMaxFiles(maxFiles: Int) {
        checkInitialized()
        require(maxFiles >= 0) { "Cache max files must be non-negative" }
        VipsNative.setCacheMaxFiles(maxFiles)
    }

    /**
     * Shuts down the libvips cache and worker threads.
     */
    fun shutdown() {
        if (VipsNative.isInitialized()) {
            VipsNative.shutdown()
        }
    }

    private fun checkInitialized() {
        check(VipsNative.isInitialized()) {
            "libvips is not initialized. Make sure you call Vips.init() before invoking image operations."
        }
    }
}
