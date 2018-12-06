/*

   Hyllian's 4xBR Shader

   Copyright (C) 2011 Hyllian/Jararaca - sergiogdb@gmail.com

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

uniform mediump vec2 nestexSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;
varying vec2 vTexCoord[3];

void main() {
    vec2 ps = 1.0 / nestexSize;
    vTexCoord[0] = texcoord;
    vTexCoord[1] = vec2(0.0, -ps.y); // B
    vTexCoord[2] = vec2(-ps.x, 0.0); // D

    vec2 wposition = transform.xy + position * transform.zw;
    gl_Position = vec4(wposition, 0.0, 1.0);
}