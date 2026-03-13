#version 300 es
precision highp float;
precision highp int;

out vec4 FragColor;
in vec3 TexCoords;

uniform float daylight;

void main()
{
    float t = clamp(normalize(TexCoords).y * 0.5 + 0.5, 0.0, 1.0);

    vec3 dayHorizon = vec3(0.68, 0.82, 0.96);
    vec3 dayZenith = vec3(0.20, 0.45, 0.80);
    vec3 nightHorizon = vec3(0.03, 0.05, 0.10);
    vec3 nightZenith = vec3(0.01, 0.02, 0.06);

    vec3 dayColor = mix(dayHorizon, dayZenith, t);
    vec3 nightColor = mix(nightHorizon, nightZenith, t);

    float day = clamp(daylight, 0.0, 1.0);
    vec3 skyColor = mix(nightColor, dayColor, day);
    FragColor = vec4(skyColor, 1.0);
}