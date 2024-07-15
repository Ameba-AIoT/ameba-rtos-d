#include <math.h>

#include "ameba_soc.h"
#include "audio/audio_track.h"
#include "audio/audio_service.h"
#include "osal_c/osal_errnos.h"
#include "platform_stdlib.h"

#define PLAY_SECONDS           200
#define M_PI 3.14159265358979323846

#define EXAMPLE_AUDIO_DEBUG(fmt, args...)    printf("=> D/AudioTrackExample:[%s]: " fmt "\n", __func__, ## args)
#define EXAMPLE_AUDIO_ERROR(fmt, args...)    printf("=> E/AudioTrackExample:[%s]: " fmt "\n", __func__, ## args)

static uint32_t  g_track_rate = 16000;
static uint32_t  g_track_channel = 2;
static uint32_t  g_track_format = 16;
static uint32_t  g_write_frames_one_time = 1024;

/* pcm frequency in Hz */
static double        freq = 1000;

static void generate_sine(int8_t *buffer, uint32_t count, uint32_t rate, uint32_t channels, uint32_t bits, double *_phase)
{
	static double max_phase = 2. * M_PI;
	double phase = *_phase;
	double step = max_phase * freq / (double)rate;
	uint8_t *samples[channels];
	int32_t steps[channels];
	uint32_t chn;
	int32_t format_bits = bits;
	uint32_t maxval = (1 << (format_bits - 1)) - 1;
	/* bytes per sample */
	int32_t bps = format_bits / 8;
	int32_t to_unsigned = 0;

	for (chn = 0; chn < channels; chn++) {
		steps[chn] = bps * channels;
		samples[chn] = (uint8_t *)(buffer + chn * bps);
	}

	while (count-- > 0) {
		int32_t res, i;
		res = sin(phase) * maxval;
		if (to_unsigned) {
			res ^= 1U << (format_bits - 1);
		}
		for (chn = 0; chn < channels; chn++) {
			{
				for (i = 0; i < bps; i++) {
					*(samples[chn] + i) = (res >>  i * 8) & 0xff;
				}
			}
			samples[chn] += steps[chn];
		}
		phase += step;
		if (phase >= max_phase) {
			phase -= max_phase;
		}
	}
	*_phase = phase;
}

void play_sample(uint32_t channels, uint32_t rate, uint32_t bits, uint32_t period_size)
{
	struct RTAudioTrack *audio_track;

	uint32_t frames_written = 0;
	uint32_t frame_size = channels * bits / 8;
	int32_t track_buf_size = 4096;
	uint32_t format;
	uint32_t track_start_threshold = 0;
	uint32_t play_frame_size = rate * PLAY_SECONDS;

	RTAudioTimestamp tstamp;
	uint32_t frames_played = 0;
	uint64_t frames_played_at_us = 0;
	long long now_us = 0;

	uint32_t sine_frames_count = period_size;
	int8_t sine_buf[sine_frames_count * frame_size];
	double phase = 0;

	EXAMPLE_AUDIO_DEBUG("play sample channels:%d, rate:%d,bits=%d,period_size=%d", channels, rate, bits, period_size);

	switch (bits) {
	case 16:
		format = RTAUDIO_FORMAT_PCM_16_BIT;
		break;
	case 24:
		format = RTAUDIO_FORMAT_PCM_24_BIT_PACKED;
		break;
	case 32:
		format = RTAUDIO_FORMAT_PCM_32_BIT;
		break;
	default:
		break;
	}

	audio_track = RTAudioTrack_Create();
	if (!audio_track) {
		EXAMPLE_AUDIO_ERROR("error: new RTAudioTrack failed");
		return;
	}

	track_buf_size = RTAudioTrack_GetMinBufferBytes(audio_track, RTAUDIO_CATEGORY_MEDIA, rate, format, channels) * 2;
	RTAudioTrackConfig  track_config;
	track_config.category_type = RTAUDIO_CATEGORY_MEDIA;
	track_config.sample_rate = rate;
	track_config.format = format;
	track_config.channel_count = channels;
	track_config.buffer_bytes = track_buf_size;
	RTAudioTrack_Init(audio_track, &track_config);

	EXAMPLE_AUDIO_DEBUG("track buf size:%d", track_buf_size);

	ssize_t size = sine_frames_count * channels  * bits / 8;

	if (RTAudioTrack_Start(audio_track) != OSAL_OK) {
		EXAMPLE_AUDIO_ERROR("error: audio track start fail");
		return;
	}

	RTAudioTrack_SetVolume(audio_track, 1.0, 1.0);

	while (1) {
		generate_sine(sine_buf, sine_frames_count, rate, channels, bits, &phase);
		RTAudioTrack_Write(audio_track, (u8 *)sine_buf, size, true);
		frames_written += size / (frame_size);
		if (frames_written >= play_frame_size) {
			break;
		}
	}

	OsalMSleep(20);

	RTAudioTrack_Pause(audio_track);
	RTAudioTrack_Stop(audio_track);
	RTAudioTrack_Destroy(audio_track);

	audio_track = NULL;
}

void example_audio_track_thread(void *param)
{
	EXAMPLE_AUDIO_DEBUG("Audio track demo begin");
	(void) param;

	RTAudioService_Init();
	play_sample(g_track_channel, g_track_rate, g_track_format, g_write_frames_one_time);
	OsalSleep(2);

	vTaskDelete(NULL);
}


void example_audio_track(char **argv)
{
	/* parse command line arguments */
	while (*argv) {
		if (strcmp(*argv, "-c") == 0) {
			argv++;
			if (*argv) {
				g_track_channel = atoi(*argv);
			}
		} else if (strcmp(*argv, "-r") == 0) {
			argv++;
			if (*argv) {
				g_track_rate = atoi(*argv);
			}
		} else if (strcmp(*argv, "-b") == 0) {
			argv++;
			if (*argv) {
				g_write_frames_one_time = atoi(*argv);
			}
		} else if (strcmp(*argv, "-f") == 0) {
			argv++;
			if (*argv) {
				g_track_format = atoi(*argv);
			}
		}
		if (*argv) {
			argv++;
		}
	}

	if (xTaskCreate(example_audio_track_thread, ((const char *)"example_audio_track_thread"), 4096, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
		EXAMPLE_AUDIO_ERROR("error: xTaskCreate(example_audio_track_thread) failed");
	}
}

u32 example_track_test(u16 argc, unsigned char **argv)

{
	(void) argc;
	example_audio_track((char **)argv);
	return _TRUE;
}