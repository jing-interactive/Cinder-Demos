#pragma once

#include "cinder/vk/vk.h"

namespace cinder {
    namespace vk {
        struct ScopedDepthWrite
        {
            ScopedDepthWrite(bool newDepthWrite)
            {
                this->newDepthWrite = newDepthWrite;
                bool oldDepthWrite = vk::context()->getDepthWrite();
                if (oldDepthWrite != newDepthWrite)
                {
                    vk::enableDepthWrite(newDepthWrite);
                }
            }

            ~ScopedDepthWrite()
            {
                if (oldDepthWrite != newDepthWrite)
                {
                    vk::enableDepthWrite(oldDepthWrite);
                }
            }

            bool oldDepthWrite;
            bool newDepthWrite;
        };
    }
}

namespace gfx = vk;

#include "SanctumCathedral.inl"
