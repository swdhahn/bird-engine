#ifndef BIRD_CLOCK_H
#define BIRD_CLOCK_H

namespace bird::platform {

/**
 * @return Seconds on a monotonic clock that never jumps with wall time;
 *         only differences between two readings are meaningful.
 */
double now();

}  // namespace bird::platform

#endif	// BIRD_CLOCK_H
