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
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <locale.h>
#include <time.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif

#include <pthread.h>

#include <charset.h>
#include <logging.h>

#include "getopt.h"

#include <sacd_reader.h>
#include <scarletbook.h>
#include <scarletbook_read.h>
#include <scarletbook_output.h>
#include <scarletbook_print.h>
#include <scarletbook_helpers.h>
#include <scarletbook_id3.h>
#include <cuesheet.h>
#include <endianess.h>
#include <fileutils.h>
#include <utils.h>
#include <yarn.h>
#include <version.h>

#ifdef _WIN32
#define CHAR2WCHAR(dst, src) dst = (wchar_t *) charset_convert(src, strlen(src), "UTF-8", sizeof(wchar_t) == 2 ? "UCS-2-INTERNAL" : "UCS-4-INTERNAL")
#else
#define CHAR2WCHAR(dst, src) dst = (wchar_t *) charset_convert(src, strlen(src), "UTF-8", "WCHAR_T")
#endif

static struct opts_s
{
    int            two_channel;
    int            multi_channel;
    int            output_dsf;
    int            output_dsdiff_em;
    int            output_dsdiff;
    int            output_iso;
    int            concurrent;
    int            convert_dst;
    int            export_cue_sheet;
    int            print;
    char          *input_device; /* Access method driver should use for control */
    char           output_file[512];
    char          *output_dir;
    char          *output_dir_conc;
    int            select_tracks;
    char           selected_tracks[256]; /* scarletbook is limited to 256 tracks */
    int            dsf_nopad; 
    int            version;
} opts;

scarletbook_handle_t *handle;
scarletbook_output_t *output;

void mkdir_wrap(char *name, mode_t mode){
#ifdef __MINGW32__
    wchar_t *wname;
    wname = (wchar_t *) charset_convert(name, strlen(name), "UTF-8", "UCS-2-INTERNAL");
    _wmkdir(wname);
    free(wname);
#else
    mkdir(name, mode);
#endif
}

