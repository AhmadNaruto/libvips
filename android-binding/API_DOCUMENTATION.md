# Dokumentasi API & Contoh Penggunaan Binding JNI libvips untuk Android 12+

Dokumen ini menyediakan referensi lengkap mengenai antarmuka pemrograman aplikasi (API) dari wrapper Kotlin **libvips** beserta contoh implementasi praktis untuk Android 12 (API 31/32) dan Android 13+ (API 33+).

---

## 1. Daftar API `Vips`

Objek `io.github.anaruto.vips.Vips` adalah antarmuka utama yang digunakan oleh developer untuk memanggil seluruh fungsi pemrosesan gambar libvips.

### A. Lifecycle & Informasi Runtime

| Nama Fungsi & Signature | Tipe Return | Deskripsi |
| :--- | :--- | :--- |
| `init()` | `Boolean` | Menginisialisasi lingkungan runtime libvips. Wajib dipanggil sebelum fungsi gambar lainnya digunakan. |
| `isInitialized()` | `Boolean` | Memeriksa apakah libvips telah diinisialisasi dengan aman. |
| `getVersion()` | `String` | Mengembalikan string versi libvips yang sedang berjalan (contoh: `"8.15.2"`). |
| `shutdown()` | `Unit` | Mematikan runtime libvips, menghentikan seluruh threadpool worker, dan membebaskan sisa cache native. |

---

### B. Pemrosesan Bitmap (Direct Pixel Resizing)

> [!IMPORTANT]
> Seluruh fungsi pemrosesan Bitmap hanya mendukung format **`Bitmap.Config.ARGB_8888`**. Pastikan Bitmap yang dikirimkan belum didaur ulang (`!isRecycled`).

#### `resizeBitmap(src: Bitmap, dst: Bitmap): Boolean`
Mengubah ukuran gambar dari `src` langsung ke `dst` menggunakan memori pixel terikat (zero-copy). Fungsi ini mendukung penskalaan asimetris (*stretch*) jika aspek rasio `src` dan `dst` berbeda.
- **`src`**: `Bitmap` sumber (ARGB_8888).
- **`dst`**: `Bitmap` target (ARGB_8888).
- **Return**: `Boolean` (True jika berhasil, False jika gagal).

#### `resizeBitmap(src: Bitmap, targetWidth: Int): Bitmap`
Mengubah ukuran `Bitmap` dengan hanya menetapkan lebar target. Tinggi gambar akan otomatis dihitung agar menjaga aspek rasio asli. Fungsi ini secara instan mengembalikan objek `Bitmap` baru yang siap digunakan.
- **`src`**: `Bitmap` sumber (ARGB_8888).
- **`targetWidth`**: Lebar target dalam piksel (`> 0`).
- **Return**: `Bitmap` baru hasil skala (ARGB_8888).

---

### C. Kompresi & Transcoding (In-Memory Byte Array)

Semua fungsi di bawah ini menerima input berupa representasi biner gambar terenkripsi (JPEG, PNG, WebP) dan mengembalikan format hasil baru sebagai *Byte Array*.

#### `compressJpeg(input: ByteArray, quality: Int = 85): ByteArray?`
Mengonversi dan mengompresi gambar menjadi format JPEG.
- **`quality`**: Kualitas gambar (1 hingga 100). Default: `85`.

#### `compressWebp(input: ByteArray, quality: Int = 85): ByteArray?`
Mengonversi dan mengompresi gambar menjadi format WebP.
- **`quality`**: Kualitas gambar (1 hingga 100). Default: `85`.

#### `compressPng(input: ByteArray, compression: Int = 6): ByteArray?`
Mengonversi dan mengompresi gambar menjadi format PNG.
- **`compression`**: Tingkat kompresi Zlib (0 hingga 9). Default: `6`.

#### `convertFormat(input: ByteArray, format: String): ByteArray?`
Mengonversi format gambar secara instan tanpa mengubah resolusi.
- **`format`**: Target format, mendukung `"jpeg"`, `"png"`, dan `"webp"`.

#### `resize(input: ByteArray, scale: Double, outputFormat: String = "jpeg"): ByteArray?`
Mengubah skala citra yang terenkripsi dan langsung mengompresinya ke format keluaran.
- **`scale`**: Faktor skala (misal: `0.5` untuk membagi resolusi menjadi setengah, `2.0` untuk mendobelnya).
- **`outputFormat`**: Format hasil akhir (`"jpeg"`, `"png"`, `"webp"`).

---

### D. Pembacaan Informasi Metadata (Metadata Query)

#### `getImageInfo(path: String): String`
Membaca metadata dimensi dan properti gambar dari *file path* lokal.
- **Return**: String berformat JSON yang berisi: `width`, `height`, `bands`, `format`, `interpretation`, `estimated_size_bytes`.

#### `getImageInfo(buffer: ByteArray): String`
Membaca metadata dimensi dan properti gambar dari sebuah *byte array* terenkripsi di memori.
- **Return**: String berformat JSON yang serupa dengan fungsi di atas.

