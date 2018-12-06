/*
   Hyllian's xBR-lv2-lq Shader

   Copyright (C) 2011/2015 Hyllian/Jararaca - sergiogdb@gmail.com

   Copyright (C) 2011-2015 Hyllian - sergiogdb@gmail.com

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

#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute

struct out_vertex {
    vec2 _texCoord1;
    vec4 _t1;
    vec4 _t2;
    vec4 _t3;
};

COMPAT_ATTRIBUTE vec2 position;
COMPAT_ATTRIBUTE vec2 texcoord;
COMPAT_VARYING vec4 TEX0;
COMPAT_VARYING vec4 TEX1;
COMPAT_VARYING vec4 TEX2;
COMPAT_VARYING vec4 TEX3;

uniform mediump vec2 nestexSize;
uniform mediump vec2 outputSize;
uniform highp vec4 transform;

void main()
{
    out_vertex _OUT;
    vec2 _ps;
    
    vec2 wposition = transform.xy + position * transform.zw;
    gl_Position = vec4(wposition, 0.0, 1.0);
    
    _ps = vec2(1.00000000E+00/nestexSize.x, 1.00000000E+00/nestexSize.y);
    _OUT._t1 = texcoord.xxxy + vec4(-_ps.x, 0.00000000E+00, _ps.x, -_ps.y);
    _OUT._t2 = texcoord.xxxy + vec4(-_ps.x, 0.00000000E+00, _ps.x, 0.00000000E+00);
    _OUT._t3 = texcoord.xxxy + vec4(-_ps.x, 0.00000000E+00, _ps.x, _ps.y);

    TEX0.xy = texcoord.xy;
    TEX1 = _OUT._t1;
    TEX2 = _OUT._t2;
    TEX3 = _OUT._t3;

    return;
}