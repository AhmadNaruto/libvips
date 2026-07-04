# Panduan Integrasi Binding Android JNI untuk libvips (Teroptimasi)

Folder ini berisi binding JNI (Java Native Interface) yang telah dioptimalkan untuk menggunakan pustaka pengolahan citra berkinerja tinggi **libvips** di dalam aplikasi Android menggunakan Kotlin.

## 🚀 Fitur Utama & Optimasi
1. **Direct NDK Bitmap Resizing**: Mengubah ukuran `Bitmap` Android (`Config.ARGB_8888`) secara langsung di level native C++ (zero-copy creation) menggunakan API `AndroidBitmap_lockPixels`.
2. **Format Transcoding & Compression**: Fungsi in-memory untuk membaca info gambar, kompresi JPEG, WebP, PNG, dan konversi format langsung dari data byte array.
3. **Validasi Input yang Kuat**: Menghindari *Crash Native* (seperti Segmentation Fault) dengan memvalidasi status bitmap, kesesuaian dimensi, status inisialisasi, dan rentang kompresi langsung pada layer Kotlin.
4. **Sesuai Android 12+**: Dioptimalkan untuk modern Android (API 31+), kompatibel dengan Kotlin 2.2.x, Gradle 8.x+, dan Scoped Storage.

---

## 1. Struktur File Binding

Lacak letak berkas-berkas berikut di folder ini:
- **Kotlin Source:**
  - `src/main/kotlin/io/github/anaruto/vips/Vips.kt` (Wrapper tingkat tinggi yang ramah developer dengan validasi input)
  - `src/main/kotlin/io/github/anaruto/vips/VipsNative.kt` (Wrapper pemanggilan JNI native & pemuatan pustaka)
- **C++ JNI Source:**
  - `src/main/cpp/vips_jni.cpp` (Kode jembatan JNI C++ yang mengimplementasikan pemrosesan pixel Bitmap dan pemanggilan libvips)
  - `src/main/cpp/CMakeLists.txt` (Konfigurasi kompilasi CMake)
- **Unit Test Source:**
  - `src/androidTest/kotlin/io/github/anaruto/vips/VipsTest.kt` (Unit test instrumen untuk verifikasi fungsi)

---

## 2. Cara Integrasi ke Proyek Android Studio

Ikuti langkah-langkah di bawah ini untuk mengintegrasikan binding ke dalam proyek Android Anda:

### Langkah A: Salin Berkas Kotlin
Salin folder `io/github/anaruto/vips` beserta isinya (`Vips.kt` dan `VipsNative.kt`) ke dalam direktori kode sumber Kotlin proyek Anda, misalnya:
`app/src/main/java/io/github/anaruto/vips/` atau `app/src/main/kotlin/io/github/anaruto/vips/`

### Langkah B: Salin Berkas C++
1. Salin berkas `vips_jni.cpp` dan `CMakeLists.txt` ke folder C++ proyek Anda, biasanya di:
   `app/src/main/cpp/`
2. Salin folder header/include libvips dan glib ke:
   `app/src/main/include/`
