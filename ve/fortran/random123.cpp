/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

// This is the C99/OpenMP interface to Random123

#include "random123.hpp"

extern "C" {
    __int128 random123(uint64_t* start, uint64_t* key, uint64_t* index) {
        __int128 lol = random123_wrap(*start, *key, *index);
        printf("i: %ld,v: %lu\n", *index, lol);
        return random123_wrap(*start, *key, *index);
    }
}
