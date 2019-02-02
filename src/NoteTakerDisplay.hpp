#pragma once

#include "SchmickleWorks.hpp"

struct NoteTaker;

struct NoteTakerDisplay : TransparentWidget {
    void draw(NVGcontext *vg) override;

    NoteTaker* module;
};