---

### E. Konfigurasi Thread & Cache (Optimasi Mobile)

Fungsi-fungsi ini sangat berguna di Android untuk membatasi konsumsi sumber daya CPU dan RAM guna mencegah *Application Not Responding* (ANR) atau *Out Of Memory* (OOM).

#### `setConcurrency(concurrency: Int)`
Mengatur jumlah maksimum thread paralel yang digunakan untuk pengolahan gambar. Default: jumlah core CPU.
- **`concurrency`**: Jumlah thread (`>= 1`).

#### `setCacheMaxMem(maxMemBytes: Long)`
Membatasi konsumsi RAM maksimum yang boleh digunakan untuk cache operasi libvips. Default: **50MB** (`52428800` bytes).
- **`maxMemBytes`**: Batas memori dalam byte. Set `0` untuk tanpa batasan.

#### `setCacheMax(maxOperations: Int)`
Membatasi jumlah maksimum operasi gambar yang disimpan dalam cache. Default: `50`.

#### `setCacheMaxFiles(maxFiles: Int)`
Membatasi jumlah maksimum file terbuka yang disimpan dalam cache internal. Default: `10`.

---

## 2. Contoh Implementasi Lengkap di Android

Berikut adalah contoh alur lengkap aplikasi Android yang menginisialisasi libvips, mengambil gambar dari galeri via **Photo Picker** (Android 12/13+), mengompresinya secara in-memory, dan menyimpannya kembali ke galeri publik memakai **MediaStore API**.

### A. Inisialisasi pada `Application`

```kotlin
package com.example.myapp

import android.app.Application
import io.github.anaruto.vips.Vips

class MyApp : Application() {
    override fun onCreate() {
        super.onCreate()
        
        // 1. Inisialisasi runtime libvips
        val initialized = Vips.init()
        if (initialized) {
            // 2. Optimalkan cache secara dinamis untuk perangkat mobile dengan RAM kecil
            Vips.setCacheMaxMem(30 * 1024 * 1024L) // Batasi cache RAM ke 30MB
            Vips.setCacheMax(20)                  // Batasi riwayat cache ke 20 operasi
            Vips.setConcurrency(4)                 // Batasi maksimal 4 thread paralel
        }
    }

    override fun onTerminate() {
        super.onTerminate()
        // Matikan runtime saat aplikasi dihancurkan
        Vips.shutdown()
    }
}
```

### B. Alur Aktivitas (Activity) Pemrosesan Gambar

```kotlin
package com.example.myapp

import android.content.ContentValues
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import android.provider.MediaStore
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.lifecycle.lifecycleScope
import io.github.anaruto.vips.Vips
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.InputStream

class MainActivity : ComponentActivity() {

    // Registrasi Photo Picker (Tidak memerlukan izin galeri runtime di Android 11+)
    private val pickMedia = registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) {
            processAndSaveImage(uri)
        } else {
            Toast.makeText(this, "Tidak ada gambar yang dipilih", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Panggil galeri
        pickMedia.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
    }

    private fun processAndSaveImage(uri: Uri) {
        // Lakukan pemrosesan di Background Thread agar UI tetap lancar
        lifecycleScope.launch(Dispatchers.Default) {
            try {
                // 1. Load Uri ke Bitmap
                val sourceBitmap = loadBitmapFromUri(uri) 
                    ?: throw RuntimeException("Gagal me-decode gambar dari Uri")

                // 2. Gunakan libvips untuk resize dengan menjaga aspek rasio (target lebar: 1080px)
                val resizedBitmap = Vips.resizeBitmap(sourceBitmap, 1080)
                
                // Bebaskan memori bitmap asal
                sourceBitmap.recycle()

                // 3. Simpan resizedBitmap ke galeri publik (Pictures/MyAppImages)
                val isSaved = saveToGallery("resized_image_${System.currentTimeMillis()}", resizedBitmap)
                
                // Bebaskan memori bitmap hasil
                resizedBitmap.recycle()

                withContext(Dispatchers.Main) {
                    if (isSaved) {
                        Toast.makeText(this@MainActivity, "Gambar berhasil di-resize dan disimpan!", Toast.LENGTH_LONG).show()
                    } else {
                        Toast.makeText(this@MainActivity, "Gagal menyimpan gambar", Toast.LENGTH_LONG).show()
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
                withContext(Dispatchers.Main) {
                    Toast.makeText(this@MainActivity, "Error: ${e.message}", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun loadBitmapFromUri(uri: Uri): Bitmap? {
        val options = BitmapFactory.Options().apply {
            inPreferredConfig = Bitmap.Config.ARGB_8888 // Pastikan ARGB_8888 untuk JNI libvips
        }
        return contentResolver.openInputStream(uri)?.use { inputStream ->
            BitmapFactory.decodeStream(inputStream, null, options)
        }
    }

    private fun saveToGallery(displayName: String, bitmap: Bitmap): Boolean {
        val contentValues = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, "$displayName.jpg")
            put(MediaStore.MediaColumns.MIME_TYPE, "image/jpeg")
            put(MediaStore.MediaColumns.RELATIVE_PATH, "Pictures/MyAppImages")
        }

        val resolver = contentResolver
        val imageUri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, contentValues) 
            ?: return false

        return try {
            resolver.openOutputStream(imageUri)?.use { outputStream ->
                // Kompresi akhir JPEG ke file
                bitmap.compress(Bitmap.CompressFormat.JPEG, 90, outputStream)
            }
            true
        } catch (e: Exception) {
            e.printStackTrace()
            resolver.delete(imageUri, null, null) // Rollback baris jika gagal
            false
        }
    }
}
```

