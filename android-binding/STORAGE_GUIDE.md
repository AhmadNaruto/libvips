# Panduan Akses File (Membaca/Menulis) di Android 12+ & Android 13+

Dokumen ini menjelaskan kesiapan pustaka binding JNI **libvips** terhadap kebijakan penyimpanan Android modern, serta memberikan panduan praktis bagi developer untuk membaca dan menulis file citra di Android 12 (API 31/32) dan Android 13+ (API 33+).

---

## 1. Kesiapan Pustaka JNI libvips

> [!TIP]
> **Kompatibilitas 100%**: Pustaka JNI libvips ini **100% siap dan kompatibel** dengan Android 12+ tanpa modifikasi kode native. 

Mengapa?
- Pustaka JNI ini **tidak melakukan operasi I/O file secara langsung** di tingkat C++.
- Proses penskalaan sepenuhnya dilakukan **in-memory** menggunakan objek `Bitmap` Android, `ByteArray` (buffer encoded), atau jalur file yang Anda tentukan.
- Pustaka ini tidak meminta izin penyimpanan (*storage permissions*) apa pun secara implisit. Tanggung jawab membaca gambar dari media penyimpanan ke dalam memory (serta menulis hasil kembali ke penyimpanan) berada di tingkat aplikasi (Kotlin/Java).

---

## 2. Kebijakan Penyimpanan Android Modern (Android 12 & 13+)

Sejak Android 11 (API 30), Google memberlakukan **Scoped Storage** secara ketat. Pada Android 12 dan 13+, perilakunya adalah sebagai berikut:

| Target Versi Android | Izin Membaca File Media | Izin Menulis File Media | Akses Direktori Khusus Aplikasi (Cache/Files) |
| :--- | :--- | :--- | :--- |
| **Android 12 (API 31/32)** | Memerlukan `READ_EXTERNAL_STORAGE` (untuk galeri publik). | `WRITE_EXTERNAL_STORAGE` diabaikan/tidak diperlukan jika menggunakan MediaStore untuk menyimpan ke folder Publik. | **Bebas Tanpa Izin** (menggunakan File API biasa). |
| **Android 13+ (API 33+)** | Memerlukan `READ_MEDIA_IMAGES` (bukan `READ_EXTERNAL_STORAGE` lagi). | Tidak memerlukan izin untuk menulis/menyimpan melalui MediaStore. | **Bebas Tanpa Izin** (menggunakan File API biasa). |

---

## 3. Praktik Terbaik Membaca/Menulis File untuk libvips di Android 12+

Berikut adalah beberapa cara yang direkomendasikan untuk memuat gambar ke `Bitmap` atau `ByteArray` dan menyimpannya kembali:

### A. Membaca/Menulis di Penyimpanan Khusus Aplikasi (App-Specific Storage)
Jika Anda hanya perlu menyimpan gambar sementara (cache) atau file privat yang tidak perlu muncul di Galeri:
- **Lokasi**: `context.cacheDir` atau `context.filesDir` atau `context.getExternalFilesDir(null)`.
- **Izin**: **Tidak memerlukan izin runtime apa pun.**
- **Metode**: Gunakan File API Java/Kotlin standar atau `BitmapFactory`.

```kotlin
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import java.io.File
import java.io.FileOutputStream

// 1. Membaca Bitmap dari Cache
fun loadFromCache(context: Context, fileName: String): Bitmap? {
    val file = File(context.cacheDir, fileName)
    return if (file.exists()) {
        BitmapFactory.decodeFile(file.absolutePath)
    } else null
}

// 2. Menulis Bitmap ke Cache
fun saveToCache(context: Context, bitmap: Bitmap, fileName: String) {
    val file = File(context.cacheDir, fileName)
    FileOutputStream(file).use { out ->
        bitmap.compress(Bitmap.CompressFormat.JPEG, 90, out)
    }
}
```

---

