#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <vips/vips.h>
#include <string>
#include <cstring>

#define LOG_TAG "VIPS_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

// ============================================================================
// Core Library Lifecycle
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_io_github_anaruto_vips_VipsNative_init(JNIEnv *env, jclass clazz) {
    if (VIPS_INIT("libvips_jni") != 0) {
        LOGE("Failed to initialize libvips: %s", vips_error_buffer());
        vips_error_clear();
        return JNI_FALSE;
    }
    LOGI("libvips initialized successfully. Version: %s", vips_version_string());
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_io_github_anaruto_vips_VipsNative_shutdown(JNIEnv *env, jclass clazz) {
    vips_shutdown();
    LOGI("libvips shutdown successfully");
}

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getVersion(JNIEnv *env, jclass clazz) {
    return env->NewStringUTF(vips_version_string());
}

// ============================================================================
// Image Info Query Operations
// ============================================================================

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getImageInfo(JNIEnv *env, jclass clazz, jstring path) {
    if (!path) return nullptr;
    const char *cpath = env->GetStringUTFChars(path, nullptr);
    
    VipsImage *image = vips_image_new_from_file(cpath, nullptr);
    if (!image) {
        LOGE("Failed to load image: %s. Error: %s", cpath, vips_error_buffer());
        vips_error_clear();
        env->ReleaseStringUTFChars(path, cpath);
        return env->NewStringUTF("{\"error\": \"Failed to load image from file\"}");
    }
    
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    VipsBandFormat format = vips_image_get_format(image);
    VipsInterpretation interpretation = vips_image_get_interpretation(image);
    
    size_t size_estimate = (size_t)width * height * bands * vips_format_sizeof(format);
    
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
        cpath, width, height, bands,
        vips_enum_nick(VIPS_TYPE_BAND_FORMAT, format),
        vips_enum_nick(VIPS_TYPE_INTERPRETATION, interpretation),
        vips_image_get_xres(image),
        vips_image_get_yres(image),
        size_estimate
    );
    
    g_object_unref(image);
    env->ReleaseStringUTFChars(path, cpath);
    return env->NewStringUTF(info);
}

JNIEXPORT jstring JNICALL
Java_io_github_anaruto_vips_VipsNative_getImageInfoFromBuffer(JNIEnv *env, jclass clazz, jbyteArray buffer) {
    if (!buffer) return nullptr;
    jsize len = env->GetArrayLength(buffer);
    jbyte *data = env->GetByteArrayElements(buffer, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to load image from buffer: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(buffer, data, JNI_ABORT);
        return env->NewStringUTF("{\"error\": \"Failed to load image from buffer\"}");
    }
    
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    VipsBandFormat format = vips_image_get_format(image);
    VipsInterpretation interpretation = vips_image_get_interpretation(image);
    
    size_t size_estimate = (size_t)width * height * bands * vips_format_sizeof(format);
    
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
        vips_enum_nick(VIPS_TYPE_BAND_FORMAT, format),
        vips_enum_nick(VIPS_TYPE_INTERPRETATION, interpretation),
        size_estimate
    );
    
    g_object_unref(image);
    env->ReleaseByteArrayElements(buffer, data, JNI_ABORT);
    return env->NewStringUTF(info);
}

