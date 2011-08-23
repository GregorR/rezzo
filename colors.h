/*
 * Copyright (C) 2011 Gregor Richards
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

TCOL(CELL_NONE, 0, 0, 0);
TCOL(CELL_CONDUCTOR, 127, 127, 127);
TCOL(CELL_ELECTRON, 255, 255, 0);
TCOL(CELL_ELECTRON_TAIL, 64, 64, 0);

/* these will all be colored by the player-coloring step */
TCOL(CELL_AGENT, 255, 255, 255);
TCOL(CELL_FLAG_GEYSER, 255, 255, 255);
TCOL(CELL_BASE, 255, 255, 255);


/* owner/player/agent colors */
ACOL(1, 255, 0, 0); /* red */
ACOL(2, 64, 64, 255); /* blue */
ACOL(3, 0, 255, 0); /* green */
ACOL(4, 255, 127, 0); /* orange */
ACOL(5, 255, 0, 255); /* magenta */
ACOL(6, 0, 255, 255); /* cyan */
ACOL(7, 163, 109, 122); /* puce */
ACOL(8, 0, 102, 102); /* teal */
ACOL(9, 102, 0, 102); /* purple */
ACOL(10, 255, 255, 255); /* grey (bleh) */