/* Parse all options. */
static int parse_options(int argc, char *argv[]) 
{
    int opt; /* used for argument parsing */
    char *program_name = NULL;

    static const char help_text[] =
        "Usage: %s [options] [outfile]\n"
        "  -2, --2ch-tracks                : Export two channel tracks (default)\n"
        "  -m, --mch-tracks                : Export multi-channel tracks\n"
        "  -e, --output-dsdiff-em          : output as Philips DSDIFF (Edit Master) file\n"
        "  -p, --output-dsdiff             : output as Philips DSDIFF file\n"
        "  -s, --output-dsf                : output as Sony DSF file\n"
        "  -z, --dsf-nopad                 : Do not zero pad DSF (cannot be used with -t)\n"
        "  -t, --select-track              : only output selected track(s) (ex. -t 1,5,13)\n"
        "  -I, --output-iso                : output as RAW ISO\n"
#ifndef SECTOR_LIMIT
        "  -w, --concurrent                : Concurrent ISO+DSF/DSDIFF processing mode\n"
#endif
        "  -c, --convert-dst               : convert DST to DSD\n"
        "  -C, --export-cue                : Export a CUE Sheet\n"
        "  -i, --input[=FILE]              : set source and determine if \"iso\" image, \n"
        "                                    device or server (ex. -i 192.168.1.10:2002)\n"
        "  -o, --output-dir[=DIR]          : Output directory (ISO output dir for concurrent processing mode)\n"
        "  -y, --output-dir-conc[=DIR]     : DSF/DSDIFF Output directory for concurrent processing mode\n"
        "  -P, --print                     : display disc and track information\n" 
        "  -v, --version                   : Display version\n"
        "\n"
        "Help options:\n"
        "  -?, --help                      : Show this help message\n"
        "  --usage                         : Display brief usage message\n";

    static const char usage_text[] = 
        "Usage: %s [-2|--2ch-tracks] [-m|--mch-tracks] [-p|--output-dsdiff]\n"
#ifdef SECTOR_LIMIT
        "        [-e|--output-dsdiff-em] [-s|--output-dsf] [-z|--dsf-nopad] [-I|--output-iso]\n"
#else
        "        [-e|--output-dsdiff-em] [-s|--output-dsf] [-z|--dsf-nopad] [-I|--output-iso] [-w|--concurrent]\n"
#endif
        "        [-c|--convert-dst] [-C|--export-cue] [-i|--input FILE] [-o|--output-dir DIR] [-y|--output-dir-conc DIR] [-P|--print]\n"
        "        [-?|--help] [--usage]\n";
#ifdef SECTOR_LIMIT
    static const char options_string[] = "2mepszIcCvi:o:y:t:P?";
#else
    static const char options_string[] = "2mepszIwcCvi:o:y:t:P?";
#endif
    static const struct option options_table[] = {
        {"2ch-tracks", no_argument, NULL, '2' },
        {"mch-tracks", no_argument, NULL, 'm' },
        {"output-dsdiff-em", no_argument, NULL, 'e'}, 
        {"output-dsdiff", no_argument, NULL, 'p'}, 
        {"output-dsf", no_argument, NULL, 's'}, 
        {"dsf-nopad", no_argument, NULL, 'z'}, 
        {"output-iso", no_argument, NULL, 'I'}, 
#ifndef SECTOR_LIMIT
        {"concurrent", no_argument, NULL, 'w'}, 
#endif
        {"convert-dst", no_argument, NULL, 'c'}, 
        {"export-cue", no_argument, NULL, 'C'}, 
        {"version", no_argument, NULL, 'v'},
        {"input", required_argument, NULL, 'i' },
        {"output-dir", required_argument, NULL, 'o' },
        {"output-dir-conc", required_argument, NULL, 'y' },
        {"print", no_argument, NULL, 'P' },

        {"help", no_argument, NULL, '?' },
        {"usage", no_argument, NULL, 'u' },
        { NULL, 0, NULL, 0 }
    };

    program_name = strrchr(argv[0],'/');
    program_name = program_name ? strdup(program_name+1) : strdup(argv[0]);

    while ((opt = getopt_long(argc, argv, options_string, options_table, NULL)) >= 0) {
        switch (opt) {
        case '2': 
            opts.two_channel = 1; 
            break;
        case 'm': 
            opts.multi_channel = 1; 
            break;
        case 'e': 
            opts.output_dsdiff_em = 1;
            opts.output_dsdiff = 0;
            opts.output_dsf = 0; 
            opts.output_iso = 0;
            opts.export_cue_sheet = 1;
            break;
        case 'p': 
            opts.output_dsdiff_em = 0; 
            opts.output_dsdiff = 1; 
            opts.output_dsf = 0; 
            break;
        case 's': 
            opts.output_dsdiff_em = 0; 
            opts.output_dsdiff = 0; 
            opts.output_dsf = 1; 
            break;
        case 't': 
            {
                int track_nr, count = 0;
                char *track = strtok(optarg, " ,");
                while (track != 0)
                {
                    track_nr = atoi(track);
                    track = strtok(0, " ,");
                    if (!track_nr)
                        continue;
                    track_nr = (track_nr - 1) & 0xff;
                    opts.selected_tracks[track_nr] = 1;
                    count++;
                }
                opts.select_tracks = count != 0;
            }
            break;
        case 'z':
            opts.dsf_nopad = 1;
            break;
        case 'I': 
            opts.output_dsdiff_em = 0; 
            opts.output_iso = 1;
            break;
        case 'w':
            opts.concurrent = 1;
            break;
        case 'c': opts.convert_dst = 1; break;
        case 'C': opts.export_cue_sheet = 1; break;
        case 'i': opts.input_device = strdup(optarg); break;
        case 'o': opts.output_dir = strdup(optarg); break;
        case 'y': opts.output_dir_conc = strdup(optarg); break;
        case 'P': opts.print = 1; break;
        case 'v': opts.version = 1; break;
        case '?':
            fprintf(stdout, help_text, program_name);
            free(program_name);
            return 0;
            break;

        case 'u':
            fprintf(stderr, usage_text, program_name);
            free(program_name);
            return 0;
            break;
        }
    }

    if (optind < argc) {
        const char *remaining_arg = argv[optind++];
        strcpy(opts.output_file, remaining_arg);
    }

    return 1;
}

