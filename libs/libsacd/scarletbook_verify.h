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

#ifndef SCARLETBOOK_VERIFY_H_INCLUDED
#define SCARLETBOOK_VERIFY_H_INCLUDED

#include <inttypes.h>
#include "scarletbook.h"

/**
 * Verify ScarletBook integrity with structural checks and sector spot-testing
 * Works with any SACD source: device, server, or ISO file
 * @param handle ScarletBook handle to verify
 * @return 0 if verification passed, positive number of errors if failed, -1 on invalid input
 */
int scarletbook_verify(scarletbook_handle_t *handle);

#endif /* SCARLETBOOK_VERIFY_H_INCLUDED */
