# SACD Ripper 69% Stalling Bug Analysis and Fix

## Task ID
BUG-69PercentStalling

## Problem Statement
The SACD ripper frequently stalls during DSF and ISO extraction processes, typically in the 65-90% completion range (observed at 69% and 85% marks on different discs). The application becomes unresponsive with no progress indication, and the data rate average slowly drops to zero. This occurs particularly when extracting stereo content to DSF format, suggesting the issue may be related to the end of stereo content, transitions between disc areas, or variable disc structures across different SACD releases.

## Proposed Implementation
Implement improved error handling and progress reporting in the processing thread to:
1. Distinguish between expected EOF (normal completion) and unexpected read failures
2. Provide informative error messages showing exact stall percentage and potential causes
3. Add proper handling for area transitions and partial block reads
4. Implement more robust progress calculation that accounts for SACD disc structure

### Phase 1 (Immediate Fix - Completed)
- Enhanced error reporting in `scarletbook_output.c` processing loop
- Clear distinction between normal completion vs actual errors
- User-friendly progress percentage display at stall point

### Phase 2 (Comprehensive Solution)
- Implement area transition handling for SACD disc structure
- Add partial block recovery mechanisms with retry logic
- Refine progress calculation for multi-area operations
- Add timeout mechanisms for hanging reads

## Components Involved
- **Core Processing**: `libs/libsacd/scarletbook_output.c` - Main processing thread
- **Disc I/O**: `libs/libsacd/sacd_input.c` - Low-level sector reading
- **Reader Interface**: `libs/libsacd/sacd_reader.c` - High-level read operations
- **Progress Tracking**: `src/ripping.c` - User interface progress reporting
- **SACD Structure**: Area transitions, sector boundaries, encryption regions

## Dependencies
- Understanding of SACD disc format (stereo/multi-channel areas)
- Knowledge of PS3 storage API (`sys_storage_read`)
- Familiarity with progress tracking and atomic operations
- Access to test SACD discs that exhibit the stalling behavior

## Implementation Checklist
### Phase 1 (Immediate Fix)
- [x] Identify root cause in processing thread loop
- [x] Enhance error reporting in `blocks_readed == 0` condition
- [x] Add percentage calculation for stall point reporting
- [x] Distinguish between normal EOF and unexpected failures
- [x] Remove duplicate include in main.c (cleanup)

### Phase 2 (Comprehensive Solution)
- [ ] Collect stall point data across multiple discs to identify patterns
- [ ] Analyze SACD area transition points and disc-specific structural variations
- [ ] Implement retry logic for failed reads with smaller block sizes
- [ ] Add timeout mechanisms for hanging read operations
- [ ] Enhance progress calculation to account for variable disc structures
- [ ] Add logging for area transitions, encryption boundaries, and stall points
- [ ] Implement graceful handling of partial sector reads
- [ ] Add disc structure validation and sanity checks

## Verification Steps
### Phase 1
- [x] Code compiles without errors after changes
- [ ] Test with SACD disc that previously stalled at 69%
- [ ] Verify improved error message appears with exact percentage
- [ ] Confirm distinction between normal vs error completion

### Phase 2
- [ ] Automated test suite with various SACD disc types
- [ ] Performance benchmarks for read retry mechanisms
- [ ] Integration test covering full stereo + multi-channel extraction
- [ ] Regression test ensuring no new stalling points introduced

## Decision Authority
**Independent Decisions:**
- Code structure and implementation details
- Error message wording and formatting
- Logging levels and diagnostic information
- Performance optimizations within existing architecture

**Requires User Input:**
- Timeout values for read operations
- Retry attempt limits and backoff strategies
- Whether to fail fast or attempt recovery on disc errors
- UI/UX changes for progress reporting

## Questions/Uncertainties

### Blocking
- None currently identified

### Non-blocking
- **Optimal retry count**: Assuming 3 retries with exponential backoff
- **Timeout duration**: Assuming 30-second timeout for individual read operations
- **Block size reduction strategy**: Assuming halving block size on partial reads
- **Area transition detection**: Using existing encryption boundary logic

## Acceptable Tradeoffs
- **Performance vs Reliability**: Accept slower extraction if it reduces stalling
- **Code Complexity vs Robustness**: Add complexity for better error handling
- **Memory Usage**: Additional buffers for retry mechanisms acceptable
- **Backward Compatibility**: Maintain existing API while improving internals

## Status
Phase 1: Completed
Phase 2: Not Started

## Notes

### Implementation Decisions
- Enhanced error reporting provides immediate user value without architectural changes
- Used float percentage calculation for precise stall point reporting
- Maintained existing atomic operation patterns for thread safety

### Root Cause Analysis
The stalling occurs in the main processing loop where `sacd_read_block_raw()` returns 0 blocks. Key findings:
1. **Critical Path**: `sacd_dev_input_read()` truncates partial sectors when EOF reached
2. **Progress Issue**: No distinction between expected vs unexpected zero reads
3. **Variable Stall Points**: Observed at 69% and 85% on different discs, suggesting disc-specific content boundaries or structural variations
4. **Pattern**: Likely corresponds to stereo content boundaries, area transitions, or disc-specific metadata/padding regions

### Technical Details
- **Main Processing Loop**: `scarletbook_output.c:605-720`
- **Read Function**: `sacd_input.c:228-282` 
- **Progress Tracking**: `ripping.c:295-301`
- **Stall Condition**: `blocks_readed == 0` without context

### Future Considerations
- Monitor user reports for stall percentage patterns after Phase 1 fix (currently observed: 69%, 85%)
- Build database of stall patterns across different SACD releases/manufacturers
- Consider implementing read-ahead buffering for better performance
- Evaluate need for disc integrity checking before extraction begins
- Document SACD disc structure patterns for improved area handling
- Investigate correlation between stall points and specific disc characteristics (age, manufacturer, content type)