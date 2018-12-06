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

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform mediump vec2 nestexSize;
uniform sampler2D nestex;

varying vec4 TEX0;
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

#define FragColor gl_FragColor

#define Source nestex

#define CRTCGWG_GAMMA 2.7

#define TEX2D(c) texture2D(Source ,(c))
#define PI 3.141592653589

void main()
{
    vec2 uv_ratio = fract(ratio_scale);
    vec3 col, col2;

    mat4 texes0 = mat4(TEX2D(c01).xyzw, TEX2D(c11).xyzw, TEX2D(c21).xyzw, TEX2D(c31).xyzw);
    mat4 texes1 = mat4(TEX2D(c02).xyzw, TEX2D(c12).xyzw, TEX2D(c22).xyzw, TEX2D(c32).xyzw);

    vec4 coeffs = vec4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x) + 0.005;
    coeffs      = sin(PI * coeffs) * sin(0.5 * PI * coeffs) / (coeffs * coeffs);
    coeffs      = coeffs / dot(coeffs, vec4(1.0, 1.0, 1.0, 1.0));

    vec3 weights  = vec3( 3.33 * uv_ratio.y,        uv_ratio.y *  3.33,        uv_ratio.y *  3.33);
    vec3 weights2 = vec3(-3.33 * uv_ratio.y + 3.33, uv_ratio.y * -3.33 + 3.33, uv_ratio.y * -3.33 + 3.33);

    col  = clamp(texes0 * coeffs, 0.0, 1.0).xyz;
    col2 = clamp(texes1 * coeffs, 0.0, 1.0).xyz;

    vec3 wid  = 2.0 * pow(col,  vec3(4.0, 4.0, 4.0)) + 2.0;
    vec3 wid2 = 2.0 * pow(col2, vec3(4.0, 4.0, 4.0)) + 2.0;

    col  = pow(col,  vec3(CRTCGWG_GAMMA));
    col2 = pow(col2, vec3(CRTCGWG_GAMMA));

    vec3 sqrt1 = inversesqrt(0.5 * wid);
    vec3 sqrt2 = inversesqrt(0.5 * wid2);

    vec3 pow_mul1 = weights * sqrt1;
    vec3 pow_mul2 = weights2 * sqrt2;

    vec3 div1 = 0.1320 * wid  + 0.392;
    vec3 div2 = 0.1320 * wid2 + 0.392;

    vec3 pow1 = -pow(pow_mul1, wid);
    vec3 pow2 = -pow(pow_mul2, wid2);

    weights  = exp(pow1) / div1;
    weights2 = exp(pow2) / div2;

    vec3 multi = col * weights + col2 * weights2;
    vec3 mcol  = mix(vec3(1.0, 0.7, 1.0), vec3(0.7, 1.0, 0.7), floor(mod(mod_factor, 2.0)));

    FragColor = vec4(pow(mcol * multi, vec3(0.454545, 0.454545, 0.454545)), 1.0);
}