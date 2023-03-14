@vs vs
in vec4 vertex_pos;

void main() {
    gl_Position = vertex_pos;
}
@end

@fs fs_draw
uniform draw_params {
    vec2 resolution;
};

uniform sampler2D ibnds;
uniform sampler2D ifluid;

out vec4 out_color;

void main() {
    vec4 fluid = texture(ifluid, gl_FragCoord.xy / resolution);
    vec4 bnd = texture(ibnds, gl_FragCoord.xy / resolution);

    out_color = vec4(gl_FragCoord.xy / resolution, 0.7, 1) * vec4(fluid.w / 5);
    if (bnd.a > 0.0) {
        out_color = vec4(0.8, 0.8, 0.8, 1);
    }
    out_color.a = 1;
}
@end

@fs fs_fluid
uniform fluid_params {
    vec2 resolution;
    vec3 v3;
    vec2 wind;
    vec2 external_force;
    vec2 force_position;
    float K;
    float dt;
    float radius;
    float decay_rate;
    int clicked;
    int frame_cnt;
 };

//      x           y          z       w
// (velocity_x, velocity_y, pressure, ink)
uniform sampler2D bnds;
uniform sampler2D fluid;

out vec4 out_color;

vec4 fluidAt(vec2 coords) {
    return texture(fluid, coords / resolution);
}

bool bndsAt(vec2 coords) {
    return texture(bnds, coords / resolution).x > 0;
}

void main() {
    if (frame_cnt == 0) {
        out_color = vec4(0, 0, 1.5, 0);
        return;
    }

    vec4 curr   = fluidAt(gl_FragCoord.xy);
    vec4 top    = fluidAt(gl_FragCoord.xy + vec2(0, 1));
    vec4 bottom = fluidAt(gl_FragCoord.xy - vec2(0, 1));
    vec4 right  = fluidAt(gl_FragCoord.xy + vec2(1, 0));
    vec4 left   = fluidAt(gl_FragCoord.xy - vec2(1, 0));

    vec4 laplace = (top + bottom + left + right - 4 * curr);

    vec4 dx = (right - left) / 2;
    vec4 dy = (top - bottom) / 2;

    // divergence step
    // divergence = sum(incoming velocities)
    float div = dx.x + dy.y;

    // euler method / mass conservation step
    curr.z -= dt * (dx.z * curr.x + dy.z * curr.y + div * curr.z);

    // semi lagrangian advection
    // step backwards to find out what fluid must travel at this velocity to reach this cell
    curr.xyw = fluidAt(gl_FragCoord.xy - dt * curr.xy).xyw;

    // viscosity
    // TODO: what is this vector?!?!?
    curr.xyw += dt * v3 * laplace.xyw;
    // fix divergence
    // by adjusting the velocity to try and even out the pressure
    curr.xy -= K * vec2(dx.z, dy.z);
    // external forces
    vec2 dist = force_position - gl_FragCoord.xy;
    curr.xyw += vec3(external_force, clicked) * exp(-dot(dist, dist)/radius);
    curr.xy += wind;

    // dissipate ink
    // TODO: is there a better way to decay??
    curr.w *= decay_rate;

    // clamp velocities to ensure condition that dt < dx/u and dy/v
    // and clamp pressures to stop exploding
    curr = clamp(curr, vec4(-5, -5, 0.5, 0), vec4(5, 5, 3, 10));

    // boundary conditions TODO
    for (int i = 0; i < 4; ++i) { }

    out_color = curr;

    // if a neighbor is a boundary set
    // its velocities to negative to "repel" the fluid
    /*
    vec2 dirs[4] = {vec2(1, 0), vec2(0, 1), vec2(-1, 0), vec2(0, -1)};
    for (int i = 0; i < 4; ++i) {
        if (bndsAt(gl_FragCoord.xy + dirs[i])) {
            vec2 zero = vec2(1) - abs(dirs[i]);
            out_color.xy *= zero;
        }
    }*/

    if (bndsAt(gl_FragCoord.xy + vec2(1, 0)) || bndsAt(gl_FragCoord.xy - vec2(1, 0))) {
        out_color.x = - out_color.x;
    }
    if (bndsAt(gl_FragCoord.xy + vec2(0, 1)) || bndsAt(gl_FragCoord.xy - vec2(0, 1))) {
        out_color.y = - out_color.y;
    }

    /*
    if (bndsAt(gl_FragCoord.xy)) {
        // if its a boundary set its velocities to negative to "repel" the fluid
        out_color.xy = vec2(-dx.x, -dy.y);//, 0, 0);
        //out_color.w = 0;
    }*/
}
@end


// Method as implemented by the paper
@fs fs_fluid_ref
uniform fluid_ref_params {
    vec2 resolution;
    float c_scale;
    float K;
    float dt;
    vec2 v;
    vec2 external_force;
    vec2 force_position;
};

uniform sampler2D fluid;

out vec4 out_color;

void main() {
    float s = K / dt;
    vec2 uv = gl_FragCoord.xy / resolution;
    vec2 uvr = (gl_FragCoord.xy + vec2(1, 0)) / resolution;
    vec2 uvl = (gl_FragCoord.xy - vec2(1, 0)) / resolution;
    vec2 uvt = (gl_FragCoord.xy + vec2(0, 1)) / resolution;
    vec2 uvb = (gl_FragCoord.xy - vec2(0, 1)) / resolution;
    vec4 fc = texture(fluid, uv);
    vec3 fr = texture(fluid, uvr).xyz;
    vec3 fl = texture(fluid, uvl).xyz;
    vec3 ft = texture(fluid, uvt).xyz;
    vec3 fb = texture(fluid, uvb).xyz;

    mat4x3 fmat = mat4x3(fr, fl, ft, fb);

    vec3 udx = vec3(fmat[0] - fmat[1]) * c_scale;
    vec3 udy = vec3(fmat[2] - fmat[3]) * c_scale;

    float udiv = udx.x + udy.y;
    vec2 ddx = vec2(udx.z, udy.z);

    // SOLVE FOR DENSITY
    fc.z -= dt * dot(vec3(ddx, udiv), fc.xyz);
    fc.z = clamp(fc.z, 0.5, 3);

    // SOLVE FOR VELOCITY
    vec2 pdx = s * ddx;
    // NOTE: This could be wrong - refer to implementation?
    vec2 laplace = vec4(1) * mat2x4(fmat) - 4 * fc.xy;
    vec2 viscosity = v * laplace;

    // advection
    vec2 was = uv - dt*fc.xy/resolution;
    fc.xy = texture(fluid, was).xy;

    vec2 force = external_force;
    if (length(gl_FragCoord.xy - force_position) > 3) {
        force = vec2(0);
    }

    fc.xy += dt * (viscosity - pdx + force);

    // boundary conditions TODO
    for (int i = 0; i < 4; ++i) { }

    out_color = fc;
}
@end

@fs fs_bnds

uniform bnds_params {
    vec2 resolution;
    int add;
    vec2 add_pos;
    float rad;
};

uniform sampler2D c_bnds;

out vec4 fragColor;

void main() {
    fragColor = texture(c_bnds, gl_FragCoord.xy / resolution);
    if (length(gl_FragCoord.xy - add_pos) < rad && add != 0) {
        fragColor = add * vec4(1, 0, 0, 1);
    }
}
@end

@program bnds vs fs_bnds
@program fluid vs fs_fluid
@program draw vs fs_draw
