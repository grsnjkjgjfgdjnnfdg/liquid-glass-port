#pragma once

// Core types
#include "opal_types.h"
#include "opal_config.h"
#include "opal_color.h"
#include "opal_draw.h"

// Backend
#include "backend/dx11_backend.h"
#include "backend/dx11_state.h"
#include "backend/dx11_shaders.h"
#include "backend/dx11_resources.h"

// Glass pipeline
#include "glass/glass_renderer.h"
#include "glass/glass_blur.h"
#include "glass/glass_composite.h"
#include "glass/glass_gradient.h"
#include "glass/glass_noise.h"
#include "glass/glass_backdrop_blur.h"

// Layout
#include "layout/layout_engine.h"
#include "layout/layout_grid.h"
#include "layout/layout_flex.h"
#include "layout/layout_tab.h"
#include "layout/layout_primitives.h"
#include "layout/layout_declarative.h"

// Components
#include "components/component_base.h"
#include "components/comp_panel.h"
#include "components/comp_group.h"
#include "components/comp_sidebar.h"
#include "components/comp_sidebar_selector.h"
#include "components/comp_user_section.h"
#include "components/comp_settings_menu.h"
#include "components/comp_liquid_knob.h"
#include "components/comp_toggle.h"
#include "components/comp_slider.h"
#include "components/comp_combo.h"
#include "components/comp_button.h"
#include "components/comp_checkbox.h"
#include "components/comp_colorpicker.h"
#include "components/comp_keybind.h"
#include "components/comp_label.h"
#include "components/comp_section.h"
#include "components/comp_separator.h"
#include "components/comp_row.h"
#include "components/comp_title.h"
#include "components/comp_tooltip.h"
#include "components/comp_searchbar.h"
#include "components/comp_card.h"
#include "components/comp_scroll.h"
#include "components/comp_window.h"

// Animation
#include "animation/anim_core.h"
#include "animation/anim_transition.h"
#include "animation/anim_text.h"
#include "animation/anim_micro.h"
#include "animation/anim_ink.h"
#include "animation/anim_particle.h"
#include "animation/anim_presets.h"

// Theme
#include "theme/theme.h"
#include "theme/theme_presets.h"

// Features
#include "features/feature_registry.h"
#include "features/feature_page.h"

// ESP drawing system (game-agnostic)
#include "esp/esp.h"

// Masked config persistence (used by esp_serialize)
#include "cfg/cfg.h"

// Utilities
#include "utils/opal_math.h"
#include "utils/opal_rand.h"
#include "utils/opal_string.h"
#include "utils/opal_debug_log.h"