3. Salin pustaka prebuilt `.so` (seperti `libvips.so`, `libglib-2.0.so`, `libgio-2.0.so`, `libintl.so`, `libz.so`, dll.) untuk setiap arsitektur (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`) ke folder:
   `app/src/main/jniLibs/`

### Langkah C: Konfigurasi `app/build.gradle.kts` (Kotlin DSL - Kotlin 2.2.x)
Pastikan proyek Anda menggunakan plugin Kotlin versi `2.2.x` dan konfigurasikan `app/build.gradle.kts` Anda seperti berikut:

```kotlin
plugins {
    alias(libs.plugins.android.application) // Versi AGP 8.10.0+
    kotlin("android") version "2.2.10"       // Menggunakan Kotlin 2.2.x
}

android {
    namespace = "io.github.anaruto.vips"
    compileSdk = 34

    defaultConfig {
        minSdk = 21
        targetSdk = 34

        externalNativeBuild {
            cmake {
                cppFlags("-std=c++11")
                // Tambahkan argumen yang diperlukan untuk libvips jika ada
            }
        }
        
        ndk {
            abiFilters.addAll(setOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64"))
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // Konfigurasi opsi compiler Kotlin 2.2.x menggunakan compilerOptions DSL baru
    kotlin {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
            freeCompilerArgs.addAll("-Xjsr305=strict")
        }
    }
}
```

---

## 3. Contoh Cara Penggunaan di Kotlin

### A. Inisialisasi dan Shutdown (Lifecycle)

libvips harus diinisialisasi sekali sebelum digunakan (biasanya di kelas `Application` Anda) dan ditutup saat aplikasi akan keluar.

```kotlin
import android.app.Application
import io.github.anaruto.vips.Vips

class MyApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // Menginisialisasi runtime libvips secara aman
        val success = Vips.init()
        if (!success) {
            android.util.Log.e("MyApplication", "Gagal menginisialisasi libvips!")
        }
    }

    override fun onTerminate() {
        super.onTerminate()
        // Membebaskan sumber daya native libvips
        Vips.shutdown()
    }
}
```

#### B. Pengaturan Concurrency & Cache (Opsional)
Untuk perangkat mobile yang memiliki keterbatasan memori, Anda dapat menyesuaikan batas memori cache dan jumlah thread worker agar aplikasi berjalan optimal tanpa risiko kehabisan memori (OOM).

```kotlin
// Membatasi memori cache libvips menjadi 30MB (default 50MB)
Vips.setCacheMaxMem(30 * 1024 * 1024L)

// Membatasi cache maksimal 30 operasi gambar
Vips.setCacheMax(30)

// Membatasi jumlah thread paralel (misalnya 4 thread)
Vips.setConcurrency(4)
```

### C. Mengubah Ukuran Bitmap (Direct Resizing)

#### 1. Resizing dengan Menentukan Bitmap Tujuan (Asymmetric Scaling / Stretch)
Mengubah ukuran gambar secara efisien langsung di level native C++ menggunakan buffer pixel terikat.

```kotlin
import android.graphics.Bitmap
import io.github.anaruto.vips.Vips

fun resizeBitmapDirectly(source: Bitmap): Bitmap {
    val targetWidth = 1280
    val targetHeight = 720
    val destBitmap = Bitmap.createBitmap(targetWidth, targetHeight, Bitmap.Config.ARGB_8888)

    // Mengubah ukuran pixel source langsung ke destBitmap
    val success = Vips.resizeBitmap(source, destBitmap)
    if (!success) {
        destBitmap.recycle()
        throw RuntimeException("Resizing gagal!")
    }
    
    return destBitmap
}
```

#### 2. Resizing Mengikuti Aspek Rasio (Width-Only)
Masukkan target lebar saja, tinggi gambar akan secara otomatis dihitung untuk menjaga aspek rasio citra asli.

```kotlin
import android.graphics.Bitmap
import io.github.anaruto.vips.Vips

fun resizeKeepAspectRatio(source: Bitmap, targetWidth: Int): Bitmap {
    // libvips akan menghitung tinggi dan menghasilkan Bitmap baru yang disesuaikan
    return Vips.resizeBitmap(source, targetWidth)
}
```

### C. Kompresi & Transcoding In-Memory (Byte Array)

Untuk mengirim gambar melalui API/Network tanpa menyimpannya ke storage internal sebagai file temporer:

```kotlin
import io.github.anaruto.vips.Vips

fun processRawBytes(inputBytes: ByteArray): ByteArray {
    // 1. Dapatkan informasi gambar
    val infoJson = Vips.getImageInfo(inputBytes)
    println("Info Gambar: $infoJson")

    // 2. Ubah ukuran encoded byte array (skala 0.5 = setengah dimensi asli)
    val resizedBytes = Vips.resize(inputBytes, scale = 0.5, outputFormat = "webp")
        ?: throw RuntimeException("Resizing gagal")

    // 3. Kompresi ke format WebP dengan kualitas 80
    val compressedBytes = Vips.compressWebp(resizedBytes, quality = 80)
        ?: throw RuntimeException("Kompresi gagal")

    return compressedBytes
}
```

---

## 4. Cara Pengujian (Testing)

Untuk memastikan integrasi JNI dan wrapper Kotlin berjalan dengan baik di arsitektur target, jalankan pengujian terintegrasi (*Instrumented Unit Test*):

1. Salin berkas **`src/androidTest/kotlin/io/github/anaruto/vips/VipsTest.kt`** ke folder pengujian instrumen di proyek Android Studio Anda, misalnya:
   `app/src/androidTest/java/io/github/anaruto/vips/`
2. Hubungkan perangkat Android fisik atau jalankan emulator Android.
3. Di Android Studio, klik kanan berkas `VipsTest.kt` di panel proyek, lalu pilih **Run 'VipsTest'**.
4. Pengujian ini akan memverifikasi fungsi inisialisasi, pembacaan metadata JSON, resizing bitmap ARGB_8888, penyesuaian aspek rasio, serta validasi memori aman.
