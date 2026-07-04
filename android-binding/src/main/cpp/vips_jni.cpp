#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <vips/vips.h>
#include <string>
#include <cstring>
#include <thread>

// Conditional logging: Info and Error logging is disabled in Release builds.
// In Android, NDEBUG is defined for release builds.
#ifdef NDEBUG
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#else
#define LOG_TAG "VIPS_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

// JNI Exception Checks Helpers
#define CHECK_EXCEPTION(env) \
    if (env->ExceptionCheck()) { \
        return nullptr; \
    }

#define CHECK_EXCEPTION_BOOL(env) \
    if (env->ExceptionCheck()) { \
        return JNI_FALSE; \
    }

#define CHECK_EXCEPTION_VOID(env) \
    if (env->ExceptionCheck()) { \
        return; \
    }

extern "C" {

// ============================================================================
// RAII Classes for safe and automated resource lifecycle management
// ============================================================================

// RAII Wrapper for VipsImage* to ensure proper g_object_unref
class VipsImagePtr {
public:
    explicit VipsImagePtr(VipsImage *ptr = nullptr) : ptr_(ptr) {}
    ~VipsImagePtr() {
        if (ptr_) {
            g_object_unref(ptr_);
        }
    }
    VipsImage* get() const { return ptr_; }
    VipsImage* release() {
        VipsImage* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }
    void reset(VipsImage* ptr) {
        if (ptr_) {
            g_object_unref(ptr_);
        }
        ptr_ = ptr;
    }
    operator VipsImage*() const { return ptr_; }
    VipsImage* operator->() const { return ptr_; }
    bool operator!() const { return !ptr_; }
private:
    VipsImage *ptr_;
    VipsImagePtr(const VipsImagePtr&) = delete;
    VipsImagePtr& operator=(const VipsImagePtr&) = delete;
};

// RAII Lock/Unlock for Android Bitmap pixels
class BitmapPixelLock {
public:
    BitmapPixelLock(JNIEnv *env, jobject bitmap) : env_(env), bitmap_(bitmap), pixels_(nullptr), locked_(false) {
        if (bitmap_) {
            if (AndroidBitmap_lockPixels(env_, bitmap_, &pixels_) >= 0) {
                locked_ = true;
            }
        }
    }
    ~BitmapPixelLock() {
        if (locked_) {
            AndroidBitmap_unlockPixels(env_, bitmap_);
        }
    }
    void* pixels() const { return pixels_; }
    bool isLocked() const { return locked_; }
private:
    JNIEnv *env_;
    jobject bitmap_;
    void *pixels_;
    bool locked_;
    BitmapPixelLock(const BitmapPixelLock&) = delete;
    BitmapPixelLock& operator=(const BitmapPixelLock&) = delete;
};

// RAII String characters fetch & release
class JniStringChars {
public:
    JniStringChars(JNIEnv *env, jstring str) : env_(env), str_(str), chars_(nullptr) {
        if (str_) {
            chars_ = env_->GetStringUTFChars(str_, nullptr);
        }
    }
    ~JniStringChars() {
        if (chars_) {
            env_->ReleaseStringUTFChars(str_, chars_);
        }
    }
    const char* get() const { return chars_; }
    operator const char*() const { return chars_; }
    bool operator!() const { return !chars_; }
private:
    JNIEnv *env_;
    jstring str_;
    const char *chars_;
    JniStringChars(const JniStringChars&) = delete;
    JniStringChars& operator=(const JniStringChars&) = delete;
};

// RAII Byte array elements fetch & release with JNI_ABORT mode (read-only)
class JniByteArrayElements {
public:
    JniByteArrayElements(JNIEnv *env, jbyteArray array, jint mode = JNI_ABORT) 
        : env_(env), array_(array), elements_(nullptr), mode_(mode) {
        if (array_) {
            elements_ = env_->GetByteArrayElements(array_, nullptr);
        }
    }
    ~JniByteArrayElements() {
        if (elements_) {
            env_->ReleaseByteArrayElements(array_, elements_, mode_);
        }
    }
    jbyte* get() const { return elements_; }
    operator jbyte*() const { return elements_; }
    bool operator!() const { return !elements_; }
private:
    JNIEnv *env_;
    jbyteArray array_;
    jbyte *elements_;
    jint mode_;
    JniByteArrayElements(const JniByteArrayElements&) = delete;
    JniByteArrayElements& operator=(const JniByteArrayElements&) = delete;
};

// ============================================================================
// Concurrency Heuristic
// ============================================================================

// Returns optimal threads to spawn based on logical processor count.
// Caps threads to protect performance cores, prevent thermal throttling,
// and avoid UI main thread starvation on big.LITTLE architectures.
inline unsigned int get_optimal_concurrency(unsigned int cores) {
    if (cores <= 1) return 1;
    if (cores <= 3) return 2;
    if (cores <= 7) return cores - 1; // e.g. 4 cores -> 3 threads, leaving 1 for UI
    return cores / 2; // e.g. 8 cores -> 4 threads, utilizing big cores only
}

// ============================================================================
// Core Library Lifecycle & Optimizations
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_io_github_anaruto_vips_VipsNative_init(JNIEnv *env, jclass clazz) {
    if (VIPS_INIT("libvips_jni") != 0) {
        LOGE("Failed to initialize libvips: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }
    
    // Set optimized defaults for resource-constrained Android devices:
    unsigned int cores = std::thread::hardware_concurrency();
    unsigned int optimal_threads = get_optimal_concurrency(cores);
    vips_concurrency_set(optimal_threads);
    vips_cache_set_max_mem(50 * 1024 * 1024); // Limit cache memory to 50MB by default to prevent OOM
    vips_cache_set_max(50);                   // Limit cache size to 50 operations
    vips_cache_set_max_files(10);             // Limit max open files to 10
    
    LOGI("libvips initialized successfully (concurrency: %d threads, cache: 50MB). Version: %s", 
         vips_concurrency_get(), vips_version_string());
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_shutdown(JNIEnv *env, jclass clazz) {
    vips_shutdown();
    LOGI("libvips shutdown successfully");
}

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getVersion(JNIEnv *env, jclass clazz) {
    jstring result = env->NewStringUTF(vips_version_string());
    CHECK_EXCEPTION(env);
    return result;
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_setConcurrency(JNIEnv *env, jclass clazz, jint concurrency) {
    vips_concurrency_set(concurrency);
    LOGI("libvips concurrency configured to: %d threads", concurrency);
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_setCacheMax(JNIEnv *env, jclass clazz, jint maxOperations) {
    vips_cache_set_max(maxOperations);
    LOGI("libvips cache max operations configured to: %d", maxOperations);
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_setCacheMaxMem(JNIEnv *env, jclass clazz, jlong maxMemBytes) {
    vips_cache_set_max_mem(maxMemBytes);
    LOGI("libvips cache max memory configured to: %lld bytes", (long long)maxMemBytes);
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_setCacheMaxFiles(JNIEnv *env, jclass clazz, jint maxFiles) {
    vips_cache_set_max_files(maxFiles);
    LOGI("libvips cache max files configured to: %d", maxFiles);
}

// ============================================================================
// Image Info Query Operations
// ============================================================================

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getImageInfo(JNIEnv *env, jclass clazz, jstring path) {
    if (!path) return nullptr;
    JniStringChars cpath(env, path);
    if (!cpath.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_file(cpath, nullptr));
    if (!image) {
        LOGE("Failed to load image: %s. Error: %s", cpath.get(), vips_error_buffer());
        vips_error_clear();
        return env->NewStringUTF("{\"error\": \"Failed to load image from file\"}");
    }
    
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    VipsBandFormat format = vips_image_get_format(image);
    VipsInterpretation interpretation = vips_image_get_interpretation(image);
    
    size_t size_estimate = (size_t)width * height * bands * vips_format_sizeof(format);
    
    const char *format_nick = vips_enum_nick(VIPS_TYPE_BAND_FORMAT, format);
    if (!format_nick) format_nick = "unknown";
    const char *interpretation_nick = vips_enum_nick(VIPS_TYPE_INTERPRETATION, interpretation);
    if (!interpretation_nick) interpretation_nick = "unknown";
    
    char info[2048];
    snprintf(info, sizeof(info),
        "{\n"
        "  \"filename\": \"%s\",\n"
        "  \"width\": %d,\n"
        "  \"height\": %d,\n"
        "  \"bands\": %d,\n"
        "  \"format\": \"%s\",\n"
        "  \"interpretation\": \"%s\",\n"
        "  \"xres\": %.2f,\n"
        "  \"yres\": %.2f,\n"
        "  \"estimated_size_bytes\": %zu\n"
        "}",
        cpath.get(), width, height, bands,
        format_nick,
        interpretation_nick,
        vips_image_get_xres(image),
        vips_image_get_yres(image),
        size_estimate
    );
    
    jstring result = env->NewStringUTF(info);
    CHECK_EXCEPTION(env);
    return result;
}

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getImageInfoFromBuffer(JNIEnv *env, jclass clazz, jbyteArray buffer) {
    if (!buffer) return nullptr;
    jsize len = env->GetArrayLength(buffer);
    JniByteArrayElements data(env, buffer);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to load image from buffer: %s", vips_error_buffer());
        vips_error_clear();
        return env->NewStringUTF("{\"error\": \"Failed to load image from buffer\"}");
    }
    
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    VipsBandFormat format = vips_image_get_format(image);
    VipsInterpretation interpretation = vips_image_get_interpretation(image);
    
    size_t size_estimate = (size_t)width * height * bands * vips_format_sizeof(format);
    
    const char *format_nick = vips_enum_nick(VIPS_TYPE_BAND_FORMAT, format);
    if (!format_nick) format_nick = "unknown";
    const char *interpretation_nick = vips_enum_nick(VIPS_TYPE_INTERPRETATION, interpretation);
    if (!interpretation_nick) interpretation_nick = "unknown";
    
    char info[2048];
    snprintf(info, sizeof(info),
        "{\n"
        "  \"source\": \"buffer\",\n"
        "  \"buffer_size\": %d,\n"
        "  \"width\": %d,\n"
        "  \"height\": %d,\n"
        "  \"bands\": %d,\n"
        "  \"format\": \"%s\",\n"
        "  \"interpretation\": \"%s\",\n"
        "  \"estimated_size_bytes\": %zu\n"
        "}",
        len, width, height, bands,
        format_nick,
        interpretation_nick,
        size_estimate
    );
    
    jstring result = env->NewStringUTF(info);
    CHECK_EXCEPTION(env);
    return result;
}

// ============================================================================
// Raw Buffer / ByteArray Compressor & Conversion Operations
// ============================================================================

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressJpeg(JNIEnv *env, jclass clazz, jbyteArray input, jint quality) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    JniByteArrayElements data(env, input);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to parse source buffer for JPEG compression: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_jpegsave_buffer(image, &outBuf, &outLen, "Q", quality, nullptr) != 0) {
        LOGE("Failed to encode JPEG: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    if (result) {
        env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    }
    
    g_free(outBuf);
    CHECK_EXCEPTION(env);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressWebp(JNIEnv *env, jclass clazz, jbyteArray input, jint quality) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    JniByteArrayElements data(env, input);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to parse source buffer for WebP compression: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_webpsave_buffer(image, &outBuf, &outLen, "Q", quality, nullptr) != 0) {
        LOGE("Failed to encode WebP: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    if (result) {
        env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    }
    
    g_free(outBuf);
    CHECK_EXCEPTION(env);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressPng(JNIEnv *env, jclass clazz, jbyteArray input, jint compression) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    JniByteArrayElements data(env, input);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to parse source buffer for PNG compression: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_pngsave_buffer(image, &outBuf, &outLen, "compression", compression, nullptr) != 0) {
        LOGE("Failed to encode PNG: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    if (result) {
        env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    }
    
    g_free(outBuf);
    CHECK_EXCEPTION(env);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_convertFormat(JNIEnv *env, jclass clazz, jbyteArray input, jstring format) {
    if (!input || !format) return nullptr;
    JniStringChars formatStr(env, format);
    if (!formatStr.get()) return nullptr;
    
    jsize len = env->GetArrayLength(input);
    JniByteArrayElements data(env, input);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to parse source buffer: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    int result_code = -1;
    
    if (strcasecmp(formatStr, "jpeg") == 0 || strcasecmp(formatStr, "jpg") == 0) {
        result_code = vips_jpegsave_buffer(image, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "png") == 0) {
        result_code = vips_pngsave_buffer(image, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "webp") == 0) {
        result_code = vips_webpsave_buffer(image, &outBuf, &outLen, nullptr);
    } else {
        LOGE("Unsupported format for convert: %s", formatStr.get());
    }
    
    if (result_code != 0) {
        LOGE("Failed to convert image to %s. Error: %s", formatStr.get(), vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    jbyteArray output = env->NewByteArray(outLen);
    if (output) {
        env->SetByteArrayRegion(output, 0, outLen, (jbyte*)outBuf);
    }
    
    g_free(outBuf);
    CHECK_EXCEPTION(env);
    return output;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_resize(JNIEnv *env, jclass clazz, jbyteArray input, jdouble scale, jstring outputFormat) {
    if (!input || !outputFormat) return nullptr;
    JniStringChars formatStr(env, outputFormat);
    if (!formatStr.get()) return nullptr;
    
    jsize len = env->GetArrayLength(input);
    JniByteArrayElements data(env, input);
    if (!data.get()) return nullptr;
    
    VipsImagePtr image(vips_image_new_from_buffer(data, len, "", nullptr));
    if (!image) {
        LOGE("Failed to parse image for resizing: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    VipsImagePtr resized;
    VipsImage *resized_raw = nullptr;
    if (vips_resize(image, &resized_raw, scale, nullptr) != 0) {
        LOGE("Failed to resize image. Error: %s", vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    resized.reset(resized_raw);
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    int result_code = -1;
    
    if (strcasecmp(formatStr, "jpeg") == 0 || strcasecmp(formatStr, "jpg") == 0) {
        result_code = vips_jpegsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "png") == 0) {
        result_code = vips_pngsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "webp") == 0) {
        result_code = vips_webpsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else {
        LOGE("Unsupported format for resized image output: %s", formatStr.get());
    }
    
    if (result_code != 0) {
        LOGE("Failed to save resized image. Error: %s", formatStr.get(), vips_error_buffer());
        vips_error_clear();
        return nullptr;
    }
    
    jbyteArray output = env->NewByteArray(outLen);
    if (output) {
        env->SetByteArrayRegion(output, 0, outLen, (jbyte*)outBuf);
    }
    
    g_free(outBuf);
    CHECK_EXCEPTION(env);
    return output;
}

// ============================================================================
// Android Bitmap Pixel Processing Operations (Optimized for Android 12+)
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_io_github_anaruto_vips_VipsNative_resizeBitmap(JNIEnv *env, jclass clazz, jobject srcBitmap, jobject dstBitmap) {
    if (!srcBitmap || !dstBitmap) return JNI_FALSE;

    AndroidBitmapInfo srcInfo;
    if (AndroidBitmap_getInfo(env, srcBitmap, &srcInfo) < 0) {
        LOGE("Failed to get source Bitmap info");
        return JNI_FALSE;
    }

    AndroidBitmapInfo dstInfo;
    if (AndroidBitmap_getInfo(env, dstBitmap, &dstInfo) < 0) {
        LOGE("Failed to get destination Bitmap info");
        return JNI_FALSE;
    }

    if (srcInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888 || dstInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Error: Only ARGB_8888 format is supported for Bitmaps");
        return JNI_FALSE;
    }

    BitmapPixelLock srcLock(env, srcBitmap);
    if (!srcLock.isLocked()) {
        LOGE("Failed to lock source Bitmap pixels");
        return JNI_FALSE;
    }

    BitmapPixelLock dstLock(env, dstBitmap);
    if (!dstLock.isLocked()) {
        LOGE("Failed to lock destination Bitmap pixels");
        return JNI_FALSE;
    }

    void* srcPixels = srcLock.pixels();
    void* dstPixels = dstLock.pixels();

    // Create a VipsImage wrapper around the source Bitmap's raw memory address (zero-copy creation)
    VipsImagePtr image(vips_image_new_from_memory(
        srcPixels,
        (size_t)srcInfo.width * srcInfo.height * 4,
        srcInfo.width,
        srcInfo.height,
        4,
        VIPS_BAND_FORMAT_UCHAR
    ));
    if (!image) {
        LOGE("Failed to construct VipsImage wrapper from Bitmap pixels: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }

    // Set the interpretation to sRGB so filters are correctly calculated
    vips_image_set_interpretation(image, VIPS_INTERPRETATION_sRGB);

    // Calculate asymmetric scaling ratios
    double scale_x = (double)dstInfo.width / srcInfo.width;
    double scale_y = (double)dstInfo.height / srcInfo.height;
    double vscale = scale_y / scale_x;

    // PREMULTIPLY ALPHA: Multiply color channels by alpha channel prior to resizing.
    // This resolves dark borders/halos on transparent areas during interpolation.
    VipsImagePtr premultiplied;
    VipsImage *premultiplied_raw = nullptr;
    if (vips_premultiply(image, &premultiplied_raw, nullptr) != 0) {
        LOGE("Vips: Failed to premultiply alpha: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }
    premultiplied.reset(premultiplied_raw);

    // Resize using Lanczos-3 (default vips_resize kernel).
    // Specifying "vscale" allows asymmetric resizing (stretching) if target aspect ratio is different.
    VipsImagePtr resized;
    VipsImage *resized_raw = nullptr;
    if (vips_resize(premultiplied, &resized_raw, scale_x, "vscale", vscale, nullptr) != 0) {
        LOGE("Vips: Failed to execute resize operation: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }
    resized.reset(resized_raw);

    // UNPREMULTIPLY ALPHA: Divide color channels by alpha channel back to standard RGBA.
    VipsImagePtr unpremultiplied;
    VipsImage *unpremultiplied_raw = nullptr;
    if (vips_unpremultiply(resized, &unpremultiplied_raw, nullptr) != 0) {
        LOGE("Vips: Failed to unpremultiply alpha: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }
    unpremultiplied.reset(unpremultiplied_raw);

    // Ensure output has exactly 4 channels (RGBA) matching ARGB_8888 target format
    VipsImagePtr output;
    
    // Check and normalize format/bands
    VipsImage *temp = unpremultiplied.get();
    g_object_ref(temp);
    VipsImagePtr temp_ptr(temp);

    if (vips_image_get_interpretation(temp_ptr) != VIPS_INTERPRETATION_sRGB) {
        VipsImage *coloured = nullptr;
        if (vips_colourspace(temp_ptr, &coloured, VIPS_INTERPRETATION_sRGB, nullptr) == 0) {
            temp_ptr.reset(coloured);
        }
    }

    int bands = vips_image_get_bands(temp_ptr);
    VipsImage *rgba_raw = nullptr;

    if (bands == 4) {
        rgba_raw = temp_ptr.get();
        g_object_ref(rgba_raw);
    } else if (bands == 3) {
        VipsImage *alpha = nullptr;
        if (vips_black(&alpha, temp_ptr->Xsize, temp_ptr->Ysize, "bands", 1, nullptr) == 0) {
            VipsImage *alpha_full = nullptr;
            if (vips_linear1(alpha, &alpha_full, 1.0, 255.0, nullptr) == 0) {
                vips_bandjoin2(temp_ptr, alpha_full, &rgba_raw, nullptr);
            }
            if (alpha_full) g_object_unref(alpha_full);
        }
        if (alpha) g_object_unref(alpha);
    } else if (bands == 1) {
        VipsImage *rgb = nullptr;
        if (vips_colourspace(temp_ptr, &rgb, VIPS_INTERPRETATION_sRGB, nullptr) == 0) {
            VipsImage *alpha = nullptr;
            if (vips_black(&alpha, rgb->Xsize, rgb->Ysize, "bands", 1, nullptr) == 0) {
                VipsImage *alpha_full = nullptr;
                if (vips_linear1(alpha, &alpha_full, 1.0, 255.0, nullptr) == 0) {
                    vips_bandjoin2(rgb, alpha_full, &rgba_raw, nullptr);
                }
                if (alpha_full) g_object_unref(alpha_full);
            }
            if (alpha) g_object_unref(alpha);
            g_object_unref(rgb);
        }
    } else if (bands == 2) {
        VipsImage *gray = nullptr;
        VipsImage *alpha = nullptr;
        if (vips_extract_band(temp_ptr, &gray, 0, "n", 1, nullptr) == 0 &&
            vips_extract_band(temp_ptr, &alpha, 1, "n", 1, nullptr) == 0) {
            VipsImage *rgb = nullptr;
            if (vips_colourspace(gray, &rgb, VIPS_INTERPRETATION_sRGB, nullptr) == 0) {
                vips_bandjoin2(rgb, alpha, &rgba_raw, nullptr);
                g_object_unref(rgb);
            }
        }
        if (gray) g_object_unref(gray);
        if (alpha) g_object_unref(alpha);
    }

    if (!rgba_raw) {
        rgba_raw = temp_ptr.get();
        g_object_ref(rgba_raw);
    }
    output.reset(rgba_raw);

    if (vips_image_get_format(output) != VIPS_BAND_FORMAT_UCHAR) {
        VipsImage *cast_img = nullptr;
        if (vips_cast(output, &cast_img, VIPS_BAND_FORMAT_UCHAR, nullptr) == 0) {
            output.reset(cast_img);
        }
    }

    // Render/Write output image pixels directly to a newly allocated memory buffer
    size_t written_size = 0;
    void *out_pixels = vips_image_write_to_memory(output, &written_size);
    if (!out_pixels) {
        LOGE("Vips: Failed to write output pixels memory buffer: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }

    // Safely copy the rendered pixels into the locked destination bitmap
    size_t expected_size = (size_t)dstInfo.width * dstInfo.height * 4;
    size_t copy_size = (written_size < expected_size) ? written_size : expected_size;
    std::memcpy(dstPixels, out_pixels, copy_size);

    // Clean up allocated native resources
    g_free(out_pixels);
    return JNI_TRUE;
}

} // extern "C"
