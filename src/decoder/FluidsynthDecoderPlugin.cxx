/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "FluidsynthDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "CheckAudioFormat.hxx"
#include "conf.h"

#include <glib.h>

#include <fluidsynth.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "fluidsynth"

static unsigned sample_rate;
static const char *soundfont_path;

/**
 * Convert a fluidsynth log level to a GLib log level.
 */
static GLogLevelFlags
fluidsynth_level_to_glib(enum fluid_log_level level)
{
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		return G_LOG_LEVEL_CRITICAL;

	case FLUID_WARN:
		return G_LOG_LEVEL_WARNING;

	case FLUID_INFO:
		return G_LOG_LEVEL_INFO;

	case FLUID_DBG:
	case LAST_LOG_LEVEL:
		return G_LOG_LEVEL_DEBUG;
	}

	/* invalid fluidsynth log level */
	return G_LOG_LEVEL_MESSAGE;
}

/**
 * The fluidsynth logging callback.  It forwards messages to the GLib
 * logging library.
 */
static void
fluidsynth_mpd_log_function(int level, char *message, G_GNUC_UNUSED void *data)
{
	g_log(G_LOG_DOMAIN, fluidsynth_level_to_glib(fluid_log_level(level)),
	      "%s", message);
}

static bool
fluidsynth_init(const config_param &param)
{
	GError *error = nullptr;

	sample_rate = param.GetBlockValue("sample_rate", 48000u);
	if (!audio_check_sample_rate(sample_rate, &error)) {
		g_warning("%s\n", error->message);
		g_error_free(error);
		return false;
	}

	soundfont_path = param.GetBlockValue("soundfont",
					     "/usr/share/sounds/sf2/FluidR3_GM.sf2");

	fluid_set_log_function(LAST_LOG_LEVEL,
			       fluidsynth_mpd_log_function, nullptr);

	return true;
}

static void
fluidsynth_file_decode(struct decoder *decoder, const char *path_fs)
{
	char setting_sample_rate[] = "synth.sample-rate";
	/*
	char setting_verbose[] = "synth.verbose";
	char setting_yes[] = "yes";
	*/
	fluid_settings_t *settings;
	fluid_synth_t *synth;
	fluid_player_t *player;
	int ret;
	enum decoder_command cmd;

	/* set up fluid settings */

	settings = new_fluid_settings();
	if (settings == nullptr)
		return;

	fluid_settings_setnum(settings, setting_sample_rate, sample_rate);

	/*
	fluid_settings_setstr(settings, setting_verbose, setting_yes);
	*/

	/* create the fluid synth */

	synth = new_fluid_synth(settings);
	if (synth == nullptr) {
		delete_fluid_settings(settings);
		return;
	}

	ret = fluid_synth_sfload(synth, soundfont_path, true);
	if (ret < 0) {
		g_warning("fluid_synth_sfload() failed");
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* create the fluid player */

	player = new_fluid_player(synth);
	if (player == nullptr) {
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	ret = fluid_player_add(player, path_fs);
	if (ret != 0) {
		g_warning("fluid_player_add() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* start the player */

	ret = fluid_player_play(player);
	if (ret != 0) {
		g_warning("fluid_player_play() failed");
		delete_fluid_player(player);
		delete_fluid_synth(synth);
		delete_fluid_settings(settings);
		return;
	}

	/* initialization complete - announce the audio format to the
	   MPD core */

	const AudioFormat audio_format(sample_rate, SampleFormat::S16, 2);
	decoder_initialized(decoder, audio_format, false, -1);

	while (fluid_player_get_status(player) == FLUID_PLAYER_PLAYING) {
		int16_t buffer[2048];
		const unsigned max_frames = G_N_ELEMENTS(buffer) / 2;

		/* read samples from fluidsynth and send them to the
		   MPD core */

		ret = fluid_synth_write_s16(synth, max_frames,
					    buffer, 0, 2,
					    buffer, 1, 2);
		if (ret != 0)
			break;

		cmd = decoder_data(decoder, nullptr, buffer, sizeof(buffer),
				   0);
		if (cmd != DECODE_COMMAND_NONE)
			break;
	}

	/* clean up */

	fluid_player_stop(player);
	fluid_player_join(player);

	delete_fluid_player(player);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);
}

static bool
fluidsynth_scan_file(const char *file,
		     G_GNUC_UNUSED const struct tag_handler *handler,
		     G_GNUC_UNUSED void *handler_ctx)
{
	return fluid_is_midifile(file);
}

static const char *const fluidsynth_suffixes[] = {
	"mid",
	nullptr
};

const struct decoder_plugin fluidsynth_decoder_plugin = {
	"fluidsynth",
	fluidsynth_init,
	nullptr,
	nullptr,
	fluidsynth_file_decode,
	fluidsynth_scan_file,
	nullptr,
	nullptr,
	fluidsynth_suffixes,
	nullptr,
};