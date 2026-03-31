#include "effects/EffectEngine.h"

EffectEngine::EffectEngine(CoreState &state, LedDriver &driver) : state_(state), driver_(driver) {}

void EffectEngine::begin() {
  driver_.begin();
}

void EffectEngine::renderFrame() {
  if (!state_.power) {
    driver_.setAll(0);
    driver_.show();
    return;
  }

  driver_.setAll(state_.brightness);
  driver_.show();
}
