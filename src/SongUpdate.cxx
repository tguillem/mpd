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

#include "config.h" /* must be first for large file support */
#include "DetachedSong.hxx"
#include "db/plugins/simple/Song.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "fs/FileInfo.hxx"
#include "decoder/DecoderList.hxx"
#include "tag/Tag.hxx"
#include "tag/TagBuilder.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "tag/ApeTag.hxx"
#include "TagStream.hxx"
#include "input/InputStream.hxx"

#ifdef ENABLE_ARCHIVE
#include "TagArchive.hxx"
#endif

#include <assert.h>
#include <string.h>
#include <sys/stat.h>

#ifdef ENABLE_DATABASE

Song *
Song::LoadFile(Storage &storage, const char *path_utf8, Directory &parent)
{
	assert(!uri_has_scheme(path_utf8));
	assert(strchr(path_utf8, '\n') == nullptr);

	Song *song = NewFile(path_utf8, parent);

	//in archive ?
	bool success =
#ifdef ENABLE_ARCHIVE
		parent.device == DEVICE_INARCHIVE
		? song->UpdateFileInArchive(storage)
		:
#endif
		song->UpdateFile(storage);
	if (!success) {
		song->Free();
		return nullptr;
	}

	return song;
}

#endif

/**
 * Attempts to load APE or ID3 tags from the specified file.
 */
static bool
tag_scan_fallback(InputStream &is,
		  const struct tag_handler *handler, void *handler_ctx)
{
	return tag_ape_scan2(is, handler, handler_ctx) ||
		tag_id3_scan(is, handler, handler_ctx);
}

#ifdef ENABLE_DATABASE

bool
Song::UpdateFile(Storage &storage)
{
	const auto &relative_uri = GetURI();

	StorageFileInfo info;
	if (!storage.GetInfo(relative_uri.c_str(), true, info, IgnoreError()))
		return false;

	if (!info.IsRegular())
		return false;

	TagBuilder tag_builder;

	Mutex mutex;
	Cond cond;
	const auto absolute_uri = storage.MapUTF8(relative_uri.c_str());

	InputStream *is = InputStream::OpenReady(absolute_uri.c_str(), mutex,
						 cond, IgnoreError());
	if (is == nullptr)
		return false;

	if (!tag_stream_scan(*is, full_tag_handler, &tag_builder))
	{
		delete is;
		return false;
	}
	if (tag_builder.IsEmpty())
		tag_scan_fallback(*is, &full_tag_handler,
				  &tag_builder);

	delete is;
	mtime = info.mtime;
	tag_builder.Commit(tag);
	return true;
}

#endif

#ifdef ENABLE_ARCHIVE

bool
Song::UpdateFileInArchive(const Storage &storage)
{
	/* check if there's a suffix and a plugin */

	const char *suffix = uri_get_suffix(uri);
	if (suffix == nullptr)
		return false;

	if (!decoder_plugins_supports_suffix(suffix))
		return false;

	const auto path_fs = parent->IsRoot()
		? storage.MapFS(uri)
		: storage.MapChildFS(parent->GetPath(), uri);
	if (path_fs.IsNull())
		return false;

	TagBuilder tag_builder;
	if (!tag_archive_scan(path_fs, full_tag_handler, &tag_builder))
		return false;

	tag_builder.Commit(tag);
	return true;
}

#endif

bool
DetachedSong::Update()
{
	TagBuilder tag_builder;
	Mutex mutex;
	Cond cond;

	if (IsAbsoluteFile()) {
		const AllocatedPath path_fs =
			AllocatedPath::FromUTF8(GetRealURI());
		FileInfo fi;
		if (!GetFileInfo(path_fs, fi) || !fi.IsRegular())
			return false;
		mtime = fi.GetModificationTime();
	} else {
		mtime = 0; // TODO: implement
	}
	InputStream *is = InputStream::OpenReady(uri.c_str(), mutex, cond,
						 IgnoreError());
	if (is == nullptr)
		return false;

	if (!tag_stream_scan(*is, full_tag_handler, &tag_builder))
	{
		delete is;
		return false;
	}
	if (tag_builder.IsEmpty())
		tag_scan_fallback(*is, &full_tag_handler,
				  &tag_builder);
	delete is;

	tag_builder.Commit(tag);
	return true;
}
