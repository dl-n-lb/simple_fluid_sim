#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

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
} input;

static struct {
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
    state.fluid[i].pass_action =
        (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE}};
    state.fluid[i].pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {.attrs[ATTR_vs_vertex_pos].format = SG_VERTEXFORMAT_FLOAT2},
        .shader = sg_make_shader(fluid_shader_desc(sg_query_backend())),
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA32F,
        .depth.pixel_format = SG_PIXELFORMAT_DEPTH,
    });

    float fsq_verts[] = {-1.f, -3.f, 3.f, 1.f, -1.f, 1.f};
    state.fluid[i].bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(fsq_verts),
    });
    state.fluid[i].bind.fs_images[SLOT_fluid] = state.fluid[i == 0].target;
    state.fluid[i].pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.fluid[i].target,
        .depth_stencil_attachment.image = state.fluid[i].dpth,
    });
  }
}

void init(void) {
  sg_setup(&(sg_desc){
      .context = sapp_sgcontext(),
      .logger.func = slog_func,
  });

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
  int c = sapp_frame_count() % 2;
  state.render.bind.fs_images[SLOT_fluid] = state.fluid[c].target;
  sg_begin_pass(state.fluid[c].pass, &state.fluid[c].pass_action);
  {
    sg_apply_pipeline(state.fluid[c].pip);
    sg_apply_bindings(&state.fluid[c].bind);
    fluid_params_t fp = {
        .frame_cnt = sapp_frame_count(),
        .K = 0.2,
        .dt = sapp_frame_duration() * 5,
        .resolution = {scr_w, scr_h},
        .v = {0.05, 0.05},
        .c_scale = 0.5,
        .external_force =
            {
                FACTOR * (input.m_x - input.pm_x) * input.clicked,
                FACTOR * (input.pm_y - input.m_y) * input.clicked,
            },
        .radius = 1500,
        .clicked = input.clicked,
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
  }
  sg_end_pass();
  sg_commit();
}

void event(const sapp_event *event) {
  switch (event->type) {
  case SAPP_EVENTTYPE_MOUSE_MOVE: {
    input.pm_x = input.m_x;
    input.pm_y = input.m_y;
    input.m_x = (int)event->mouse_x;
    input.m_y = (int)event->mouse_y;
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_DOWN: {
    input.clicked = true;
    break;
  }
  default: {
    input.clicked = false;
    break;
  }
  }
}

void cleanup(void) { sg_shutdown(); }

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