#pragma once
#include "AudioTools.h"

class OffsetFilter : public Filter<int32_t> {
  public:
    OffsetFilter(int32_t offsetVal = 223031000) : offset(offsetVal) {}
    virtual ~OffsetFilter() = default;

    virtual int32_t process(int32_t in) override {
    return in + offset;  // Orginal Filter
    //return in >> 8;   // Did not work
    }


protected:
  int32_t offset;
};
