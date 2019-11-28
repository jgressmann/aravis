#include <glib.h>
#include <arv.h>
#include <stdint.h>

static ArvCamera *camera = NULL;

static void
register_test (void)
{
	ArvDevice *device;
	int int_value;
	double dbl_value;
	double boolean_value;

	device = arv_camera_get_device (camera);
	g_assert (ARV_IS_GV_DEVICE (device));

	/* Check default */
	int_value = arv_device_get_integer_feature_value (device, "Width", NULL);
	g_assert_cmpint (int_value, ==, ARV_FAKE_CAMERA_WIDTH_DEFAULT);

	arv_device_set_integer_feature_value (device, "Width", 1024, NULL);
	int_value = arv_device_get_integer_feature_value (device, "Width", NULL);
	g_assert_cmpint (int_value, ==, 1024);

	/* Check default */
	int_value = arv_device_get_integer_feature_value (device, "Height", NULL);
	g_assert_cmpint (int_value, ==, ARV_FAKE_CAMERA_HEIGHT_DEFAULT);

	arv_device_set_integer_feature_value (device, "Height", 1024, NULL);
	int_value = arv_device_get_integer_feature_value (device, "Height", NULL);
	g_assert_cmpint (int_value, ==, 1024);

	int_value = arv_device_get_integer_feature_value (device, "BinningHorizontal", NULL);
	g_assert_cmpint (int_value, ==, ARV_FAKE_CAMERA_BINNING_HORIZONTAL_DEFAULT);
	int_value = arv_device_get_integer_feature_value (device, "BinningVertical", NULL);
	g_assert_cmpint (int_value, ==, ARV_FAKE_CAMERA_BINNING_VERTICAL_DEFAULT);
	int_value = arv_device_get_integer_feature_value (device, "PixelFormat", NULL);
	g_assert_cmpint (int_value, ==, ARV_FAKE_CAMERA_PIXEL_FORMAT_DEFAULT);

	dbl_value = arv_device_get_float_feature_value (device, "AcquisitionFrameRate", NULL);
	g_assert_cmpfloat (dbl_value, ==, ARV_FAKE_CAMERA_ACQUISITION_FRAME_RATE_DEFAULT);
	dbl_value = arv_device_get_float_feature_value (device,  "ExposureTimeAbs", NULL);
	g_assert_cmpfloat (dbl_value, ==, ARV_FAKE_CAMERA_EXPOSURE_TIME_US_DEFAULT);

	int_value = arv_device_get_integer_feature_value (device, "GainRaw", NULL);
	g_assert_cmpint (int_value, ==, 0);
	int_value = arv_device_get_integer_feature_value (device, "GainAuto", NULL);
	g_assert_cmpint (int_value, ==, 1);

	int_value = arv_device_get_integer_feature_value (device, "PayloadSize", NULL);
	g_assert_cmpint (int_value, ==, 1024 * 1024);

	arv_device_set_boolean_feature_value (device, "TestBoolean", FALSE, NULL);
	boolean_value = arv_device_get_boolean_feature_value (device, "TestBoolean", NULL);
	g_assert_cmpint (boolean_value, ==, FALSE);
	int_value = arv_device_get_integer_feature_value (device, "TestRegister", NULL);
	g_assert_cmpint (int_value, ==, 123);

	arv_device_set_boolean_feature_value (device, "TestBoolean", TRUE, NULL);
	boolean_value = arv_device_get_boolean_feature_value (device, "TestBoolean", NULL);
	g_assert_cmpint (boolean_value, ==, TRUE);
	int_value = arv_device_get_integer_feature_value (device, "TestRegister", NULL);
	g_assert_cmpint (int_value, ==, 321);
}

