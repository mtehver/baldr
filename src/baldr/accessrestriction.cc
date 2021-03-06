#include <string.h>
#include "baldr/accessrestriction.h"

namespace valhalla {
namespace baldr {

// Constructor with arguments
AccessRestriction::AccessRestriction(const uint32_t edgeindex,
                                     const AccessType type,
                                     const uint32_t modes,
                                     const uint32_t dow,
                                     const uint64_t value)
  : edgeindex_(edgeindex),
    type_(static_cast<uint32_t>(type)),
    modes_(modes),
    days_of_week_(dow),
    value_(value) {
}

// Get the internal edge Id.
uint32_t AccessRestriction::edgeindex() const {
  return edgeindex_;
}

// Set the internal edge index to which this access restriction applies.
void AccessRestriction::set_edgeindex(const uint32_t edgeindex) {
  edgeindex_ = edgeindex;
}

// Get the type
AccessType AccessRestriction::type() const {
  return static_cast<AccessType>(type_);
}

// Get the modes impacted by access restriction.
uint32_t AccessRestriction::modes() const {
  return modes_;
}

// Gets the days of the week for this access restriction.
uint32_t AccessRestriction::days_of_week() const {
  return days_of_week_;
}

// Get the value
uint64_t AccessRestriction::value() const {
  return value_;
}

// Set the value
void AccessRestriction::set_value(const uint64_t v) {
  value_ = v;
}

// operator < - for sorting. Sort by route Id.
bool AccessRestriction::operator < (const AccessRestriction& other) const {
  return edgeindex() < other.edgeindex();
}

}
}
