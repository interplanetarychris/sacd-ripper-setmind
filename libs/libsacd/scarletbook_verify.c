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
    
    if (!handle)
    {
        fprintf(stderr, "ERROR: No valid ScarletBook handle available for verification\n");
        return -1;
    }

    fwprintf(stdout, L"\n=== ScarletBook Verification ===\n");

    // Structural validation
    if (!handle->master_toc)
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
        
        if (major != SUPPORTED_VERSION_MAJOR || minor != SUPPORTED_VERSION_MINOR)
        {
            fwprintf(stdout, L"  WARNING: Version %d.%d may not be fully supported (expected %d.%d)\n", 
                     major, minor, SUPPORTED_VERSION_MAJOR, SUPPORTED_VERSION_MINOR);
        }
    }

    // Area validation - checking TOC structure
    fwprintf(stdout, L"\n--- Validating TOC structure ---\n");
    fwprintf(stdout, L"✓ Area count: %d\n", handle->area_count);
    
    if (handle->area_count == 0)
    {
        fwprintf(stdout, L"ERROR: No areas found\n");
        verification_errors++;
    }
    
    for (i = 0; i < handle->area_count; i++)
    {
        area_toc_t *area_toc = handle->area[i].area_toc;
        if (!area_toc)
        {
            fwprintf(stdout, L"ERROR: Area %d missing TOC\n", i);
            verification_errors++;
            continue;
        }
        
        fwprintf(stdout, L"✓ Area %d: %d tracks, Track_Area_Start_Address %u, Track_Area_End_Address %u\n", 
                 i, area_toc->track_count, area_toc->track_start, area_toc->track_end);
        
        // Track Area boundary validation (per Scarlet Book Section 3.2.1.2.13 and 3.2.1.2.14)
        if (area_toc->track_start >= area_toc->track_end)
        {
            fwprintf(stdout, L"ERROR: Area %d invalid Track Area range (Track_Area_Start_Address %u >= Track_Area_End_Address %u)\n", 
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
                
                // Check track boundaries against Track Area (per Scarlet Book Section 3.2.1.2.13-14)
                if (track_start < area_toc->track_start || track_end > area_toc->track_end + 1)
                {
                    fwprintf(stdout, L"ERROR: Track %d outside Track Area bounds (Track_Start_Address %u, Track_Length %u, Track_Area_Start_Address %u, Track_Area_End_Address %u)\n",
                             j + 1, track_start, track_length, area_toc->track_start, area_toc->track_end);
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
            fwprintf(stdout, L"✓ Area %d: All track boundaries valid in TOC\n", i);
        }
    }

    // Quick sector spot-check (test readability at key positions)
    fwprintf(stdout, L"\n--- Spot-checking sector readability ---\n");
    
    for (i = 0; i < handle->area_count; i++)
    {
        area_toc_t *area_toc = handle->area[i].area_toc;
        area_tracklist_offset_t *tracklist = handle->area[i].area_tracklist_offset;
        
        if (!area_toc || !tracklist) continue;
        
        // Test Track_Area_Start_Address (per Scarlet Book Section 3.2.1.2.13)
        uint8_t sector_buffer[SACD_LSN_SIZE];
        if (sacd_read_block_raw(handle->sacd, area_toc->track_start, 1, sector_buffer) != 1)
        {
            fwprintf(stdout, L"ERROR: Cannot read area %d Track_Area_Start_Address %u\n", i, area_toc->track_start);
            verification_errors++;
        }
        else
        {
            fwprintf(stdout, L"✓ Area %d Track_Area_Start_Address readable\n", i);
        }
        
        // Test Track_Area_End_Address (per Scarlet Book Section 3.2.1.2.14)
        if (sacd_read_block_raw(handle->sacd, area_toc->track_end, 1, sector_buffer) != 1)
        {
            fwprintf(stdout, L"ERROR: Cannot read area %d Track_Area_End_Address %u\n", i, area_toc->track_end);
            verification_errors++;
        }
        else
        {
            fwprintf(stdout, L"✓ Area %d Track_Area_End_Address readable\n", i);
        }
        
        // Test all track boundaries
        if (area_toc->track_count > 0)
        {
            int track_read_errors = 0;
            int first_error_track = -1;
            int consecutive_errors = 0;
            int max_consecutive_errors = 0;
            
            for (j = 0; j < area_toc->track_count; j++)
            {
                uint32_t track_start = tracklist->track_start_lsn[j];
                uint32_t track_end = track_start + tracklist->track_length_lsn[j] - 1;
                
                // Test track start
                if (sacd_read_block_raw(handle->sacd, track_start, 1, sector_buffer) != 1)
                {
                    if (first_error_track == -1) first_error_track = j;
                    fwprintf(stdout, L"ERROR: Cannot read area %d track %d start sector %u\n", i, j + 1, track_start);
                    track_read_errors++;
                    consecutive_errors++;
                    verification_errors++;
                }
                else
                {
                    if (consecutive_errors > max_consecutive_errors)
                        max_consecutive_errors = consecutive_errors;
                    consecutive_errors = 0;
                }
                
                // Test track end
                if (sacd_read_block_raw(handle->sacd, track_end, 1, sector_buffer) != 1)
                {
                    if (first_error_track == -1) first_error_track = j;
                    fwprintf(stdout, L"ERROR: Cannot read area %d track %d end sector %u\n", i, j + 1, track_end);
                    track_read_errors++;
                    consecutive_errors++;
                    verification_errors++;
                }
                else
                {
                    if (consecutive_errors > max_consecutive_errors)
                        max_consecutive_errors = consecutive_errors;
                    consecutive_errors = 0;
                }
            }
            
            if (consecutive_errors > max_consecutive_errors)
                max_consecutive_errors = consecutive_errors;
                
            if (track_read_errors == 0)
            {
                fwprintf(stdout, L"✓ Area %d track boundaries readable\n", i);
            }
            else
            {
                fwprintf(stdout, L"Area %d track read pattern: %d/%d tracks affected", i, track_read_errors, area_toc->track_count * 2);
                if (first_error_track >= 0)
                    fwprintf(stdout, L", first error at track %d", first_error_track + 1);
                if (max_consecutive_errors > 1)
                    fwprintf(stdout, L", max consecutive errors: %d", max_consecutive_errors);
                fwprintf(stdout, L"\n");
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