static lock *g_fwprintf_lock = 0;

static int safe_fwprintf(FILE *stream, const wchar_t *format, ...)
{
    int retval;
    va_list arglist;

    possess(g_fwprintf_lock);

    va_start(arglist, format);
    retval = vfwprintf(stream, format, arglist);
    va_end(arglist);

    fflush(stream);

    release(g_fwprintf_lock);

    return retval;
}

static void handle_sigint(int sig_no)
{
    safe_fwprintf(stdout, L"\rUser interrupted..                                                      \n");
    scarletbook_output_interrupt(output);
}

static void handle_status_update_track_callback(char *filename, int current_track, int total_tracks, int dst_decomp)
{
    wchar_t *str_decomp;
#ifdef _WIN32
    wchar_t *wide_filename = (wchar_t *) charset_convert(filename, strlen(filename), "UTF-8", sizeof(wchar_t) == 2 ? "UCS-2-INTERNAL" : "UCS-4-INTERNAL");
#else
    wchar_t *wide_filename = (wchar_t *) charset_convert(filename, strlen(filename), "UTF-8", "WCHAR_T");
#endif

    CHAR2WCHAR(str_decomp, dst_decomp ? "(d) " : "");
    safe_fwprintf(stdout, L"\rProcessing %ls[%ls] (%d/%d)..\n", str_decomp, wide_filename, current_track, total_tracks);
    free(wide_filename);
    free(str_decomp);
}

static time_t started_processing;

static void handle_status_update_progress_callback(uint32_t stats_total_sectors, uint32_t stats_total_sectors_processed,
                                 uint32_t stats_current_file_total_sectors, uint32_t stats_current_file_sectors_processed,
                                 uint32_t stats_iso_total_sectors, uint32_t stats_iso_sectors_processed,
                                 uint32_t stats_dsf_total_sectors, uint32_t stats_dsf_sectors_processed,
                                 int stats_dsf_tracks_completed, int stats_dsf_total_tracks,
                                 char *current_track_name, int current_track_number, int is_iso_processing)
{
    float total_mb = (float)((double) stats_total_sectors_processed * SACD_LSN_SIZE / 1048576.00);
    float speed = total_mb / (float)(time(0) - started_processing);
    
    // Calculate current track/file progress
    int current_percentage = stats_current_file_total_sectors > 0 ? 
        (stats_current_file_sectors_processed * 100 / stats_current_file_total_sectors) : 0;
    int total_percentage = stats_total_sectors > 0 ? 
        (stats_total_sectors_processed * 100 / stats_total_sectors) : 0;
    
    float current_mb = (float)((double) stats_current_file_sectors_processed * SACD_LSN_SIZE / 1048576.00);
    
    // Show appropriate progress based on processing type and completion status
    if (is_iso_processing)
    {
        safe_fwprintf(stdout, L"\rISO: %d%% (%.1fMB), Total: %d%% (%.1fMB) at %.2fMB/sec\033[K",
                     current_percentage, current_mb, total_percentage, total_mb, speed);
    }
    else
    {
        // Get the actual display track number first
        int display_track = (current_track_number > 0) ? current_track_number : 1;
        
        // Check if DSF tracks are completed or if we're on the final track and it's at/near completion
        int dsf_completed = (stats_dsf_tracks_completed >= stats_dsf_total_tracks && stats_dsf_total_tracks > 0);
        int final_track_near_complete = (display_track == stats_dsf_total_tracks && 
                                        current_percentage >= 100 && stats_dsf_total_tracks > 0);
        
        if (dsf_completed || final_track_near_complete)
        {
            // DSF tracks completed or final track finished, show ISO-only progress
            int iso_percentage = stats_iso_total_sectors > 0 ? 
                (stats_iso_sectors_processed * 100 / stats_iso_total_sectors) : 0;
            float iso_mb = (float)((double) stats_iso_sectors_processed * SACD_LSN_SIZE / 1048576.00);
            
            safe_fwprintf(stdout, L"\rISO: %d%% (%.1fMB), Total: %d%% (%.1fMB) at %.2fMB/sec\033[K",
                         iso_percentage, iso_mb, total_percentage, total_mb, speed);
        }
        else
        {
            // Normal track processing
            safe_fwprintf(stdout, L"\rTrack %d: %d%% (%.1fMB), Total: %d%% (%.1fMB) at %.2fMB/sec\033[K",
                         display_track, current_percentage, current_mb, total_percentage, total_mb, speed);
        }
    }
}