### C. Alur Upload API In-Memory (Retrofit Transcoding)

Contoh mengubah ukuran dan format gambar mentah (`ByteArray`) secara in-memory untuk dikirim langsung ke server Web API tanpa menulis berkas temporer ke penyimpanan fisik:

```kotlin
package com.example.myapp

import io.github.anaruto.vips.Vips
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.MultipartBody
import okhttp3.RequestBody.Companion.toRequestBody

suspend fun uploadResizedImage(rawImageBytes: ByteArray) {
    withContext(Dispatchers.Default) {
        // 1. Dapatkan metadata JSON gambar asal
        val metadata = Vips.getImageInfo(rawImageBytes)
        println("Metadata asli: $metadata")

        // 2. Resize skala 0.5 (setengah dari resolusi asli) dan transcode ke WebP secara in-memory
        val webpBytes = Vips.resize(rawImageBytes, scale = 0.5, outputFormat = "webp")
            ?: throw RuntimeException("Gagal memproses gambar")

        // 3. Persiapkan request Multipart Retrofit/OkHttp
        val requestFile = webpBytes.toRequestBody("image/webp".toMediaTypeOrNull(), 0, webpBytes.size)
        val body = MultipartBody.Part.createFormData("image", "avatar.webp", requestFile)

        // 4. Kirim ke API service ...
        // apiService.uploadAvatar(body)
    }
}
```

---

## 3. Detail Optimasi & Arsitektur JNI (Terbaru)

Binding ini dikonfigurasi untuk memenuhi tuntutan kinerja ekstrim dan efisiensi memori tinggi di Android dengan optimasi berikut:

### A. Dynamic Loading Efisien
Memuat library native secara otomatis menggunakan satu panggilan `System.loadLibrary("vips_jni")`. Sistem dynamic linker Android 6.0+ (API 23+) akan otomatis memetakan relasi dependensi rekursif (seperti `libvips.so`, GLib, dsb.) secara efisien dari metadata ELF header.

### B. Adaptive Concurrency (Penjadwalan Thread Pintar)
Concurrency threads disesuaikan secara dinamis untuk CPU mobile berbasis arsitektur heterogeneous *big.LITTLE*:
- Jika core `>= 8`, threads diset ke `cores / 2` (biasanya hanya menggunakan core berkinerja tinggi).
- Jika core `4 - 7`, threads diset ke `cores - 1` (menyisakan core untuk kelancaran UI thread).
- Menghindari thermal throttling dan menjaga frame rate aplikasi tetap tinggi.

### C. Pemrosesan Bitmap Zero-Leak (RAII & Zero-Copy)
- **Zero-Copy input**: Menggunakan memory address bitmap asal (`AndroidBitmap_lockPixels`) secara langsung tanpa membuat buffer temporer.
- **Garbage-Free C++ RAII**: Seluruh referensi gambar native (`VipsImagePtr`), penguncian bitmap (`BitmapPixelLock`), JNI strings, dan byte arrays dibungkus dengan C++ RAII. Memory dilepas secara otomatis saat keluar dari scope eksekusi fungsi native, menjamin nol kebocoran memori.
- **Normalisasi RGBA**: [resizeBitmap](file:///data/data/com.termux/files/home/libvips/android-binding/src/main/kotlin/io/github/anaruto/vips/Vips.kt#L135-L145) secara otomatis melakukan konversi interpretations sRGB, alpha premultiplication (mencegah halo gelap di area transparan), dan konversi format output ke `UCHAR` 4-channels (RGBA) yang kompatibel penuh dengan `ARGB_8888`.

### D. Optimasi Kompiler Tingkat Tinggi (CMake)
Kompilasi native menggunakan flags `-O3` dan `-ffast-math` untuk optimasi instruksi ARM NEON, `-fno-exceptions` dan `-fno-rtti` untuk mereduksi ukuran file binary `.so`, serta mengaktifkan *Link-Time Optimization (LTO)* untuk analisis kode lintas modul.

### E. Kotlin GC & Lock-Free Optimization
- Validasi format input menggunakan cache statis Set `SUPPORTED_FORMATS` untuk mereduksi sampah objek temporer di Java Virtual Machine.
- Pengecekan inisialisasi thread-safe dan lock-free menggunakan status thread visibility volatile `AtomicBoolean` pada Kotlin wrapper.

