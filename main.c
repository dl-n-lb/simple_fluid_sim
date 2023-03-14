#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "util/sokol_imgui.h"

#include "shader.glsl.h"

// array looks like
// vec4(v_x, v_y, density, _)
// n.b. density should remain constant
// by the simplified navier stokes

static int scr_w = 800;
static int scr_h = 600;

#define FACTOR 0.5

static struct {
  int m_x, m_y;
  int pm_x, pm_y;
  bool clicked;
  bool add_liquid;
} input;

static struct {
  float k;
  float dt;
  float add_fluid_rad;
  float force_multiplier;
  float decay_rate;
  float add_bnds_rad;
  float v3[3];
  float wind_amt[2];
} config;

static struct {
  struct {
    sg_image target;
    sg_image dpth;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass pass;
    sg_pass_action pass_action;
  } bnds[2];
  struct {
    sg_image target;
    sg_image dpth;
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass pass;
    sg_pass_action pass_action;
  } fluid[2]; // for odd and even frames, using the other as input
  struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
  } render; // draw to screen (dont need pass since it is default)
} state;

void setup_fluid_passes() {
  sg_image_desc img_dsc = {
      .render_target = true,
      .width = scr_w,
      .height = scr_h,
      .pixel_format = SG_PIXELFORMAT_RGBA32F,
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR,
      .wrap_u = SG_WRAP_REPEAT,
      .wrap_v = SG_WRAP_REPEAT,
      .sample_count = 1,
  };
  sg_image_desc dpth_dsc = img_dsc;
  dpth_dsc.pixel_format = SG_PIXELFORMAT_DEPTH;
  for (int i = 0; i < 2; ++i) {
    state.fluid[i].target = sg_make_image(&img_dsc);
    state.fluid[i].dpth = sg_make_image(&dpth_dsc);
  }

  for (int i = 0; i < 2; ++i) {
    state.bnds[i].target = sg_make_image(&img_dsc);
    state.bnds[i].dpth = sg_make_image(&dpth_dsc);

    state.bnds[i].pass_action =
        (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE}};
    state.bnds[i].pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {.attrs[ATTR_vs_vertex_pos].format = SG_VERTEXFORMAT_FLOAT2},
        .shader = sg_make_shader(bnds_shader_desc(sg_query_backend())),
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA32F,
        .depth.pixel_format = SG_PIXELFORMAT_DEPTH,
    });
    float fsq_verts[] = {-1.f, -3.f, 3.f, 1.f, -1.f, 1.f};
    state.bnds[i].bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(fsq_verts),
    });
    state.bnds[i].pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.bnds[i].target,
        .depth_stencil_attachment.image = state.bnds[i].dpth,
    });
    state.bnds[i].bind.fs_images[SLOT_c_bnds] = state.bnds[i == 0].target;

    state.fluid[i].pass_action =
        (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE}};
    state.fluid[i].pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {.attrs[ATTR_vs_vertex_pos].format = SG_VERTEXFORMAT_FLOAT2},
        .shader = sg_make_shader(fluid_shader_desc(sg_query_backend())),
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA32F,
        .depth.pixel_format = SG_PIXELFORMAT_DEPTH,
    });

    state.fluid[i].bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(fsq_verts),
    });
    state.fluid[i].bind.fs_images[SLOT_fluid] = state.fluid[i == 0].target;
    state.fluid[i].bind.fs_images[SLOT_bnds] = state.bnds[i].target;
    state.fluid[i].pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.fluid[i].target,
        .depth_stencil_attachment.image = state.fluid[i].dpth,
    });
  }
  state.bnds[0].bind.fs_images[SLOT_c_bnds] = state.bnds[1].target;
  state.bnds[1].bind.fs_images[SLOT_c_bnds] = state.bnds[0].target;
}

void init(void) {
  sg_setup(&(sg_desc){
      .context = sapp_sgcontext(),
      .logger.func = slog_func,
  });

  simgui_setup(&(simgui_desc_t){});

  float fsq_verts[] = {-1.f, -3.f, 3.f, 1.f, -1.f, 1.f};
  state.render.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(fsq_verts),
      .label = "fsq vertices",
  });

  state.render.pip = sg_make_pipeline(&(sg_pipeline_desc){
      .layout =
          {
              .attrs[ATTR_vs_vertex_pos].format = SG_VERTEXFORMAT_FLOAT2,
          },
      .shader = sg_make_shader(draw_shader_desc(sg_query_backend())),
  });

  state.render.pass_action = (sg_pass_action){
      .colors[0] =
          {
              .action = SG_ACTION_CLEAR,
              .value = {0.18, 0.18, 0.18, 1},
          },
  };

  setup_fluid_passes();

  config.k = 0.2;
  config.v3[0] = 0.5;
  config.v3[1] = 0.5;
  config.v3[2] = 0.1;
  config.dt = 5;
  config.force_multiplier = 0.5;
  config.add_fluid_rad = 500;
  config.add_bnds_rad = 50;
  config.decay_rate = 1;
}

