// w=0 is neutral, including UI, inventory, particles and shadow submissions.
uniform vec4 u_modelAmbient;
vec4 applyModelAmbient(vec4 color)
{
    color.rgb *= mix(vec3_splat(1.0), u_modelAmbient.rgb, u_modelAmbient.w);
    return color;
}
