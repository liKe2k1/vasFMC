///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2005-2006 Martin Böhme
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
///////////////////////////////////////////////////////////////////////////////

// vasfmc_gauge_main.h

#ifndef __VASFMC_GAUGE_MAIN_H__
#define __VASFMC_GAUGE_MAIN_H__

#include "fs9gauges.h"

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

EXTERN_C void FSAPI PfdCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
EXTERN_C BOOL FSAPI PfdMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

EXTERN_C void FSAPI NdCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
EXTERN_C BOOL FSAPI NdMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

EXTERN_C void FSAPI UecamCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
EXTERN_C BOOL FSAPI UecamMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

EXTERN_C void FSAPI McduCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
EXTERN_C BOOL FSAPI McduMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);
void toggleMcduKeyboard();

EXTERN_C void FSAPI FcuCallback(PGAUGEHDR pgauge, SINT32 service_id, UINT32 extra_data);
EXTERN_C BOOL FSAPI FcuMouseFunction(PPIXPOINT arg1, FLAGS32 mouse_flags);

#define PFD_WIDTH        100
#define PFD_HEIGHT       100
#define PFD_BORDER        20

#define ND_WIDTH        100
#define ND_HEIGHT       100
#define ND_BORDER        20

#define UECAM_WIDTH        100
#define UECAM_HEIGHT       100
#define UECAM_BORDER        20

#define MCDU_WIDTH      377
#define MCDU_HEIGHT     600
#define MCDU_BORDER      10
#define MCDU_BORDER_TOP  50

#define FCU_WIDTH      998
#define FCU_HEIGHT     166
#define FCU_BORDER      10
#define FCU_BORDER_TOP  0

#endif // __VASFMC_GAUGE_MAIN_H__