void frame(void) {
#ifdef DEBUG
  if (sapp_frame_count() % 100 == 0) {
    fprintf(stderr, "Current FPS: %f\n", 1.0 / sapp_frame_duration());
  }
#endif
  int w = sapp_width();
  int h = sapp_height();
  if (w != scr_w || h != scr_h) {
    scr_w = w;
    scr_h = h;
#ifdef DEBUG
    fprintf(stderr, "resolution change: %d, %d\n", w, h);
#endif
    setup_fluid_passes();
  }

  simgui_new_frame(&(simgui_frame_desc_t){
      .width = sapp_width(),
      .height = sapp_height(),
      .delta_time = sapp_frame_duration(),
      .dpi_scale = sapp_dpi_scale(),
  });

  bool display = true;
  igSetNextWindowSize((ImVec2){(scr_w > 500) ? 500 : scr_w, 200},
                      ImGuiCond_Always);
  igSetNextWindowPos((ImVec2){0, 0}, ImGuiCond_Always, (ImVec2){0, 0});
  igBegin("Settings", &display, ImGuiWindowFlags_NoResize);
  igInputFloat("K", &config.k, 0.001, 0.1, "%.3f", ImGuiSliderFlags_None);
  igInputFloat3("v3", &config.v3[0], "%.3f", ImGuiSliderFlags_None);
  igInputFloat2("wind", &config.wind_amt[0], "%.3f", ImGuiSliderFlags_None);
  igSliderFloat("fluid input radius", &config.add_fluid_rad, 1, 2000, "%.1f",
                ImGuiSliderFlags_None);
  igSliderFloat("solid input radius", &config.add_bnds_rad, 1, 1000, "%.1f",
                ImGuiSliderFlags_None);
  igSliderFloat("input force multiplier", &config.force_multiplier, 0, 5,
                "%.3f", ImGuiSliderFlags_None);
  igSliderFloat("delta time multiplier", &config.dt, 0, 100, "%.3f",
                ImGuiSliderFlags_None);
  igSliderFloat("decay rate", &config.decay_rate, 0.99, 1, "%.5f",
                ImGuiSliderFlags_None);
  igEnd();

  int c = sapp_frame_count() % 2;
  state.render.bind.fs_images[SLOT_ifluid] = state.fluid[c].target;
  state.render.bind.fs_images[SLOT_ibnds] = state.bnds[c].target;

  sg_begin_pass(state.bnds[c].pass, &state.bnds[c].pass_action);
  {
    sg_apply_pipeline(state.bnds[c].pip);
    sg_apply_bindings(&state.bnds[c].bind);
    bnds_params_t bp = {
        .resolution = {scr_w, scr_h},
        .add = input.clicked && !input.add_liquid,
        .add_pos = {input.m_x, scr_h - input.m_y},
        .rad = config.add_bnds_rad,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_bnds_params, &SG_RANGE(bp));
    sg_draw(0, 3, 1);
  }
  sg_end_pass();

  sg_begin_pass(state.fluid[c].pass, &state.fluid[c].pass_action);
  {
    sg_apply_pipeline(state.fluid[c].pip);
    sg_apply_bindings(&state.fluid[c].bind);
    fluid_params_t fp = {
        .frame_cnt = sapp_frame_count(),
        .K = config.k,
        .dt = config.dt * sapp_frame_duration(),
        .resolution = {scr_w, scr_h},
        .v3 = {config.v3[0], config.v3[1], config.v3[2]},
        .wind = {config.wind_amt[0], config.wind_amt[1]},
        .external_force =
            {
                config.force_multiplier * (input.m_x - input.pm_x) *
                    input.clicked,
                config.force_multiplier * (input.pm_y - input.m_y) *
                    input.clicked,
            },
        .radius = config.add_fluid_rad,
        .decay_rate = config.decay_rate,
        .clicked = input.clicked && input.add_liquid,
        .force_position = {input.m_x, scr_h - input.m_y},
    };
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_fluid_params, &SG_RANGE(fp));
    sg_draw(0, 3, 1);
  }
  sg_end_pass();
  sg_begin_default_pass(&state.render.pass_action, w, h);
  {
    sg_apply_pipeline(state.render.pip);
    sg_apply_bindings(&state.render.bind);
    draw_params_t dp = {.resolution = {scr_w, scr_h}};
    sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_draw_params, &SG_RANGE(dp));
    sg_draw(0, 3, 1);
    simgui_render();
  }
  sg_end_pass();
  sg_commit();
}

void event(const sapp_event *event) {
  if (simgui_handle_event(event))
    return;
  switch (event->type) {
  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    input.pm_x = input.m_x;
    input.pm_y = input.m_y;
    input.m_x = (int)event->mouse_x;
    input.m_y = (int)event->mouse_y;
    break;
  }
    /*
  case SAPP_EVENTTYPE_TOUCHES_MOVED: {
    input.pm_x = input.m_x;
    input.pm_y = input.m_y;
    input.m_x = event->touches[0].pos_x;
    input.m_y = event->touches[0].pos_y;
    break;
  }*/
  case SAPP_EVENTTYPE_KEY_DOWN: {
    if (event->key_code == SAPP_KEYCODE_SPACE)
      input.add_liquid = !input.add_liquid;
    break;
  }
    case SAPP_EVENTTYPE_MOUSE_DOWN: {
  //case SAPP_EVENTTYPE_TOUCHES_BEGAN: {
    input.clicked = true;
    break;
  }

  default: {
    input.clicked = false;
    break;
  }
  }
}

void cleanup(void) {
  simgui_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = event,
      .width = 800,
      .height = 600,
      .window_title = "i <3 fluids",
      .icon.sokol_default = true,
      .logger.func = slog_func,
  };
}
