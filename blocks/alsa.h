/* CFLAGS+=$(pkg-config --libs --cflags alsa) */
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SND_CTL_SUBSCRIBE 1

#define block_alsa_strerror(msg) block_errorf(msg ": %s", snd_strerror(err))

DEFINE_BLOCK(alsa)
{
	struct {
		snd_ctl_t *ctl;
		char const *card;
		snd_mixer_selem_id_t *sid;
	} *state;

	snd_ctl_event_t *event;
	snd_mixer_elem_t *elem;
	snd_mixer_t *mixer;
	int err;

	bool changed = false;

	BLOCK_INIT {
		int const mixer_index = 0;
		char const *mixer_name = b->arg;

		state->card = "default";

		if ((err = snd_ctl_open(&state->ctl, state->card, SND_CTL_READONLY)) < 0)
			block_alsa_strerror("failed to open card");

		if ((err = snd_ctl_subscribe_events(state->ctl, SND_CTL_SUBSCRIBE)) < 0)
			block_alsa_strerror("failed to subscribe to events");

		snd_ctl_nonblock(state->ctl, true);

		snd_mixer_selem_id_malloc(&state->sid);
		snd_mixer_selem_id_set_index(state->sid, mixer_index);
		snd_mixer_selem_id_set_name(state->sid, mixer_name);

		struct pollfd *const pfd = BLOCK_POLLFD;
		assert(1 == snd_ctl_poll_descriptors_count(state->ctl));
		snd_ctl_poll_descriptors(state->ctl, pfd, 1);

		changed = true;

	}

	snd_ctl_event_alloca(&event);

	for (;;) {
		if ((err = snd_ctl_read(state->ctl, event)) < 0) {
			if (-EAGAIN == err)
				break;

			block_alsa_strerror("failed to read event");
		}

		/* is changed? */
		if (SND_CTL_EVENT_ELEM == snd_ctl_event_get_type(event) &&
		    (SND_CTL_EVENT_MASK_VALUE & snd_ctl_event_elem_get_mask(event)))
		{
			changed = true;
		}
	}

	if (!changed)
		return;

	if ((err = snd_mixer_open(&mixer, 0)) < 0)
		block_alsa_strerror("failed to open mixer");

	if ((err = snd_mixer_attach(mixer, state->card)) < 0)
		block_alsa_strerror("snd_mixer_attach()");

	if ((err = snd_mixer_selem_register(mixer, NULL, NULL)) < 0)
		block_alsa_strerror("snd_mixer_selem_register()");

	if ((err = snd_mixer_load(mixer)) < 0)
		block_alsa_strerror("snd_mixer_load()");

	if (!(elem = snd_mixer_find_selem(mixer, state->sid)))
		block_alsa_strerror("snd_mixer_find_selem()");

	FORMAT_BEGIN {
	case 'i':
	{
		int unmuted;

		if (snd_mixer_selem_has_playback_switch(elem)) {
			if ((err = snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &unmuted)) < 0) {
				block_alsa_strerror("failed to query mute state");
				break;
			}
		} else {
			unmuted = 1;
		}

		char *const icon = unmuted ? "\xef\xa9\xbd" : "\xef\xaa\x80";
		size = strlen(icon);
		memcpy(p, icon, size), p += size;
	}
		continue;

	case 'd':
	{
		long db, min_db, max_db;
		if ((err = snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db)) < 0 ||
		    (err = snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_MONO, &db)) < 0)
		{
			block_alsa_strerror("failed to get volume");
			break;
		}

		if (min_db < db) {
			p += sprintf(p, "%ddB", db / 100);
		} else {
			char *const minus_inf = "-\xe2\x88\x9e" "dB";
			size = strlen(minus_inf);
			memcpy(p, minus_inf, size), p += size;
		}
	}
		continue;
	} FORMAT_END;

	snd_mixer_close(mixer);
}