// ============================================================================
// Raw Buffer / ByteArray Compressor & Conversion Operations
// ============================================================================

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressJpeg(JNIEnv *env, jclass clazz, jbyteArray input, jint quality) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    jbyte *data = env->GetByteArrayElements(input, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to parse source buffer for JPEG compression: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_jpegsave_buffer(image, &outBuf, &outLen, "Q", quality, nullptr) != 0) {
        LOGE("Failed to encode JPEG: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    
    g_free(outBuf);
    g_object_unref(image);
    env->ReleaseByteArrayElements(input, data, JNI_ABORT);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressWebp(JNIEnv *env, jclass clazz, jbyteArray input, jint quality) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    jbyte *data = env->GetByteArrayElements(input, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to parse source buffer for WebP compression: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_webpsave_buffer(image, &outBuf, &outLen, "Q", quality, nullptr) != 0) {
        LOGE("Failed to encode WebP: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    
    g_free(outBuf);
    g_object_unref(image);
    env->ReleaseByteArrayElements(input, data, JNI_ABORT);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_compressPng(JNIEnv *env, jclass clazz, jbyteArray input, jint compression) {
    if (!input) return nullptr;
    jsize len = env->GetArrayLength(input);
    jbyte *data = env->GetByteArrayElements(input, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to parse source buffer for PNG compression: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    if (vips_pngsave_buffer(image, &outBuf, &outLen, "compression", compression, nullptr) != 0) {
        LOGE("Failed to encode PNG: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(outLen);
    env->SetByteArrayRegion(result, 0, outLen, (jbyte*)outBuf);
    
    g_free(outBuf);
    g_object_unref(image);
    env->ReleaseByteArrayElements(input, data, JNI_ABORT);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_convertFormat(JNIEnv *env, jclass clazz, jbyteArray input, jstring format) {
    if (!input || !format) return nullptr;
    const char *formatStr = env->GetStringUTFChars(format, nullptr);
    jsize len = env->GetArrayLength(input);
    jbyte *data = env->GetByteArrayElements(input, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to parse source buffer: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        env->ReleaseStringUTFChars(format, formatStr);
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    int result = -1;
    
    if (strcasecmp(formatStr, "jpeg") == 0 || strcasecmp(formatStr, "jpg") == 0) {
        result = vips_jpegsave_buffer(image, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "png") == 0) {
        result = vips_pngsave_buffer(image, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "webp") == 0) {
        result = vips_webpsave_buffer(image, &outBuf, &outLen, nullptr);
    } else {
        LOGE("Unsupported format for convert: %s", formatStr);
    }
    
    if (result != 0) {
        LOGE("Failed to convert image to %s. Error: %s", formatStr, vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        env->ReleaseStringUTFChars(format, formatStr);
        return nullptr;
    }
    
    jbyteArray output = env->NewByteArray(outLen);
    env->SetByteArrayRegion(output, 0, outLen, (jbyte*)outBuf);
    
    g_free(outBuf);
    g_object_unref(image);
    env->ReleaseByteArrayElements(input, data, JNI_ABORT);
    env->ReleaseStringUTFChars(format, formatStr);
    return output;
}

JNIEXPORT jbyteArray JNICALL
Java_io_github_anaruto_vips_VipsNative_resize(JNIEnv *env, jclass clazz, jbyteArray input, jdouble scale, jstring outputFormat) {
    if (!input || !outputFormat) return nullptr;
    const char *formatStr = env->GetStringUTFChars(outputFormat, nullptr);
    jsize len = env->GetArrayLength(input);
    jbyte *data = env->GetByteArrayElements(input, nullptr);
    
    VipsImage *image = vips_image_new_from_buffer(data, len, "", nullptr);
    if (!image) {
        LOGE("Failed to parse image for resizing: %s", vips_error_buffer());
        vips_error_clear();
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        env->ReleaseStringUTFChars(outputFormat, formatStr);
        return nullptr;
    }
    
    VipsImage *resized = nullptr;
    if (vips_resize(image, &resized, scale, nullptr) != 0) {
        LOGE("Failed to resize image. Error: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        env->ReleaseStringUTFChars(outputFormat, formatStr);
        return nullptr;
    }
    
    void *outBuf = nullptr;
    size_t outLen = 0;
    int result = -1;
    
    if (strcasecmp(formatStr, "jpeg") == 0 || strcasecmp(formatStr, "jpg") == 0) {
        result = vips_jpegsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "png") == 0) {
        result = vips_pngsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else if (strcasecmp(formatStr, "webp") == 0) {
        result = vips_webpsave_buffer(resized, &outBuf, &outLen, nullptr);
    } else {
        LOGE("Unsupported format for resized image output: %s", formatStr);
    }
    
    if (result != 0) {
        LOGE("Failed to save resized image. Error: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(resized);
        g_object_unref(image);
        env->ReleaseByteArrayElements(input, data, JNI_ABORT);
        env->ReleaseStringUTFChars(outputFormat, formatStr);
        return nullptr;
    }
    
    jbyteArray output = env->NewByteArray(outLen);
    env->SetByteArrayRegion(output, 0, outLen, (jbyte*)outBuf);
    
    g_free(outBuf);
    g_object_unref(resized);
    g_object_unref(image);
    env->ReleaseByteArrayElements(input, data, JNI_ABORT);
    env->ReleaseStringUTFChars(outputFormat, formatStr);
    return output;
}

// ============================================================================
// Android Bitmap Pixel Processing Operations (Optimized for Android 12+)
// ============================================================================

JNIEXPORT jboolean JNICALL
Java_io_github_anaruto_vips_VipsNative_resizeBitmap(JNIEnv *env, jclass clazz, jobject srcBitmap, jobject dstBitmap) {
    if (!srcBitmap || !dstBitmap) return JNI_FALSE;

    AndroidBitmapInfo srcInfo;
    void* srcPixels = nullptr;
    if (AndroidBitmap_getInfo(env, srcBitmap, &srcInfo) < 0 || AndroidBitmap_lockPixels(env, srcBitmap, &srcPixels) < 0) {
        LOGE("Failed to lock source Bitmap pixels");
        return JNI_FALSE;
    }

    AndroidBitmapInfo dstInfo;
    void* dstPixels = nullptr;
    if (AndroidBitmap_getInfo(env, dstBitmap, &dstInfo) < 0 || AndroidBitmap_lockPixels(env, dstBitmap, &dstPixels) < 0) {
        LOGE("Failed to lock destination Bitmap pixels");
        AndroidBitmap_unlockPixels(env, srcBitmap);
        return JNI_FALSE;
    }

    if (srcInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888 || dstInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Error: Only ARGB_8888 format is supported for Bitmaps");
        AndroidBitmap_unlockPixels(env, srcBitmap);
        AndroidBitmap_unlockPixels(env, dstBitmap);
        return JNI_FALSE;
    }

    // Create a VipsImage wrapper around the source Bitmap's raw memory address (zero-copy creation)
    VipsImage *image = vips_image_new_from_memory(
        srcPixels,
        (size_t)srcInfo.width * srcInfo.height * 4,
        srcInfo.width,
        srcInfo.height,
        4,
        VIPS_BAND_FORMAT_UCHAR
    );
    if (!image) {
        LOGE("Failed to construct VipsImage wrapper from Bitmap pixels: %s", vips_error_buffer());
        vips_error_clear();
        AndroidBitmap_unlockPixels(env, srcBitmap);
        AndroidBitmap_unlockPixels(env, dstBitmap);
        return JNI_FALSE;
    }

    // Set the interpretation to sRGB so filters are correctly calculated
    vips_image_set_interpretation(image, VIPS_INTERPRETATION_sRGB);

    // Calculate asymmetric scaling ratios
    double scale_x = (double)dstInfo.width / srcInfo.width;
    double scale_y = (double)dstInfo.height / srcInfo.height;
    double vscale = scale_y / scale_x;

    // Resize using Lanczos-3 (default vips_resize kernel).
    // Specifying "vscale" allows asymmetric resizing (stretching) if target aspect ratio is different.
    VipsImage *resized = nullptr;
    if (vips_resize(image, &resized, scale_x, "vscale", vscale, nullptr) != 0) {
        LOGE("Vips: Failed to execute resize operation: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(image);
        AndroidBitmap_unlockPixels(env, srcBitmap);
        AndroidBitmap_unlockPixels(env, dstBitmap);
        return JNI_FALSE;
    }

    // Ensure output has exactly 4 channels (RGBA) matching ARGB_8888 target format
    VipsImage *output = nullptr;
    if (vips_image_get_bands(resized) == 3) {
        // If image channels got flattened to RGB (3 channels), attach solid Alpha channel (value 255)
        VipsImage *alpha = nullptr;
        double alpha_val = 255.0;
        if (vips_black(&alpha, resized->Xsize, resized->Ysize, "bands", 1, nullptr) == 0 &&
            vips_linear1(alpha, &alpha, 1.0, alpha_val, nullptr) == 0) {
            vips_bandjoin2(resized, alpha, &output, nullptr);
        }
        if (alpha) g_object_unref(alpha);
    } else {
        output = resized;
        g_object_ref(output);
    }

    if (!output) {
        // Fallback
        output = resized;
        g_object_ref(output);
    }

    // Render/Write output image pixels directly to a newly allocated memory buffer
    size_t written_size = 0;
    void *out_pixels = vips_image_write_to_memory(output, &written_size);
    if (!out_pixels) {
        LOGE("Vips: Failed to write output pixels memory buffer: %s", vips_error_buffer());
        vips_error_clear();
        g_object_unref(output);
        g_object_unref(resized);
        g_object_unref(image);
        AndroidBitmap_unlockPixels(env, srcBitmap);
        AndroidBitmap_unlockPixels(env, dstBitmap);
        return JNI_FALSE;
    }

    // Safely copy the rendered pixels into the locked destination bitmap
    size_t expected_size = (size_t)dstInfo.width * dstInfo.height * 4;
    size_t copy_size = (written_size < expected_size) ? written_size : expected_size;
    std::memcpy(dstPixels, out_pixels, copy_size);

    // Clean up allocated native resources
    g_free(out_pixels);
    g_object_unref(output);
    g_object_unref(resized);
    g_object_unref(image);

    // Unlock pixels
    AndroidBitmap_unlockPixels(env, srcBitmap);
    AndroidBitmap_unlockPixels(env, dstBitmap);
    return JNI_TRUE;
}

} // extern "C"
