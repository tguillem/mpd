/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "fs/io/GunzipReader.hxx"
#include "fs/io/FileReader.hxx"
#include "fs/io/StdioOutputStream.hxx"
#include "util/Error.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static bool
Copy(OutputStream &dest, Reader &src, Error &error)
{
	while (true) {
		char buffer[4096];
		size_t nbytes = src.Read(buffer, sizeof(buffer), error);
		if (nbytes == 0)
			return !error.IsDefined();

		if (!dest.Write(buffer, nbytes, error))
			return false;
	}
}

static bool
CopyGunzip(OutputStream &dest, Reader &_src, Error &error)
{
	GunzipReader src(_src, error);
	return src.IsDefined() && Copy(dest, src, error);
}

static bool
CopyGunzip(FILE *_dest, Path src_path, Error &error)
{
	StdioOutputStream dest(_dest);
	FileReader src(src_path, error);
	return src.IsDefined() && CopyGunzip(dest, src, error);
}

int
main(int argc, gcc_unused char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: run_gunzip PATH\n");
		return EXIT_FAILURE;
	}

	Path path = Path::FromFS(argv[1]);

	Error error;
	if (!CopyGunzip(stdout, path, error)) {
		fprintf(stderr, "%s\n", error.GetMessage());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
