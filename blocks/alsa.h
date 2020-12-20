/* CFLAGS+=$(pkg-config --libs --cflags alsa) */
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define block_alsa_strerror(msg) block_errorf(msg ": %s", snd_strerror(err))
#define block_alsa_die(msg) do { \
	block_alsa_strerror(msg); \
	goto fail; \
} while (0)

static bool changed = false;

static int
elem_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	changed |= !!(mask & SND_CTL_EVENT_MASK_VALUE);
}

static int
mixer_callback(snd_mixer_t *mixer, unsigned int mask, snd_mixer_elem_t *elem)
{
	if (mask & SND_CTL_EVENT_MASK_ADD)
		snd_mixer_elem_set_callback(elem, elem_callback);
	return 0;
}


DEFINE_BLOCK(alsa)
{
	enum { SND_CTL_SUBSCRIBE = 1 };

	struct {
		char const *name;
		char *card_name;
		snd_mixer_t *mixer;
		snd_mixer_elem_t *elem;
		snd_mixer_selem_id_t *sid;
	} *state;

	int err;

	BLOCK_SETUP {
		char *arg = strdup(b->arg);
		char *p = strchr(arg, ',');
		if (p)
			*p = '\0';
		char const *device_name = p ? arg : "default";
		char const *selem_name = p ? p + 1 : arg;

		snd_ctl_t *ctl;
		snd_hctl_t *hctl;

		if ((err = snd_mixer_open(&state->mixer, 0)) < 0)
			block_alsa_die("failed to open state->mixer");

		snd_mixer_set_callback(state->mixer, mixer_callback);

		if ((err = snd_ctl_open(&ctl, device_name, SND_CTL_NONBLOCK | SND_CTL_READONLY)) < 0)
			block_alsa_die("failed to open device");

		if ((err = snd_hctl_open_ctl(&hctl, ctl) < 0)) {
			snd_ctl_close(ctl);
			block_alsa_die("failed to open HCTL");
		}

		if ((err = snd_mixer_attach_hctl(state->mixer, hctl)) < 0) {
			snd_ctl_close(ctl);
			snd_hctl_close(hctl);
			block_alsa_die("snd_mixer_attach()");
		}

		if ((err = snd_ctl_subscribe_events(ctl, SND_CTL_SUBSCRIBE)) < 0)
			block_alsa_die("failed to subscribe to events");

		if ((err = snd_mixer_selem_register(state->mixer, NULL, NULL)) < 0)
			block_alsa_die("snd_mixer_selem_register()");

		if ((err = snd_mixer_load(state->mixer)) < 0)
			block_alsa_die("failed to load mixer");

		snd_mixer_selem_id_malloc(&state->sid);
		snd_mixer_selem_id_set_name(state->sid, selem_name);

		if (!(state->elem = snd_mixer_find_selem(state->mixer, state->sid))) {
			block_errorf("unable to find simple control", NULL);
			goto fail;
		}

		free(arg);

		state->name = snd_ctl_name(ctl);

		snd_ctl_card_info_t *info;
		if (!snd_ctl_card_info_malloc(&info)) {
			if (!snd_ctl_card_info(ctl, info))
				state->card_name = strdup(snd_ctl_card_info_get_name(info));

			snd_ctl_card_info_free(info);
		}

		struct pollfd *const pfd = BLOCK_POLLFD;
		assert(1 == snd_ctl_poll_descriptors_count(ctl));
		snd_ctl_poll_descriptors(ctl, pfd, 1);
	}

	snd_mixer_handle_events(state->mixer);
	if (!changed)
		return;
	changed = false;

	int unmuted;
	if (snd_mixer_selem_has_playback_switch(state->elem)) {
		if ((err = snd_mixer_selem_get_playback_switch(state->elem, SND_MIXER_SCHN_MONO, &unmuted)) < 0)
			block_alsa_die("failed to query mute state");
	} else {
		unmuted = 1;
	}

	FORMAT_BEGIN {
	case 'U': /* if unmuted */
		if (unmuted)
			continue;
		break;

	case 'M': /* if muted */
		if (!unmuted)
			continue;
		break;

	case 'n': /* name */
		size = strlen(state->name);
		memcpy(p, state->name, size), p += size;
		continue;

	case 'c': /* card name */
		if (state->card_name) {
			size = strlen(state->card_name);
			memcpy(p, state->card_name, size), p += size;
		}
		continue;

	case 'i': /* speaker icon */
	{
		char const *const icon = unmuted ? "\xef\xa9\xbd" : "\xef\xaa\x80";
		size = strlen(icon);
		memcpy(p, icon, size), p += size;
	}
		continue;

	case 'p': /* volume in percent */
	{
		long vol, min_vol, max_vol;
		if ((err = snd_mixer_selem_get_playback_volume_range(state->elem, &min_vol, &max_vol)) < 0 ||
		    (err = snd_mixer_selem_get_playback_volume(state->elem, SND_MIXER_SCHN_MONO, &vol)) < 0)
		{
			block_alsa_die("failed to get volume");
			break;
		}

		p += sprintf(p, "%d%%", (vol - min_vol) * 100 / (max_vol - min_vol));
	}
		continue;

	case 'd': /* volume in decibel */
	{
		long db, min_db, max_db;
		if ((err = snd_mixer_selem_get_playback_dB_range(state->elem, &min_db, &max_db)) < 0 ||
		    (err = snd_mixer_selem_get_playback_dB(state->elem, SND_MIXER_SCHN_MONO, &db)) < 0)
		{
			block_alsa_die("failed to get volume");
			break;
		}

		if (min_db < db) {
			p += sprintf(p, "%ddB", db / 100);
		} else {
			static char const *const MINUS_INF = "-\xe2\x88\x9e" "dB";
			size = strlen(MINUS_INF);
			memcpy(p, MINUS_INF, size), p += size;
		}
	}
		continue;
	} FORMAT_END;

	return;

fail:
	snd_mixer_close(state->mixer);
	snd_mixer_selem_id_free(state->sid);
	free(state->card_name);
	BLOCK_POLLFD->fd = -1;
	BLOCK_TEARDOWN;

	b->timeout = 5;
}