/* Initialize global variables. */
static void init(void) 
{
    /* Default option values. */
    opts.two_channel        = 0;
    opts.multi_channel      = 0;
    opts.output_dsf         = 0;
    opts.output_iso         = 0;
    opts.output_dir         = 0;
    opts.output_dir_conc    = 0;
    opts.concurrent         = 0;
    opts.output_dsdiff      = 0;
    opts.output_dsdiff_em   = 0;
    opts.convert_dst        = 0;
    opts.export_cue_sheet   = 0;
    opts.print              = 0;
    opts.input_device       = "/dev/cdrom";
    opts.dsf_nopad              = 0;

#ifdef _WIN32
    signal(SIGINT, handle_sigint);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handle_sigint;
    sigaction(SIGINT, &sa, NULL);
#endif

    init_logging();
    g_fwprintf_lock = new_lock(0);
}

static uint32_t get_selected_tracks_sectors(scarletbook_handle_t *handle, int area_idx_val)
{
    uint32_t total_sectors = 0;
    area_tracklist_offset_t *tracklist = handle->area[area_idx_val].area_tracklist_offset;
    area_toc_t *toc = handle->area[area_idx_val].area_toc;
    
    if (!tracklist || !toc)
        return 0;
    
    for (int i = 0; i < toc->track_count; i++)
    {
        if (opts.select_tracks && opts.selected_tracks[i] == 0)
            continue;
            
        uint32_t track_sectors;
        if (i < toc->track_count - 1)
        {
            track_sectors = tracklist->track_start_lsn[i + 1] - tracklist->track_start_lsn[i];
        }
        else
        {
            track_sectors = toc->track_end - tracklist->track_start_lsn[i];
        }
        total_sectors += track_sectors;
    }
    
    return total_sectors;
}

static void format_track_list_message(char *buffer, size_t buffer_size)
{
    buffer[0] = '\0';
    int first = 1;
    int count = 0;
    
    for (int i = 0; i < 256; i++)
    {
        if (opts.selected_tracks[i])
        {
            count++;
            if (first)
            {
                snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%d", i + 1);
                first = 0;
            }
            else if (count <= 3)
            {
                snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), ",%d", i + 1);
            }
            else if (count == 4)
            {
                snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "...");
                break;
            }
        }
    }
    
    if (count == 1)
    {
        for (int i = 0; i < 256; i++)
        {
            if (opts.selected_tracks[i])
            {
                snprintf(buffer, buffer_size, "track %d", i + 1);
                break;
            }
        }
    }
    else if (count > 1)
    {
        char temp[256];
        strcpy(temp, buffer);
        snprintf(buffer, buffer_size, "tracks %s", temp);
    }
}