### B. Membaca File dari Galeri (Menggunakan Photo Picker)
**Photo Picker** adalah cara standar yang direkomendasikan oleh Google untuk Android 13+ (dan telah di-backport ke Android 11/12 melalui Google Play Services).
- **Keuntungan**: **Tidak memerlukan izin runtime apa pun** (`READ_MEDIA_IMAGES` atau `READ_EXTERNAL_STORAGE` tidak perlu diminta). Pengguna hanya memilih gambar secara eksplisit melalui sistem UI.

```kotlin
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.result.PickVisualMediaRequest
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri

class MyActivity : ComponentActivity() {

    // Registrasi Photo Picker launcher
    private val pickMedia = registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) {
            val bitmap = loadBitmapFromUri(uri)
            if (bitmap != null) {
                // Panggil libvips untuk resize
                val resized = io.github.anaruto.vips.Vips.resizeBitmap(bitmap, 800)
                // Gunakan resized bitmap ...
            }
        }
    }

    fun openGallery() {
        // Meluncurkan pemilih gambar saja
        pickMedia.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
    }

    private fun loadBitmapFromUri(uri: Uri): Bitmap? {
        return contentResolver.openInputStream(uri)?.use { inputStream ->
            BitmapFactory.decodeStream(inputStream)
        }
    }
}
```

---

### C. Menulis/Menyimpan Hasil Resize ke Galeri Publik (MediaStore API)
Untuk menyimpan gambar hasil resize ke folder publik seperti `/Pictures` atau `/DCIM` agar terbaca oleh Galeri sistem, gunakan **MediaStore API**.
- **Izin**: **Tidak memerlukan izin runtime apa pun** (baik di Android 12 maupun Android 13+), karena aplikasi Anda adalah pemilik sah dari baris media baru tersebut.

```kotlin
import android.content.ContentValues
import android.content.Context
import android.graphics.Bitmap
import android.provider.MediaStore

fun saveResizedImageToGallery(context: Context, resizedBitmap: Bitmap, displayName: String): Boolean {
    val contentValues = ContentValues().apply {
        put(MediaStore.MediaColumns.DISPLAY_NAME, "$displayName.jpg")
        put(MediaStore.MediaColumns.MIME_TYPE, "image/jpeg")
        // Di Android 10+, berkas akan disimpan otomatis ke folder Pictures/
        put(MediaStore.MediaColumns.RELATIVE_PATH, "Pictures/MyResizedImages")
    }

    val resolver = context.contentResolver
    val imageUri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, contentValues)
        ?: return false

    return try {
        resolver.openOutputStream(imageUri)?.use { outStream ->
            resizedBitmap.compress(Bitmap.CompressFormat.JPEG, 95, outStream)
        }
        true
    } catch (e: Exception) {
        e.printStackTrace()
        // Hapus baris kosong jika gagal menyimpan
        resolver.delete(imageUri, null, null)
        false
    }
}
```

---

### D. Membaca Media Melalui Query Custom (Perlu Izin Runtime)
Jika aplikasi Anda adalah aplikasi pengedit foto atau manajer file kustom yang perlu menelusuri seluruh galeri pengguna secara langsung tanpa menggunakan UI Photo Picker:
- Anda harus meminta izin runtime berdasarkan versi Android perangkat:

```kotlin
import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat
import androidx.activity.ComponentActivity

fun checkStoragePermission(activity: ComponentActivity): Boolean {
    val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        // Android 13+ (API 33+)
        Manifest.permission.READ_MEDIA_IMAGES
    } else {
        // Android 12 ke bawah
        Manifest.permission.READ_EXTERNAL_STORAGE
    }

    return ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED
}
```

> [!WARNING]
> Izin `WRITE_EXTERNAL_STORAGE` tidak memiliki efek apa pun di Android 13+ dan sebaiknya tidak diminta pada versi Android tersebut. Menyimpan file ke galeri umum (Pictures, Downloads) tetap dapat dilakukan melalui `MediaStore` tanpa memerlukan izin ini.
