/*
    cgwg's CRT shader

    Copyright (C) 2010-2011 cgwg, Themaister

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    (cgwg gave their consent to have their code distributed under the GPL in
    this message:

        http://board.byuu.org/viewtopic.php?p=26075#p26075

        "Feel free to distribute my shaders under the GPL. After all, the
        barrel distortion code was taken from the Curvature shader, which is
        under the GPL."
    )
*/

uniform mediump vec2 nestexSize;
uniform mediump vec2 outputSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;

varying vec4 TEX0;
// out variables go here as varying mediump whatever
// TODO/FIXME - Wrap all these in a struct-like type so we can address
varying vec2 c01;
varying vec2 c11;
varying vec2 c21;
varying vec2 c31;
varying vec2 c02;
varying vec2 c12;
varying vec2 c22;
varying vec2 c32;
varying float mod_factor;
varying vec2 ratio_scale;

#define vTexCoord TEX0.xy
#define SourceSize vec4(nestexSize, 1.0 / nestexSize) //either nestexSize or InputSize
#define outsize vec4(outputSize, 1.0 / outputSize)

void main()
{
    vec2 wposition = transform.xy + position * transform.zw;
    gl_Position = vec4(wposition, 0.0, 1.0);
        
    TEX0.xy = texcoord.xy;
    vec2 delta = SourceSize.zw;
    float dx   = delta.x;
    float dy   = delta.y;

    c01 = vTexCoord + vec2(-dx, 0.0);
    c11 = vTexCoord + vec2(0.0, 0.0);
    c21 = vTexCoord + vec2(dx, 0.0);
    c31 = vTexCoord + vec2(2.0 * dx, 0.0);
    c02 = vTexCoord + vec2(-dx, dy);
    c12 = vTexCoord + vec2(0.0, dy);
    c22 = vTexCoord + vec2(dx, dy);
    c32 = vTexCoord + vec2(2.0 * dx, dy);
    mod_factor  = vTexCoord.x * outsize.x;
    ratio_scale = vTexCoord * SourceSize.xy;
}
