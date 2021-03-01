/* CFLAGS+=$(pkg-config --libs --cflags alsa) */
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "blocks/seabar.h"

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

		if ((err = snd_mixer_open(&state->mixer, 0)) < 0)
			block_alsa_die("failed to open state->mixer");

		snd_mixer_set_callback(state->mixer, mixer_callback);

		snd_ctl_t *ctl;
		if ((err = snd_ctl_open(&ctl, device_name, SND_CTL_NONBLOCK | SND_CTL_READONLY)) < 0)
			block_alsa_die("failed to open device");

		snd_hctl_t *hctl;
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
		if (1 != snd_ctl_poll_descriptors_count(ctl))
			block_errorf("watching only 1 of %d fds",
					snd_ctl_poll_descriptors_count(ctl));
		snd_ctl_poll_descriptors(ctl, pfd, 1);
	}

	snd_mixer_handle_events(state->mixer);
	if (!changed)
		return;
	changed = false;

#define S(prefix, postfix) (capture ? prefix##capture##postfix : prefix##playback##postfix)

	int capture = 0;
	int unmuted = 0;

	FORMAT_BEGIN {
	case 'C': /* Capture. */
	case 'P': /* Playback. */
		capture = 'C' == *format;
		if (S(snd_mixer_selem_get_, _switch)(state->elem, SND_MIXER_SCHN_MONO, &unmuted) < 0)
			unmuted = 1;
		continue;

	case 'U': /* If unmuted. */
		if (unmuted)
			continue;
		break;

	case 'M': /* If muted. */
		if (!unmuted)
			continue;
		break;

	case 'n': /* Name. */
		sprint(&p, state->name);
		continue;

	case 'c': /* Card name. */
		sprint(&p, state->card_name);
		continue;

	case 'i': /* Icon. */
	{
		char const *const icon = use_text_icon
			? (unmuted
				? (!capture ? "PLAY " : "CAPT ")
				: (!capture ? "PMUT " : "CMUT "))
			: (!capture
				? (unmuted ? "\xef\xa9\xbd" : "\xef\xaa\x80")
				: (unmuted ? "\xef\x84\xb0 " : "\xef\x84\xb1 "));

		sprint(&p, icon);
	}
		continue;

	case 'p': /* Volume in percent. */
	{
		long vol, min_vol, max_vol;
		if ((err = S(snd_mixer_selem_get_, _volume_range)(state->elem, &min_vol, &max_vol)) < 0 ||
		    (err = S(snd_mixer_selem_get_, _volume)(state->elem, SND_MIXER_SCHN_MONO, &vol)) < 0)
		{
			block_alsa_die("failed to get volume");
			break;
		}

		p += sprintf(p, "%d%%", (vol - min_vol) * 100 / (max_vol - min_vol));
	}
		continue;

	case 'd': /* Volume in decibel. */
	{
		long db, min_db, max_db;
		if ((err = S(snd_mixer_selem_get_, _dB_range)(state->elem, &min_db, &max_db)) < 0 ||
		    (err = S(snd_mixer_selem_get_, _dB)(state->elem, SND_MIXER_SCHN_MONO, &db)) < 0)
		{
			block_alsa_die("failed to get volume");
			break;
		}

		if (min_db < db)
			p += sprintf(p, "%ddB", db / 100);
		else
			sprint(&p, "-\xe2\x88\x9e" "dB" /* Minus infinity. */);
	}
		continue;
	} FORMAT_END;

#undef S

	return;

fail:
	snd_mixer_close(state->mixer);
	snd_mixer_selem_id_free(state->sid);
	free(state->card_name);
	BLOCK_POLLFD->fd = -1;
	BLOCK_TEARDOWN;

	b->timeout = 5;
}