int main(int argc, char* argv[]) 
{
    char *albumdir = 0, *musicfilename, *file_path = 0;
    wchar_t *s_wchar;
    int i, j, area_idx[8];
    int n_areas = 0;
    int nogo = 0;
    sacd_reader_t *sacd_reader;

#ifdef PTW32_STATIC_LIB
    pthread_win32_process_attach_np();
    pthread_win32_thread_attach_np();
#endif

    init();
    if (parse_options(argc, argv)) 
    {
        setlocale(LC_ALL, "");
        if (fwide(stdout, 1) < 0)
        {
            fprintf(stderr, "ERROR: Output not set to wide.\n");
        }

        if(opts.version){
            fwprintf(stdout, L"sacd_extract version " SACD_RIPPER_VERSION_STRING "\n");
            fwprintf(stdout, L"git repository: " SACD_RIPPER_REPO "\n");
            nogo = 1;
        }

        // default to 2 channel
        if (opts.two_channel == 0 && opts.multi_channel == 0) 
        {
            opts.two_channel = 1;
        }

        if (opts.output_dir){
            struct stat sb;
            if(stat(opts.output_dir, &sb) != 0 || !S_ISDIR(sb.st_mode)){
                fprintf(stderr, "%s doesn't exist or is not a directory.\n", opts.output_dir);
                nogo = 1;
            }
        }

        if (opts.output_dir_conc && opts.concurrent && (opts.output_dsf || opts.output_dsdiff)){
            struct stat sb;
            if(stat(opts.output_dir_conc, &sb) != 0 || !S_ISDIR(sb.st_mode)){
                fprintf(stderr, "%s doesn't exist or is not a directory.\n", opts.output_dir_conc);
                nogo = 1;
            }
        }

        if(!nogo){
            nogo = !(sacd_reader = sacd_open(opts.input_device)) ;
        }

        if (!nogo) 
        {

            handle = scarletbook_open(sacd_reader, 0);
            if (handle)
            {
                if (opts.print)
                {
                    scarletbook_print(handle);
                }

                if (opts.output_dsf || opts.output_iso || opts.output_dsdiff || opts.output_dsdiff_em || opts.export_cue_sheet)
                {
                    output = scarletbook_output_create(handle, handle_status_update_track_callback, handle_status_update_progress_callback, safe_fwprintf);

                    // Check for sector count discrepancies against Track_Area_End_Address
                    // Per Super Audio CD System Description (Scarlet Book) Part 2 Section 3.2.1.2.14
                    uint32_t total_sectors = sacd_get_total_sectors(sacd_reader);
                    uint32_t content_end = get_content_end_lsn(handle);
                    if (content_end > 0 && content_end > total_sectors) {
                        // Server reports smaller size than Track_Area_End_Address requires
                        safe_fwprintf(stdout, L"Warning: source reporting incorrect sector count (%.1fMB vs %.1fMB Track_Area_End_Address)\n", 
                                     (float)((double) total_sectors * SACD_LSN_SIZE / 1048576.00),
                                     (float)((double) content_end * SACD_LSN_SIZE / 1048576.00));
                    }

                    // select the channel area
                    if(has_two_channel(handle) && opts.two_channel){
                        area_idx[n_areas ++] = handle->twoch_area_idx;
                    }
                    if(has_multi_channel(handle) && opts.multi_channel){
                        area_idx[n_areas ++] = handle->mulch_area_idx;
                    }

                    albumdir = (strlen(opts.output_file) > 0 ? strdup(opts.output_file) : get_album_dir(handle));

                    if(opts.export_cue_sheet){
                        char *albumdir_loc;
                        albumdir_loc = (char *)malloc(strlen(albumdir)+16);

                        for(j = 0; j < n_areas; j ++){
                            wchar_t *wide_filename;
                            strcpy(albumdir_loc, albumdir);
                            if(n_areas > 1){
                                strcat(albumdir_loc, j ? " [multi]" : " [stereo]");
                            }

                            file_path = get_unique_path(opts.output_dir, albumdir_loc, "cue");
                            CHAR2WCHAR(wide_filename, file_path);
                            safe_fwprintf(stdout, L"Exporting CUE sheet [%ls]\n", wide_filename);
                            write_cue_sheet(handle, file_path, area_idx[j], file_path);
                            free(file_path);
                            free(wide_filename);
                        }
                        free(albumdir_loc);
                    }

                    if (opts.output_iso)
                    {
                        uint32_t total_sectors = sacd_get_total_sectors(sacd_reader);
                        uint32_t content_end = get_content_end_lsn(handle);
                        uint32_t extract_sectors = total_sectors;
                        
                        // Use corrected sector count if Track_Area_End_Address extends beyond server-reported size
                        // Per Super Audio CD System Description (Scarlet Book) Part 2 Section 3.2.1.2.14:
                        // Track_Area_End_Address contains the LSN of the last sector in the Track Area
                        // Note: content_end is Track_Area_End_Address (LSN), convert to sector count (LSN + 1)
                        if (content_end > 0 && content_end >= total_sectors) {
                            extract_sectors = content_end + 1;  // Convert Track_Area_End_Address to sector count
                        }
                        
                        float extract_mb = (float)((double) extract_sectors * SACD_LSN_SIZE / 1048576.00);
                        safe_fwprintf(stdout, L"Extracting %.1fMB (full disc) to ISO\n", extract_mb);
                        
                        total_sectors = extract_sectors;
#ifdef SECTOR_LIMIT
#define FAT32_SECTOR_LIMIT 2090000
                        uint32_t sector_size = FAT32_SECTOR_LIMIT;
                        uint32_t sector_offset = 0;
                        if (extract_sectors > FAT32_SECTOR_LIMIT)
                        {
                            musicfilename = (char *) malloc(512);
                            file_path = make_filename(opts.output_dir, 0, albumdir, "iso");
                            for (i = 1; extract_sectors != 0; i++)
                            {
                                sector_size = min(extract_sectors, FAT32_SECTOR_LIMIT);
                                snprintf(musicfilename, 512, "%s.%03d", file_path, i);
                                scarletbook_output_enqueue_raw_sectors(output, sector_offset, sector_size, musicfilename, "iso");
                                sector_offset += sector_size;
                                extract_sectors -= sector_size;
                            }
                            free(musicfilename);
                        }
                        else
#endif
                        {
                            file_path = get_unique_path(opts.output_dir, albumdir, "iso");
                            scarletbook_output_enqueue_raw_sectors(output, 0, extract_sectors, file_path, "iso");


                            // Concurrent iso+dsf/dsdiff generation
                            if(opts.concurrent && (opts.output_dsf || opts.output_dsdiff)){
                                safe_fwprintf(stdout, L"Concurrent mode enabled.\n");
                                CHAR2WCHAR(s_wchar, file_path);
                                safe_fwprintf(stdout, L"ISO output: %ls\n", s_wchar);
                                free(s_wchar);
                                free(file_path);

                                // Show area-specific extraction info when extracting specific areas
                                if (n_areas > 0)
                                {
                                    for (j = 0; j < n_areas; j++)
                                    {
                                        int area_idx_val = area_idx[j];
                                        char *area_type = (area_idx_val == handle->twoch_area_idx) ? "stereo" : "multi-channel";
                                        
                                        uint32_t area_sectors;
                                        char track_info[256] = "";
                                        
                                        if (opts.select_tracks)
                                        {
                                            area_sectors = get_selected_tracks_sectors(handle, area_idx_val);
                                            format_track_list_message(track_info, sizeof(track_info));
                                        }
                                        else
                                        {
                                            area_sectors = get_area_sectors(handle, area_idx_val);
                                            strcpy(track_info, "all tracks");
                                        }
                                        
                                        float area_mb = (float)((double) area_sectors * SACD_LSN_SIZE / 1048576.00);
                                        
                                        if (strlen(track_info) > 0)
                                        {
                                            safe_fwprintf(stdout, L"Extracting %.1fMB of %hs content (%hs).\n", area_mb, area_type, track_info);
                                        }
                                        else
                                        {
                                            safe_fwprintf(stdout, L"Extracting %.1fMB of %hs content.\n", area_mb, area_type);
                                        }
                                    }
                                }

                                char *albumdir_loc;
                                albumdir_loc = (char *)malloc(strlen(albumdir)+16);
                                // fill the sub queue with items to rip
                                for (j = 0; j < n_areas; j ++){
                                    // create the output folder
                                    // If both stereo and multi-ch tracks are getting processed, create separate directories
                                    strcpy(albumdir_loc, albumdir);
                                    if(n_areas > 1){
                                        strcat(albumdir_loc, j ? " [multi]" : " [stereo]");
                                    }

                                    get_unique_dir(opts.output_dir_conc, &albumdir_loc);
                                    mkdir_wrap(albumdir_loc, 0774);
                                    if(opts.output_dsf){
                                        CHAR2WCHAR(s_wchar, albumdir_loc);
                                        safe_fwprintf(stdout, L"DSF output: %ls\n", s_wchar);
                                        free(s_wchar);
                                    }
                                    else{
                                        CHAR2WCHAR(s_wchar, albumdir_loc);
                                        safe_fwprintf(stdout, L"DSDIFF output: %ls\n", s_wchar);
                                        free(s_wchar);
                                    }

                                    for (i = 0; i < handle->area[area_idx[j]].area_toc->track_count; i++) 
                                    {
                                        if (opts.select_tracks && opts.selected_tracks[i] == 0)
                                            continue;

                                        musicfilename = get_music_filename(handle, area_idx[j], i, opts.output_file);

                                        if (opts.output_dsf)
                                        {
                                            file_path = make_filename(albumdir_loc, 0, musicfilename, "dsf");
                                            scarletbook_output_enqueue_track(output, area_idx[j], i, file_path, "dsf", 
                                                1 /* always decode to DSD */, opts.dsf_nopad && !opts.select_tracks, 1);
                                        }
                                        else if (opts.output_dsdiff)
                                        {
                                            file_path = make_filename(albumdir_loc, 0, musicfilename, "dff");
                                            scarletbook_output_enqueue_track(output, area_idx[j], i, file_path, "dsdiff", 
                                                (opts.convert_dst ? 1 : handle->area[area_idx[j]].area_toc->frame_format != FRAME_FORMAT_DST), 0, 1);
                                        }
                                        free(musicfilename);
                                        free(file_path);
                                    }
                                }
                                free(albumdir_loc);
                            }
                            else{
                                free(file_path);
                            }
                        }
                    }
                    else if (opts.output_dsdiff_em)
                    {
                        char *albumdir_loc;
                        albumdir_loc = (char *)malloc(strlen(albumdir)+16);

                        for(j = 0; j < n_areas; j ++){
                            strcpy(albumdir_loc, albumdir);
                            if(n_areas > 1){
                                strcat(albumdir_loc, j ? " [multi]" : " [stereo]");
                            }

                            file_path = get_unique_path(opts.output_dir, albumdir_loc, "dff");
                            scarletbook_output_enqueue_track(output, area_idx[j], 0, file_path, "dsdiff_edit_master",
                                (opts.convert_dst ? 1 : handle->area[area_idx[j]].area_toc->frame_format != FRAME_FORMAT_DST), 0, 0);
                            free(file_path);
                        }
                        free(albumdir_loc);
                    }

                    // Non-concurrent dsf/dsdiff generation
                    else if (!(opts.output_iso && opts.concurrent) && (opts.output_dsf || opts.output_dsdiff))
                    {
                        // Show area-specific extraction info when extracting specific areas
                        if (n_areas > 0)
                        {
                            for (j = 0; j < n_areas; j++)
                            {
                                int area_idx_val = area_idx[j];
                                char *area_type = (area_idx_val == handle->twoch_area_idx) ? "stereo" : "multi-channel";
                                
                                uint32_t area_sectors;
                                char track_info[256] = "";
                                
                                if (opts.select_tracks)
                                {
                                    area_sectors = get_selected_tracks_sectors(handle, area_idx_val);
                                    format_track_list_message(track_info, sizeof(track_info));
                                }
                                else
                                {
                                    area_sectors = get_area_sectors(handle, area_idx_val);
                                    strcpy(track_info, "all tracks");
                                }
                                
                                float area_mb = (float)((double) area_sectors * SACD_LSN_SIZE / 1048576.00);
                                
                                if (strlen(track_info) > 0)
                                {
                                    safe_fwprintf(stdout, L"Extracting %.1fMB of %hs content (%hs).\n", area_mb, area_type, track_info);
                                }
                                else
                                {
                                    safe_fwprintf(stdout, L"Extracting %.1fMB of %hs content.\n", area_mb, area_type);
                                }
                            }
                        }
                        
                        char *albumdir_loc;
                        albumdir_loc = (char *)malloc(strlen(albumdir)+16);

                        for(j = 0; j < n_areas; j ++){
                            // create the output folder
                            // If both stereo and multi-ch tracks are getting processed, create separate directories
                            strcpy(albumdir_loc, albumdir);
                            if(n_areas > 1){
                                strcat(albumdir_loc, j ? " [multi]" : " [stereo]");
                            }

                            get_unique_dir(opts.output_dir, &albumdir_loc);
                            mkdir_wrap(albumdir_loc, 0774);

                            if(opts.output_dsf){
                                CHAR2WCHAR(s_wchar, albumdir_loc);
                                safe_fwprintf(stdout, L"DSF output: %ls\n", s_wchar);
                                free(s_wchar);
                            }
                            else{
                                CHAR2WCHAR(s_wchar, albumdir_loc);
                                safe_fwprintf(stdout, L"DSDIFF output: %ls\n", s_wchar);
                                free(s_wchar);
                            }

                            // fill the queue with items to rip
                            for (i = 0; i < handle->area[area_idx[j]].area_toc->track_count; i++) 
                            {
                                if (opts.select_tracks && opts.selected_tracks[i] == 0)
                                    continue;

                                musicfilename = get_music_filename(handle, area_idx[j], i, opts.output_file);

                                if (opts.output_dsf)
                                {
                                    file_path = make_filename(albumdir_loc, 0, musicfilename, "dsf");
                                    scarletbook_output_enqueue_track(output, area_idx[j], i, file_path, "dsf", 
                                        1 /* always decode to DSD */, opts.dsf_nopad && !opts.select_tracks, 0);
                                }
                                else if (opts.output_dsdiff)
                                {
                                    file_path = make_filename(albumdir_loc, 0, musicfilename, "dff");
                                    scarletbook_output_enqueue_track(output, area_idx[j], i, file_path, "dsdiff", 
                                        (opts.convert_dst ? 1 : handle->area[area_idx[j]].area_toc->frame_format != FRAME_FORMAT_DST), 0, 0);
                                }

                                free(musicfilename);
                                free(file_path);
                            }
                        }
                        free(albumdir_loc);
                    }
                    safe_fwprintf(stdout, L"\n");

                    started_processing = time(0);
                    scarletbook_output_start(output);
                    scarletbook_output_destroy(output);

                    fprintf(stdout, "\rWe are done..                                                          \n");
                }
                scarletbook_close(handle);

                free(albumdir);
            }
            sacd_close(sacd_reader);
        }

#ifndef _WIN32
        if (freopen(0, "w", stdout) == NULL) {
            LOG(lm_main, LOG_NOTICE, ("Warning: Failed to redirect stdout"));
        }
#endif
        if (fwide(stdout, -1) >= 0)
        {
            fprintf(stderr, "ERROR: Output not set to byte oriented.\n");
        }
    }

    free_lock(g_fwprintf_lock);
    destroy_logging();

#ifdef PTW32_STATIC_LIB
    pthread_win32_process_detach_np();
    pthread_win32_thread_detach_np();
#endif

    printf("\n");
    return 0;
}
