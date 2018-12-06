uniform mediump vec2 nestexSize;
uniform mediump vec2 outputSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;

varying vec2 vTexCoord;
varying vec2 omega;

void main()
{
    vec2 wposition = transform.xy + position * transform.zw;
    gl_Position = vec4(wposition, 0.0, 1.0);
    
    vTexCoord = texcoord;
    omega = vec2(3.1415 * outputSize.x, 2.0 * 3.1415 * nestexSize.y);
}