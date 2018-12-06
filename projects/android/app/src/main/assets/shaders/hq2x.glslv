uniform vec2 nestexSize;
uniform highp vec4 transform;

attribute vec2 position;
attribute vec2 texcoord;
varying mediump vec4 vTexCoord[5];

void main() {
  vec2 dg1 = 0.5 / nestexSize;
  vec2 dg2 = vec2(-dg1.x, dg1.y);
  vec2 dx = vec2(dg1.x, 0.0);
  vec2 dy = vec2(0.0, dg1.y);

  vTexCoord[0].xy = texcoord;
  vTexCoord[1].xy = texcoord - dg1;
  vTexCoord[1].zw = texcoord - dy;
  vTexCoord[2].xy = texcoord - dg2;
  vTexCoord[2].zw = texcoord + dx;
  vTexCoord[3].xy = texcoord + dg1;
  vTexCoord[3].zw = texcoord + dy;
  vTexCoord[4].xy = texcoord + dg2;
  vTexCoord[4].zw = texcoord - dx;

  vec2 wposition = transform.xy + position * transform.zw;
  gl_Position = vec4(wposition, 0.0, 1.0);
}