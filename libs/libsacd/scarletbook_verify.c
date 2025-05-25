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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <wchar.h>

#include "scarletbook.h"
#include "scarletbook_verify.h"
#include "sacd_reader.h"

int scarletbook_verify(scarletbook_handle_t *handle)
{
    int verification_errors = 0;
    int i, j;
    
    if (\!handle)
    {
        fprintf(stderr, "ERROR: No valid ScarletBook handle available for verification\n");
        return -1;
    }

    fwprintf(stdout, L"\n=== ScarletBook Verification ===\n");

    // Structural validation
    if (\!handle->master_toc)
    {
        fwprintf(stdout, L"ERROR: Missing Master TOC\n");
        verification_errors++;
    }
    else
    {
        fwprintf(stdout, L"✓ Master TOC present\n");
        
        // Version check
        uint8_t major = handle->master_toc->version.major;
        uint8_t minor = handle->master_toc->version.minor;
        fwprintf(stdout, L"✓ Format version: %d.%d\n", major, minor);
        
        if (major \!= SUPPORTED_VERSION_MAJOR || minor \!= SUPPORTED_VERSION_MINOR)
        {
            fwprintf(stdout, L"  WARNING: Version %d.%d may not be fully supported (expected %d.%d)\n", 
                     major, minor, SUPPORTED_VERSION_MAJOR, SUPPORTED_VERSION_MINOR);
        }
    }

    // Area validation
    fwprintf(stdout, L"✓ Area count: %d\n", handle->area_count);
    
    if (handle->area_count == 0)
    {
        fwprintf(stdout, L"ERROR: No areas found\n");
        verification_errors++;
    }
    
    for (i = 0; i < handle->area_count; i++)
    {
        area_toc_t *area_toc = handle->area[i].area_toc;
        if (\!area_toc)
        {
            fwprintf(stdout, L"ERROR: Area %d missing TOC\n", i);
            verification_errors++;
            continue;
        }
        
        fwprintf(stdout, L"✓ Area %d: %d tracks, sectors %u-%u\n", 
                 i, area_toc->track_count, area_toc->track_start, area_toc->track_end);
        
        // Sector boundary validation
        if (area_toc->track_start >= area_toc->track_end)
        {
            fwprintf(stdout, L"ERROR: Area %d invalid sector range (%u >= %u)\n", 
                     i, area_toc->track_start, area_toc->track_end);
            verification_errors++;
        }
        
        // Track validation
        area_tracklist_offset_t *tracklist = handle->area[i].area_tracklist_offset;
        if (tracklist && area_toc->track_count > 0)
        {
            for (j = 0; j < area_toc->track_count; j++)
            {
                uint32_t track_start = tracklist->track_start_lsn[j];
                uint32_t track_length = tracklist->track_length_lsn[j];
                uint32_t track_end = track_start + track_length;
                
                // Check track boundaries (track_end is exclusive, so track_end == area_end is OK)
                if (track_start < area_toc->track_start || track_end > area_toc->track_end + 1)
                {
                    fwprintf(stdout, L"ERROR: Track %d outside area bounds (sectors %u-%u, area %u-%u)\n",
                             j + 1, track_start, track_end, area_toc->track_start, area_toc->track_end);
                    verification_errors++;
                }
                
                // Check for overlapping tracks (allowing adjacent tracks to share boundary)
                if (j > 0)
                {
                    uint32_t prev_end = tracklist->track_start_lsn[j-1] + tracklist->track_length_lsn[j-1];
                    // Tracks can be adjacent (curr_start == prev_end - 1) but not overlap (curr_start < prev_end - 1)
                    if (track_start < prev_end - 1)
                    {
                        fwprintf(stdout, L"ERROR: Track %d overlaps with previous track (prev_end=%u, curr_start=%u)\n", 
                                 j + 1, prev_end, track_start);
                        verification_errors++;
                    }
                }
            }
            fwprintf(stdout, L"✓ All tracks within area boundaries\n");
        }
    }

    // Quick sector spot-check (test readability at key positions)
    fwprintf(stdout, L"\n--- Spot-checking sector readability ---\n");
    
    for (i = 0; i < handle->area_count; i++)
    {
        area_toc_t *area_toc = handle->area[i].area_toc;
        area_tracklist_offset_t *tracklist = handle->area[i].area_tracklist_offset;
        
        if (\!area_toc || \!tracklist) continue;
        
        // Test area start
        uint8_t sector_buffer[SACD_LSN_SIZE];
        if (sacd_read_block_raw(handle->sacd, area_toc->track_start, 1, sector_buffer) \!= 1)
        {
            fwprintf(stdout, L"ERROR: Cannot read area %d start sector %u\n", i, area_toc->track_start);
            verification_errors++;
        }
        else
        {
            fwprintf(stdout, L"✓ Area %d start sector readable\n", i);
        }
        
        // Test area end  
        if (sacd_read_block_raw(handle->sacd, area_toc->track_end - 1, 1, sector_buffer) \!= 1)
        {
            fwprintf(stdout, L"ERROR: Cannot read area %d end sector %u\n", i, area_toc->track_end - 1);
            verification_errors++;
        }
        else
        {
            fwprintf(stdout, L"✓ Area %d end sector readable\n", i);
        }
        
        // Test track boundaries (first and last track)
        if (area_toc->track_count > 0)
        {
            // First track
            uint32_t first_track_start = tracklist->track_start_lsn[0];
            if (sacd_read_block_raw(handle->sacd, first_track_start, 1, sector_buffer) \!= 1)
            {
                fwprintf(stdout, L"ERROR: Cannot read first track start sector %u\n", first_track_start);
                verification_errors++;
            }
            
            // Last track
            int last_track_idx = area_toc->track_count - 1;
            uint32_t last_track_end = tracklist->track_start_lsn[last_track_idx] + 
                                      tracklist->track_length_lsn[last_track_idx] - 1;
            if (sacd_read_block_raw(handle->sacd, last_track_end, 1, sector_buffer) \!= 1)
            {
                fwprintf(stdout, L"ERROR: Cannot read last track end sector %u\n", last_track_end);
                verification_errors++;
            }
            else
            {
                fwprintf(stdout, L"✓ Track boundary sectors readable\n");
            }
        }
    }

    // Summary
    fwprintf(stdout, L"\n=== Verification Summary ===\n");
    if (verification_errors == 0)
    {
        fwprintf(stdout, L"✓ ScarletBook verification PASSED - no errors detected\n");
    }
    else
    {
        fwprintf(stdout, L"✗ ScarletBook verification FAILED - %d error(s) detected\n", verification_errors);
    }
    fwprintf(stdout, L"===========================\n\n");
    
    return verification_errors;
}
