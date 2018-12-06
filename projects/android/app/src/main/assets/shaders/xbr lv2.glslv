/*
   Hyllian's xBR-lv2 Shader

   Copyright (C) 2011-2016 Hyllian - sergiogdb@gmail.com
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
   Incorporates some of the ideas from SABR shader. Thanks to Joshua Street.
*/


uniform mediump vec2 nestexSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;

varying mediump vec2 outTexCoord;
varying mediump vec4 t1;
varying mediump vec4 t2;
varying mediump vec4 t3;
varying mediump vec4 t4;
varying mediump vec4 t5;
varying mediump vec4 t6;
varying mediump vec4 t7;

void main() {

    float dx = (1.0/nestexSize.x);
    float dy = (1.0/nestexSize.y);

    outTexCoord     = texcoord;
	outTexCoord.x *= 1.00000001;
    t1 = texcoord.xxxy + vec4( -dx, 0, dx,-2.0*dy); // A1 B1 C1
    t2 = texcoord.xxxy + vec4( -dx, 0, dx,    -dy); //  A  B  C
    t3 = texcoord.xxxy + vec4( -dx, 0, dx,      0); //  D  E  F
    t4 = texcoord.xxxy + vec4( -dx, 0, dx,     dy); //  G  H  I
    t5 = texcoord.xxxy + vec4( -dx, 0, dx, 2.0*dy); // G5 H5 I5
    t6 = texcoord.xyyy + vec4(-2.0*dx,-dy, 0,  dy); // A0 D0 G0
    t7 = texcoord.xyyy + vec4( 2.0*dx,-dy, 0,  dy); // C4 F4 I4

    vec2 wposition = transform.xy + position * transform.zw;
    gl_Position = vec4(wposition, 0.0, 1.0);
}