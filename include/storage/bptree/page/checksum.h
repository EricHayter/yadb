/*-----------------------------------------------------------------------------
 *
 * checksum.h
 *      Page checksum interface
 *
 * To check the integrity of pages, page headers will include a checksum
 * calculated each time the page is flushed to disk. When the disk
 * is eventually read from disk into the page buffer manager the checksum will
 * be recalculated to check for any signs of corruption.
 *-----------------------------------------------------------------------------
 */

#pragma once

#include <cstdint>
#include "common/definitions.h"

/*
 * Calculate 64-bit checksum of page
 */
uint64_t checksum64(FullPage page);
