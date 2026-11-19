//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_CONSTANTS_H
#define BIRD_CONSTANTS_H

/**
 * Marks a C++ class, method, field, or free function as excluded from
 * automatic script binding generation (DartApiGenerator).
 */
#ifdef BIRD_CODEGEN
#define BIRD_NO_EXPORT __attribute__((annotate("bird_no_export")))
#else
#define BIRD_NO_EXPORT
#endif

namespace bird {

/** Global engine version and configuration constants. */
constexpr const char* kEngineVersion = "0.1.0";

}  // namespace bird

#endif //BIRD_CONSTANTS_H
