package io.github.anaruto.vips

import android.graphics.Bitmap
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Instrumented Unit Test for libvips Android JNI Bindings.
 *
 * Place this file in your Android project at:
 * `app/src/androidTest/java/io/github/anaruto/vips/VipsTest.kt`
 *
 * To run this test, connect an Android device/emulator, right-click this file in 
 * Android Studio, and select "Run 'VipsTest'".
 */
@RunWith(AndroidJUnit4::class)
class VipsTest {

    @Before
    fun setUp() {
        // Initialize the library before running each test
        Vips.init()
    }

    @After
    fun tearDown() {
        // Shut down the runtime resources
        Vips.shutdown()
    }

    @Test
    fun testVipsInitialization() {
        assertTrue("libvips should be initialized", Vips.isInitialized())
        val version = Vips.getVersion()
        assertNotNull("Version string should not be null", version)
        assertTrue("Version string should not be empty", version.isNotEmpty())
    }

    @Test
    fun testBitmapResizing() {
        // Create 100x100 source bitmap filled with pure Green color
        val srcBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        srcBitmap.eraseColor(Color.GREEN)

        // Create 50x50 destination bitmap
        val dstBitmap = Bitmap.createBitmap(50, 50, Bitmap.Config.ARGB_8888)

        // Resize the bitmap
        val success = Vips.resizeBitmap(srcBitmap, dstBitmap)
        assertTrue("Bitmap resize should return true on success", success)

        // Verify the destination pixels are correctly filled with Green color
        assertEquals(Color.GREEN, dstBitmap.getPixel(0, 0))
        assertEquals(Color.GREEN, dstBitmap.getPixel(25, 25))
        assertEquals(Color.GREEN, dstBitmap.getPixel(49, 49))

        srcBitmap.recycle()
        dstBitmap.recycle()
    }

    @Test
    fun testBitmapResizingWithTargetWidthOnly() {
        // Create a 100x200 source bitmap filled with pure Blue color (aspect ratio = 2.0)
        val srcBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888)
        srcBitmap.eraseColor(Color.BLUE)

        val targetWidth = 50
        val expectedHeight = 100 // 50 * (200/100)

        // Perform resize with auto calculated height
        val dstBitmap = Vips.resizeBitmap(srcBitmap, targetWidth)
        assertNotNull("Resized destination bitmap should be created", dstBitmap)
        assertEquals("Resized width should equal target width", targetWidth, dstBitmap.width)
        assertEquals("Resized height should match aspect ratio", expectedHeight, dstBitmap.height)
        
        // Verify output pixels
        assertEquals(Color.BLUE, dstBitmap.getPixel(0, 0))
        assertEquals(Color.BLUE, dstBitmap.getPixel(25, 50))
        assertEquals(Color.BLUE, dstBitmap.getPixel(49, 99))

        srcBitmap.recycle()
        dstBitmap.recycle()
    }

    @Test
    fun testSafeValidationRecycledBitmap() {
        val srcBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888)
        val dstBitmap = Bitmap.createBitmap(5, 5, Bitmap.Config.ARGB_8888)

        srcBitmap.recycle() // Recycle to trigger validation error

        try {
            Vips.resizeBitmap(srcBitmap, dstBitmap)
            fail("Should have thrown IllegalArgumentException due to recycled source bitmap")
        } catch (e: IllegalArgumentException) {
            assertNotNull(e.message)
            assertTrue(e.message!!.contains("recycled", ignoreCase = true))
        }

        dstBitmap.recycle()
    }

    @Test
    fun testMetadataParsing() {
        // Create an encoded jpeg buffer manually to feed into the metadata parser.
        // We'll create a small bitmap, compress it to JPEG using standard Android API,
        // and then pass its byte array to libvips.
        val bitmap = Bitmap.createBitmap(32, 32, Bitmap.Config.ARGB_8888)
        bitmap.eraseColor(Color.RED)
        
        val stream = java.io.ByteArrayOutputStream()
        bitmap.compress(Bitmap.CompressFormat.JPEG, 90, stream)
        val jpegBytes = stream.toByteArray()
        bitmap.recycle()

        // Query image info using libvips JNI
        val infoJson = Vips.getImageInfo(jpegBytes)
        assertNotNull("JSON info string should not be null", infoJson)
        assertTrue("JSON should contain width", infoJson.contains("\"width\": 32"))
        assertTrue("JSON should contain height", infoJson.contains("\"height\": 32"))
    }

    @Test
    fun testCacheAndConcurrencyConfiguration() {
        // Assert setting concurrency, cache memory, cache operations and cache files limits works without exceptions
        try {
            Vips.setConcurrency(4)
            Vips.setCacheMax(10)
            Vips.setCacheMaxMem(10 * 1024 * 1024L) // 10MB
            Vips.setCacheMaxFiles(5)
        } catch (e: Exception) {
            fail("Setting configuration threw exception: ${e.message}")
        }
    }
}
