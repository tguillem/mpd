/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_CHUNK_H
#define MPD_CHUNK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
	/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
	CHUNK_SIZE = 1020,
};

struct audio_format;

/**
 * A chunk of music data.  Its format is defined by the
 * music_pipe_append() caller.
 */
struct music_chunk {
	/** number of bytes stored in this chunk */
	uint16_t length;

	/** current bit rate of the source file */
	uint16_t bit_rate;

	/** the time stamp within the song */
	float times;

	/**
	 * An optional tag associated with this chunk (and the
	 * following chunks); appears at song boundaries.  The tag
	 * object is owned by this chunk, and must be freed when this
	 * chunk is deinitialized in music_chunk_free()
	 */
	struct tag *tag;

	/** the data (probably PCM) */
	char data[CHUNK_SIZE];
};

void
music_chunk_init(struct music_chunk *chunk);

void
music_chunk_free(struct music_chunk *chunk);

/**
 * Prepares appending to the music chunk.  Returns a buffer where you
 * may write into.  After you are finished, call music_chunk_expand().
 *
 * @param chunk the music_chunk object
 * @param audio_format the audio format for the appended data; must
 * stay the same for the life cycle of this chunk
 * @param data_time the time within the song
 * @param bit_rate the current bit rate of the source file
 * @param max_length_r the maximum write length is returned here
 * @return a writable buffer, or NULL if the chunk is full
 */
void *
music_chunk_write(struct music_chunk *chunk,
		  const struct audio_format *audio_format,
		  float data_time, uint16_t bit_rate,
		  size_t *max_length_r);

/**
 * Increases the length of the chunk after the caller has written to
 * the buffer returned by music_chunk_write().
 *
 * @param chunk the music_chunk object
 * @param audio_format the audio format for the appended data; must
 * stay the same for the life cycle of this chunk
 * @param length the number of bytes which were appended
 * @return true if the chunk is full
 */
bool
music_chunk_expand(struct music_chunk *chunk,
		   const struct audio_format *audio_format, size_t length);

#endif