static void
acquisition_test (void)
{
	GError *error = NULL;
	ArvBuffer *buffer;
	gint x, y, width, height;

	buffer = arv_camera_acquisition (camera, 0, &error);
	g_assert (error == NULL);
	g_assert (ARV_IS_BUFFER (buffer));

	arv_buffer_get_image_region (buffer, &x, &y, &width, &height);

	g_assert_cmpint (x, ==, 0);
	g_assert_cmpint (y, ==, 0);
	g_assert_cmpint (width, ==, 1024);
	g_assert_cmpint (height, ==, 1024);

	g_assert_cmpint (arv_buffer_get_image_x (buffer), ==, 0);
	g_assert_cmpint (arv_buffer_get_image_y (buffer), ==, 0);
	g_assert_cmpint (arv_buffer_get_image_width (buffer), ==, 1024);
	g_assert_cmpint (arv_buffer_get_image_height (buffer), ==, 1024);

	g_assert (arv_buffer_get_image_pixel_format (buffer) == ARV_PIXEL_FORMAT_MONO_8);

	g_clear_object (&buffer);
}

static void
new_buffer_cb (ArvStream *stream, unsigned *buffer_count)
{
	ArvBuffer *buffer;

	buffer = arv_stream_try_pop_buffer (stream);
	if (buffer != NULL) {
		(*buffer_count)++;
		if (*buffer_count == 10) {
			/* Sleep after the last buffer was received, in order
			 * to keep a reference to stream while the main loop
			 * ends. If the main is able to unref stream while
			 * this signal callback is still waiting, stream will
			 * be finalized in its stream thread contex (because
			 * g_signal_emit holds a reference to stream), leading
			 * to a deadlock. */
			sleep (1);
		}
		arv_stream_push_buffer (stream, buffer);
	}
}

static void
stream_test (void)
{
	ArvStream *stream;
	size_t payload;
	unsigned buffer_count = 0;
	unsigned i;

	stream = arv_camera_create_stream (camera, NULL, NULL);
	g_assert (ARV_IS_STREAM (stream));

	payload = arv_camera_get_payload (camera, NULL);

	for (i = 0; i < 5; i++)
		arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));

	g_signal_connect (stream, "new-buffer", G_CALLBACK (new_buffer_cb), &buffer_count);
	arv_stream_set_emit_signals (stream, TRUE);

	arv_camera_start_acquisition (camera, NULL);

	while (buffer_count < 10)
		usleep (1000);

	arv_camera_stop_acquisition (camera, NULL);
	/* The following will block until the signal callback returns
	 * which avoids a race and possible deadlock.
	 */
	arv_stream_set_emit_signals (stream, FALSE);

	g_clear_object (&stream);

	/* For actually testing the deadlock condition (see comment in
	 * new_buffer_cb), one must wait a bit before leaving this test,
	 * because otherwise the stream thread will be killed while sleeping. */
	sleep (2);
}

#define N_BUFFERS	5

static struct {
	int width;
	int height;
} rois[] = { {100, 100}, {200, 200}, {300,300} };

static void
dynamic_roi_test (void)
{
	ArvStream *stream;
	size_t payload;
	unsigned buffer_count = 0;
	unsigned i, j;

	stream = arv_camera_create_stream (camera, NULL, NULL);
	g_assert (ARV_IS_STREAM (stream));

	payload = arv_camera_get_payload (camera, NULL);

	g_signal_connect (stream, "new-buffer", G_CALLBACK (new_buffer_cb), &buffer_count);
	arv_stream_set_emit_signals (stream, TRUE);

	for (j = 0; j < G_N_ELEMENTS (rois); j++) {
		unsigned int n_deleted;
		int height, width;

		n_deleted = arv_stream_stop_thread (stream, TRUE);

		if (j == 0)
			g_assert (n_deleted == 0);
		else
			g_assert (n_deleted == N_BUFFERS);

		buffer_count = 0;

		arv_camera_set_region (camera, 0, 0, rois[j].width , rois[j].height, NULL);
		arv_camera_get_region (camera, NULL, NULL, &width, &height, NULL);

		g_assert (width == rois[j].width);
		g_assert (height == rois[j].height);

		payload = arv_camera_get_payload (camera, NULL);

		for (i = 0; i < N_BUFFERS; i++)
			arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));

		arv_stream_start_thread (stream);

		arv_camera_start_acquisition (camera, NULL);

		while (buffer_count < 10) {
			usleep (10000);
		}

		arv_camera_stop_acquisition (camera, NULL);
	}

	arv_stream_set_emit_signals (stream, FALSE);

	g_clear_object (&stream);
}


