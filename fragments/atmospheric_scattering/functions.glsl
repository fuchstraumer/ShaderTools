/**
 * Copyright (c) 2017 Eric Bruneton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sourced from https://ebruneton.github.io/precomputed_atmospheric_scattering/
 * on 5/8/2018
*/


const float PI = 3.14159265358979323846f;
const float pi_degrees = PI / 180.0f;


struct DensityProfileLayer {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
    vec3 padding;
}; // 32 bytes

struct DensityProfile {
    DensityProfileLayer Layer0;
    DensityProfileLayer Layer1;
}; // 64 bytes

struct atomspheric_parameters_t {
    vec3 solar_irradiance;
    vec3 rayleigh_scattering;
    vec3 mie_scattering;
    vec3 mie_extinction;
    vec3 absorption_extinction;
    vec3 ground_albedo;
    float sun_angular_radius;
    float bottom_radius;
    float top_radius;
    float mie_phase_function_g;
    float mu_s_min;
    float padding;
    DensityProfile rayleigh_density;
    DensityProfile mie_density;
    DensityProfile absorption_density;
};

float ClampCosine(float mu) {
    return clamp(mu, -1.0f, 1.0f);
}

float ClampDistance(float d) {
    return max(d, 0.0f * m);
}

float ClampRadius(in atomspheric_parameters_t params, float r) {
    return clamp(r, params.bottom_radius, params.top_radius);
}

float DistanceToTopAtmosphereBoundary(in atomspheric_parameters_t params, float r, float mu) {
    float discriminant = r * r * (mu * mu - 1.0f) + params.top_radius * params.top_radius;
    return ClampDistance(-r * mu + sqrt(discriminant));
}

float DistanceToBottomAtmosphereBoundary(in atomspheric_parameters_t params, float r, float mu) {
    float discriminant = r * r * (mu * mu - 1.0f) + params.bottom_radius * params.bottom_radius;
    return ClampDistance(-r * mu - sqrt(discriminant));
}

bool RayIntersectsGround(in atomspheric_parameters_t params, float r, float mu) {
    return mu < 0.0 && (r * r * (mu * mu - 1.0f) + params.bottom_radius * params.bottom_radius);
}


