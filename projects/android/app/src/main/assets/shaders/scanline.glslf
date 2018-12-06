#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D nestex;

varying vec2 vTexCoord;
varying vec2 omega;

const float base_brightness = 0.95;
const vec2 sine_comp = vec2(0.05, 0.15);

void main ()
{
    vec4 c11 = texture2D(nestex, vTexCoord);

    vec4 scanline = c11 * (base_brightness + dot(sine_comp * sin(vTexCoord * omega), vec2(1.0)));
    gl_FragColor = clamp(scanline, 0.0, 1.0);
}