#define RING_BUFFER_TEST_BUFFER_COUNT 5
static int total_locks;
static int total_unlocks;
static int per_buffer_locks[RING_BUFFER_TEST_BUFFER_COUNT];
static int per_buffer_unlocks[RING_BUFFER_TEST_BUFFER_COUNT];
static gboolean available[RING_BUFFER_TEST_BUFFER_COUNT] = {
	FALSE, TRUE, FALSE, FALSE, TRUE
};

static gboolean
try_lock_cb (ArvStream *stream, ArvBuffer *buffer)
{
	(void)stream;
	unsigned index = (uintptr_t)arv_buffer_get_user_data(buffer);
	g_assert (index >= 0 && index < RING_BUFFER_TEST_BUFFER_COUNT);

	if (available[index]) {
		++total_locks;
		++per_buffer_locks[index];
		return TRUE;
	}

	return FALSE;
}

static void
unlock_cb (ArvStream *stream, ArvBuffer *buffer)
{
	(void)stream;
	unsigned index = (uintptr_t)arv_buffer_get_user_data(buffer);
	g_assert (index >= 0 && index < RING_BUFFER_TEST_BUFFER_COUNT);
	++total_unlocks;
	++per_buffer_unlocks[index];
}

static void
stream_cb (void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer)
{
	switch (type) {
	case ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE: {
		unsigned* buffer_count = user_data;
		g_assert (buffer);
		(*buffer_count)++;
	} break;
	default:
		break;
	}

}

static void
ring_buffer_mode_test (void)
{
	ArvStream *stream;
	size_t payload;
	unsigned buffer_count = 0;
	unsigned i;
	unsigned locks = 0;
	unsigned unlocks = 0;

	stream = arv_camera_create_stream (camera, stream_cb, &buffer_count);
	g_assert (ARV_IS_STREAM (stream));

	payload = arv_camera_get_payload (camera, NULL);

	for (i = 0; i < RING_BUFFER_TEST_BUFFER_COUNT; i++) {
		ArvBuffer* buffer = arv_buffer_new_full (payload, NULL, (void*)(uintptr_t)i, NULL);
		arv_stream_push_buffer (stream, buffer);
	}

	arv_stream_set_ring_buffer_mode (stream, TRUE);
	arv_stream_set_ring_buffer_callbacks (stream, &try_lock_cb, &unlock_cb);

	arv_camera_start_acquisition (camera, NULL);

	while (buffer_count < 10)
		usleep (1000);

	arv_camera_stop_acquisition (camera, NULL);

	g_clear_object (&stream);

	/* number of lock / unlock counts must not be zero */
	g_assert (total_locks > 0);
	g_assert (total_unlocks > 0);

	/* number of lock / unlock counts must match */
	g_assert (total_locks == total_unlocks);

	/* also per buffer */
	for (i = 0; i < RING_BUFFER_TEST_BUFFER_COUNT; i++) {
		g_assert (per_buffer_locks[i] == per_buffer_locks[i]);
		locks += per_buffer_locks[i];
		unlocks += per_buffer_locks[i];
	}

	/* per buffer lock/unlock must sum up to totals */
	g_assert (total_locks == locks);
	g_assert (total_unlocks == unlocks);

	/* buffer 0, 2, 3 must not be used */
	g_assert (per_buffer_locks[0] == 0);
	g_assert (per_buffer_locks[2] == 0);
	g_assert (per_buffer_locks[3] == 0);
}

int
main (int argc, char *argv[])
{
	ArvGvFakeCamera *simulator;
	int result;

	g_test_init (&argc, &argv, NULL);

	arv_set_fake_camera_genicam_filename (GENICAM_FILENAME);

	simulator = arv_gv_fake_camera_new ("lo", NULL);

	camera = arv_camera_new ("Aravis-GV01");
	g_assert (ARV_IS_CAMERA (camera));

	g_test_add_func ("/fakegv/device_registers", register_test);
	g_test_add_func ("/fakegv/acquisition", acquisition_test);
	g_test_add_func ("/fakegv/stream", stream_test);
	g_test_add_func ("/fakegv/dynamic_roi", dynamic_roi_test);
	g_test_add_func ("/fakegv/ring_buffer_mode", ring_buffer_mode_test);

	result = g_test_run();

	g_object_unref (camera);

	g_object_unref (simulator);

	arv_shutdown ();

	return result;
}

