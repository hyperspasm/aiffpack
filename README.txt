aiffpack is a utility for creating multichannel AIFF (or optionally WAV) sound files
from a set of input sound files of varying formats and resolutions. The sample data
type and bit resolution of the output file can also be specified. The length of the
output file is the length of the longest input file. Shorter input files will create
tracks that are padded at the end with silence. The order of the input files specified
determines the order of the tracks in the output file. The first track of the output
file is the first track of the first input file and the last track of the output file
is the last track of the last input file.

aiffpack is Copyright (C) 2005, Shea Ako and is  made possible by
libsndfile Copyright (C) 1999-2004 Erik de Castro Lopo.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.