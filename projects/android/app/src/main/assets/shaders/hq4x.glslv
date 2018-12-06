/*

   4xGLSLHqFilter shader

   Copyright (C) 2005 guest(r) - guest.r@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

uniform vec2 nestexSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;
varying mediump vec4 vTexCoord[7];

void main()
{
        vec2 dg1 = 0.5 / nestexSize;
        vec2 dg2 = vec2(-dg1.x, dg1.y);
        vec2 sd1 = dg1 * 0.5;
        vec2 sd2 = dg2 * 0.5;
        vec2 ddx = vec2(dg1.x, 0.0);
        vec2 ddy = vec2(0.0, dg1.y);

        vTexCoord[0].xy = texcoord;
        vTexCoord[1].xy = texcoord - sd1;
        vTexCoord[2].xy = texcoord - sd2;
        vTexCoord[3].xy = texcoord + sd1;
        vTexCoord[4].xy = texcoord + sd2;
        vTexCoord[5].xy = texcoord - dg1;
        vTexCoord[6].xy = texcoord + dg1;
        vTexCoord[5].zw = texcoord - dg2;
        vTexCoord[6].zw = texcoord + dg2;
        vTexCoord[1].zw = texcoord - ddy;
        vTexCoord[2].zw = texcoord + ddx;
        vTexCoord[3].zw = texcoord + ddy;
        vTexCoord[4].zw = texcoord - ddx;

        vec2 wposition = transform.xy + position * transform.zw;
        gl_Position = vec4(wposition, 0.0, 1.0);
}