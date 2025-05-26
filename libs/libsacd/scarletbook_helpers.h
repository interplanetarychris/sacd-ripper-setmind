/**
 * SACD Ripper - https://github.com/sacd-ripper/
 *
 * Copyright (c) 2010-2015 by respective authors.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef SCARLETBOOK_HELPERS_H_INCLUDED
#define SCARLETBOOK_HELPERS_H_INCLUDED

#include "scarletbook.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * helper functions..
 */
static inline int has_two_channel(scarletbook_handle_t *handle)
{
    return handle->twoch_area_idx != -1;
}

static inline int has_multi_channel(scarletbook_handle_t *handle)
{
    return handle->mulch_area_idx != -1;
}

static inline int has_both_channels(scarletbook_handle_t *handle)
{
    return handle->twoch_area_idx != -1 && handle->mulch_area_idx != -1;
}

static inline area_toc_t* get_two_channel(scarletbook_handle_t *handle)
{
    return(handle->twoch_area_idx == -1 ? 0 : handle->area[handle->twoch_area_idx].area_toc);
}

static inline area_toc_t* get_multi_channel(scarletbook_handle_t *handle)
{
    return(handle->mulch_area_idx == -1 ? 0 : handle->area[handle->mulch_area_idx].area_toc);
}

static inline uint32_t get_content_end_lsn(scarletbook_handle_t *handle)
{
    uint32_t max_end_lsn = 0;
    
    // Find the highest Track_Area_End_Address from all audio areas.
    // Per Super Audio CD System Description (commonly known as "Scarlet Book"), 
    // Part 2: Audio Specification, Version 2.0 of March 2004, Section 3.2.1.2.14:
    // Track_Area_End_Address contains the LSN of the last sector in the Track Area.
    if (has_two_channel(handle))
    {
        area_toc_t *toc = get_two_channel(handle);
        if (toc && toc->track_end > max_end_lsn)
            max_end_lsn = toc->track_end;
    }
    
    if (has_multi_channel(handle))
    {
        area_toc_t *toc = get_multi_channel(handle);
        if (toc && toc->track_end > max_end_lsn)
            max_end_lsn = toc->track_end;
    }
    
    return max_end_lsn;
}

static inline uint32_t get_area_sectors(scarletbook_handle_t *handle, int area_idx)
{
    if (area_idx < 0 || area_idx >= handle->area_count)
        return 0;
    
    area_toc_t *toc = handle->area[area_idx].area_toc;
    if (!toc)
        return 0;
    
    // Calculate total sectors for this area based on track range
    return toc->track_end - toc->track_start;
}

char *get_speaker_config_string(area_toc_t *);

char *get_frame_format_string(area_toc_t *);

char *get_album_dir(scarletbook_handle_t *);

char *get_music_filename(scarletbook_handle_t *, int, int, const char *);

int utf8cpy(char *, char *, int);

#ifdef __cplusplus
};
#endif
#endif /* SCARLETBOOK_HELPERS_H_INCLUDED */
