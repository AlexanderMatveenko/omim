uniform float u_opacity;
#ifdef ENABLE_VTF
varying lowp vec4 v_color;
#else
uniform sampler2D u_colorTex;
varying vec2 v_colorTexCoords;
#endif

varying vec3 v_radius;

void main(void)
{
#ifdef ENABLE_VTF
  lowp vec4 finalColor = v_color;
#else
  lowp vec4 finalColor = texture2D(u_colorTex, v_colorTexCoords);
#endif
  float stepValue = step(v_radius.z * v_radius.z, v_radius.x * v_radius.x + v_radius.y * v_radius.y);
  finalColor.a = finalColor.a * u_opacity * (1.0 - stepValue);
  gl_FragColor = finalColor;
}
